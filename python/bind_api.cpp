#include <nanobind/nanobind.h>
#include <nanobind/stl/optional.h>
#include <nanobind/stl/shared_ptr.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/unique_ptr.h>
#include <sc-api/api.h>
#include <sc-api/api_core.h>
#include <sc-api/command.h>
#include <sc-api/device.h>
#include <sc-api/device_info.h>
#include <sc-api/events.h>
#include <sc-api/led_control.h>
#include <sc-api/session.h>
#include <sc-api/sim_data.h>
#include <sc-api/sim_data_builder.h>
#include <sc-api/telemetry.h>
#include <sc-api/util/event_queue.h>
#include <sc-api/variables.h>
#include <sc-api/version.h>

#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>
#include <cmath>
#include <variant>
#include <vector>

#include "bind_bson.h"
#include "bind_exceptions.h"
#include "bind_sim_data.h"
#include "bind_util.h"

namespace nb = nanobind;

using sc_api::Api;
using sc_api::ApiUserInformation;
using sc_api::BackendVersion;
using sc_api::DeviceSessionId;
using sc_api::Event;
using sc_api::NoAuthControlEnabler;
using sc_api::NoEvent;
using sc_api::ResultCode;
using sc_api::RgbColor;
using sc_api::Session;
using sc_api::SessionState;
using sc_api::event::DeviceInfoChanged;
using sc_api::event::SessionStateChanged;
using sc_api::event::SimDataChanged;
using sc_api::event::TelemetryDefinitionsChanged;
using sc_api::event::VariableDefinitionsChanged;

using EventQueueT = sc_api::util::EventQueue<Event>;

// --- Variant unwrapping helper ---

static nb::object unwrap_event(const Event& event) {
    return std::visit(
        [](auto&& e) -> nb::object {
            using T = std::decay_t<decltype(e)>;
            if constexpr (std::is_same_v<T, NoEvent>) {
                return nb::none();
            } else {
                return nb::cast(e);
            }
        },
        event);
}

// --- Interruptible waiting ---

/** Longest single wait with the GIL released.
 *
 * Short enough that Ctrl-C and interpreter shutdown are handled promptly, long enough that the
 * wakeup cost stays negligible.
 */
constexpr auto poll_interval = std::chrono::milliseconds(50);

/** Wait for an event, releasing the GIL only in short slices.
 *
 * A single unbounded wait with the GIL released cannot be interrupted: signals are never handled,
 * and a thread parked in one when the interpreter shuts down never unwinds its frame, which
 * strands every Python object the frame holds.
 *
 * @param deadline Absolute time to give up at, or std::nullopt to wait until an event arrives or
 *                 the queue closes.
 * @return The event, or std::nullopt if the deadline passed or the queue closed and drained.
 * @throws nb::python_error If a signal handler raised, for example on Ctrl-C.
 */
static std::optional<Event> waitForEvent(EventQueueT&                                         queue,
                                         std::optional<std::chrono::steady_clock::time_point> deadline) {
    while (true) {
        auto slice_end = std::chrono::steady_clock::now() + poll_interval;
        bool last      = false;
        if (deadline && *deadline <= slice_end) {
            slice_end = *deadline;
            last      = true;
        }

        std::optional<Event> event;
        {
            nb::gil_scoped_release release;
            event = queue.tryPopUntil(slice_end);
        }

        if (event) return event;
        // Closed: no further push can happen, but one may have landed just before the close.
        if (!queue.isOpen()) return queue.tryPop();
        if (PyErr_CheckSignals() != 0) throw nb::python_error();
        if (last) return std::nullopt;
    }
}

/** Largest timeout in seconds that toDeadline can convert.
 *
 * Half of the clock's own range, which leaves room for the addition to the current time.
 */
constexpr double max_timeout_seconds =
    std::chrono::duration_cast<std::chrono::duration<double>>(std::chrono::steady_clock::duration::max()).count() / 2.0;

/** Reject a timeout that toDeadline cannot convert.
 *
 * The conversion casts to the integer representation of the clock, which is undefined for a value
 * out of range, and for infinity and NaN.
 *
 * @throws nb::python_error If the timeout is NaN (ValueError) or too large (OverflowError).
 */
static void checkTimeout(std::optional<double> timeout) {
    if (!timeout) return;
    if (std::isnan(*timeout)) {
        PyErr_SetString(PyExc_ValueError, "timeout must be a number, not NaN");
        throw nb::python_error();
    }
    if (!(*timeout < max_timeout_seconds)) {  // Also catches positive infinity.
        PyErr_SetString(PyExc_OverflowError, "timeout is too large; pass None to wait without a limit");
        throw nb::python_error();
    }
}

/** Convert a timeout in seconds to an absolute deadline. std::nullopt means "wait forever".
 *
 * A negative timeout gives a deadline that has already passed.
 *
 * @throws nb::python_error If the timeout is NaN (ValueError) or too large (OverflowError).
 */
static std::optional<std::chrono::steady_clock::time_point> toDeadline(std::optional<double> timeout) {
    checkTimeout(timeout);
    if (!timeout) return std::nullopt;
    auto seconds = std::chrono::duration<double>(*timeout > 0.0 ? *timeout : 0.0);
    return std::chrono::steady_clock::now() + std::chrono::duration_cast<std::chrono::steady_clock::duration>(seconds);
}

/** Result slot of one Session.command call, shared with the worker thread that runs it. */
struct PendingCommand {
    std::mutex              mutex;
    std::condition_variable cv;
    bool                    done = false;
    sc_api::CommandResult   result;

    void complete(sc_api::CommandResult r) {
        {
            std::lock_guard lock(mutex);
            result = std::move(r);
            done   = true;
        }
        cv.notify_all();
    }

    /** Waits for the result until `until`; true once it has arrived. */
    bool waitUntil(std::chrono::steady_clock::time_point until) {
        std::unique_lock lock(mutex);
        return cv.wait_until(lock, until, [this] { return done; });
    }
};

// --- PyApi wrapper (stores NoAuthControlEnabler alongside Api) ---

struct PyApi {
    std::unique_ptr<Api>                  impl;
    std::unique_ptr<NoAuthControlEnabler> control_enabler;

    PyApi() : impl(std::make_unique<Api>()) {}

    PyApi(Session::ControlFlag flags, std::string id_name, ApiUserInformation user_info)
        : impl(std::make_unique<Api>()) {
        control_enabler = std::make_unique<NoAuthControlEnabler>(impl.get(), static_cast<uint32_t>(flags),
                                                                 std::move(id_name), std::move(user_info));
    }

    ~PyApi() {
        control_enabler.reset();
        if (impl) {
            nb::gil_scoped_release release;
            impl.reset();
        }
    }

    void close() {
        control_enabler.reset();
        if (impl) {
            nb::gil_scoped_release release;
            impl.reset();
        }
    }

    Api& api() const {
        if (!impl) {
            throw nb::value_error("Api has been closed");
        }
        return *impl;
    }
};

// --- Event iterator for Api.events() ---

struct EventIterator {
    std::unique_ptr<EventQueueT> queue;
    std::optional<double>        timeout;
};

// FfbPipeline and LedControl store a shared_ptr<Session> that came from Python. nanobind must
// resolve it to the control block the session manager already owns, not build a second one.
static_assert(bind_util::has_shared_from_this_v<Session>, "Session must inherit std::enable_shared_from_this publicly");

void bind_api(nb::module_& m) {
    // --- Value types (bound first so they can be used in constructor signatures) ---

    nb::class_<DeviceSessionId>(m, "DeviceSessionId",
                                "Identifies a device within a single session.\n\n"
                                "The id is assigned by Tuner and may change between sessions. "
                                "Use the device UID string for persistent identification.")
        .def(
            "__init__", [](DeviceSessionId* self, uint16_t id) { new (self) DeviceSessionId{id}; }, nb::arg("id") = 0,
            "Construct a DeviceSessionId.\n\n"
            ":param id: Raw 16-bit session-scoped device identifier. Defaults to 0 (invalid).")
        .def_prop_ro(
            "id", [](const DeviceSessionId& d) { return d.id; },
            "Raw 16-bit device identifier for the current session.")
        .def(
            "__eq__", [](const DeviceSessionId& a, const DeviceSessionId& b) { return a == b; }, nb::is_operator())
        .def(
            "__lt__", [](const DeviceSessionId& a, const DeviceSessionId& b) { return a < b; }, nb::is_operator())
        .def("__hash__", [](const DeviceSessionId& d) { return std::hash<uint16_t>()(d.id); })
        .def("__bool__", [](const DeviceSessionId& d) { return static_cast<bool>(d); })
        .def("__int__", [](const DeviceSessionId& d) { return d.id; })
        .def("__repr__", [](const DeviceSessionId& d) { return "DeviceSessionId(" + std::to_string(d.id) + ")"; });

    nb::class_<BackendVersion>(m, "BackendVersion", "Version of Simucube Tuner, in the form major.minor.patch.")
        .def(
            "__init__",
            [](BackendVersion* self, uint16_t major, uint16_t minor, uint16_t patch) {
                new (self) BackendVersion{major, minor, patch};
            },
            nb::arg("major") = 0, nb::arg("minor") = 0, nb::arg("patch") = 0,
            "Construct a BackendVersion. Each part is 0-65535. 0.0.0 means that Tuner does not report its version.")
        .def_ro("major", &BackendVersion::major)
        .def_ro("minor", &BackendVersion::minor)
        .def_ro("patch", &BackendVersion::patch)
        .def(
            "__eq__", [](const BackendVersion& a, const BackendVersion& b) { return a == b; }, nb::is_operator())
        .def(
            "__lt__", [](const BackendVersion& a, const BackendVersion& b) { return a < b; }, nb::is_operator())
        .def(
            "__le__", [](const BackendVersion& a, const BackendVersion& b) { return a <= b; }, nb::is_operator())
        .def(
            "__gt__", [](const BackendVersion& a, const BackendVersion& b) { return a > b; }, nb::is_operator())
        .def(
            "__ge__", [](const BackendVersion& a, const BackendVersion& b) { return a >= b; }, nb::is_operator())
        .def("__hash__",
             [](const BackendVersion& v) {
                 return std::hash<uint64_t>()((uint64_t{v.major} << 32) | (uint64_t{v.minor} << 16) | v.patch);
             })
        .def("__str__",
             [](const BackendVersion& v) {
                 return std::to_string(v.major) + "." + std::to_string(v.minor) + "." + std::to_string(v.patch);
             })
        .def("__repr__", [](const BackendVersion& v) {
            return "BackendVersion(" + std::to_string(v.major) + ", " + std::to_string(v.minor) + ", " +
                   std::to_string(v.patch) + ")";
        });

    m.attr("MIN_BACKEND_VERSION") = sc_api::k_min_backend_version;

    nb::class_<ApiUserInformation>(m, "ApiUserInformation", "Metadata about the API consumer shown in Simucube Tuner.")
        .def(
            "__init__",
            [](ApiUserInformation* self, std::string display_name, std::string type, std::string path,
               std::string author, std::string version_string) {
                new (self) ApiUserInformation{std::move(display_name), std::move(type), std::move(path),
                                              std::move(author), std::move(version_string)};
            },
            nb::arg("display_name") = "", nb::arg("type") = "", nb::arg("path") = "", nb::arg("author") = "",
            nb::arg("version_string") = "",
            "Construct ApiUserInformation.\n\n"
            ":param display_name: Human-readable name shown in Tuner.\n"
            ":param type: Application type identifier (e.g. ``\"game\"``, ``\"tool\"``).\n"
            ":param path: Path or URI that identifies the application.\n"
            ":param author: Author or organisation name.\n"
            ":param version_string: Application version (free-form string).")
        .def_rw("display_name", &ApiUserInformation::display_name, "Human-readable name shown in Simucube Tuner.")
        .def_rw("type", &ApiUserInformation::type, "Application type identifier (e.g. ``\"game\"`` or ``\"tool\"``).")
        .def_rw("path", &ApiUserInformation::path, "Path or URI that identifies the application.")
        .def_rw("author", &ApiUserInformation::author, "Author or organisation name.")
        .def_rw("version_string", &ApiUserInformation::version_string, "Application version as a free-form string.")
        .def("__repr__", [](const ApiUserInformation& self) {
            return "ApiUserInformation(display_name='" + self.display_name + "')";
        });

    nb::class_<RgbColor>(m, "RgbColor", "24-bit RGB colour used for device LED control.")
        .def(
            "__init__", [](RgbColor* self, uint8_t r, uint8_t g, uint8_t b) { new (self) RgbColor(r, g, b); },
            nb::arg("r") = 0, nb::arg("g") = 0, nb::arg("b") = 0,
            "Construct an RgbColor from 8-bit channel values (0-255).")
        .def_prop_ro(
            "r", [](const RgbColor& c) { return c.r; }, "Red channel (0-255).")
        .def_prop_ro(
            "g", [](const RgbColor& c) { return c.g; }, "Green channel (0-255).")
        .def_prop_ro(
            "b", [](const RgbColor& c) { return c.b; }, "Blue channel (0-255).")
        .def(
            "__eq__", [](const RgbColor& a, const RgbColor& b) { return a == b; }, nb::is_operator())
        .def("__repr__", [](const RgbColor& c) {
            return "RgbColor(" + std::to_string(c.r) + ", " + std::to_string(c.g) + ", " + std::to_string(c.b) + ")";
        });

    // --- Session ---

    nb::class_<Session>(m, "Session",
                        "An active connection to Simucube Tuner.\n\n"
                        "Obtained via ``Api.wait_for_session()`` or a ``SessionStateChanged`` "
                        "event. States progress from ``connected_monitor`` (read-only) to "
                        "``connected_control`` (full access) after a successful "
                        "``register_to_control()`` call.")
        .def_prop_ro("state", &Session::getState, "Current session state (``SessionState`` enum).")
        .def_prop_ro("backend_version", &Session::getBackendVersion,
                     "Version of the Tuner that opened this session; 0.0.0 if Tuner does not report its version.")
        .def_prop_ro("controller_id", &Session::getControllerId, "Numeric ID assigned to this controller by Tuner.")
        .def_prop_ro(
            "control_flags",
            [](const Session& self) { return static_cast<Session::ControlFlag>(self.getControlFlags()); },
            "Bitmask of ``ControlFlag`` values granted to this session.")
        .def(
            "register_to_control",
            [](Session& self, Session::ControlFlag flags, const std::string& id_name,
               const ApiUserInformation& user_info) {
                ResultCode rc;
                {
                    nb::gil_scoped_release release;
                    rc = self.registerToControl(static_cast<uint32_t>(flags), id_name, user_info);
                }
                throw_on_error(rc);
            },
            nb::arg("flags"), nb::arg("id_name"), nb::arg("user_info"),
            "Request control access from Tuner (blocks until Tuner responds).\n\n"
            "Transitions the session from ``connected_monitor`` to "
            "``connected_control`` on success.\n\n"
            ":param flags: ``ControlFlag`` bitmask of capabilities to request.\n"
            ":param id_name: Unique string identifier for this controller.\n"
            ":param user_info: Metadata about the application shown in Tuner.\n\n"
            ":raises SimucubeError: If Tuner rejects the request or the session is in an invalid state.")
        .def(
            "close",
            [](Session& self) {
                ResultCode rc;
                {
                    nb::gil_scoped_release release;
                    rc = self.close();
                }
                throw_on_error(rc);
            },
            "Close the session and release all associated resources.\n\n"
            ":raises SimucubeError: If the underlying close operation fails.")
        .def_prop_ro(
            "device_info",
            [](Session& self) -> nb::object {
                auto info = self.getDeviceInfo();
                if (!info) return nb::none();
                return nb::cast(info);
            },
            "Current device hierarchy snapshot, or ``None`` if not yet available.")
        .def_prop_ro(
            "variables", [](Session& self) { return self.getVariables(); },
            "Variable definitions available for reading in this session.")
        .def_prop_ro(
            "telemetries", [](Session& self) { return self.getTelemetries(); },
            "Telemetry channel definitions available for sending in this session.")
        .def_prop_ro(
            "sim_data",
            [](Session& self) -> nb::object {
                auto sd = self.getSimData();
                if (!sd) return nb::none();
                return nb::cast(sd);
            },
            "Current simulator state data snapshot, or ``None`` if not set.")
        .def(
            "update_sim_data",
            [](Session& self, const nb::dict& data, std::optional<std::string> sim_id, bool activate) {
                std::string resolved_sim_id;
                if (sim_id) {
                    resolved_sim_id = std::move(*sim_id);
                } else {
                    if (data.contains("sim")) {
                        nb::dict sim_dict = nb::cast<nb::dict>(data["sim"]);
                        if (sim_dict.contains("id")) {
                            resolved_sim_id = nb::cast<std::string>(sim_dict["id"]);
                        }
                    }
                    if (resolved_sim_id.empty()) {
                        throw nb::value_error(
                            "sim_id must be provided either as a parameter "
                            "or as data['sim']['id']");
                    }
                }
                sc_api::sim_data::SimDataUpdateBuilder builder(resolved_sim_id, activate);
                populate_sim_data_builder(builder, data);
                bool ok;
                {
                    nb::gil_scoped_release release;
                    ok = self.blockingUpdateSimData(builder);
                }
                if (!ok) {
                    throw nb::value_error("Failed to update sim data");
                }
            },
            nb::arg("data"), nb::arg("sim_id") = nb::none(), nb::arg("activate") = true,
            "Merge simulator state data into the existing sim data record (blocks).\n\n"
            "Only the fields present in ``data`` are updated; all other fields "
            "retain their previous values.\n\n"
            ":param data: Dict of sim data fields to update. If ``sim_id`` is omitted,\n"
            "    ``data['sim']['id']`` is used as the simulator identifier.\n"
            ":param sim_id: Explicit simulator identifier. Overrides ``data['sim']['id']``.\n"
            ":param activate: If ``True``, mark this simulator as the active one in Tuner.\n\n"
            ":raises ValueError: If no sim_id can be resolved or the update fails.")
        .def(
            "replace_sim_data",
            [](Session& self, const nb::dict& data, std::optional<std::string> sim_id, bool activate) {
                std::string resolved_sim_id;
                if (sim_id) {
                    resolved_sim_id = std::move(*sim_id);
                } else {
                    if (data.contains("sim")) {
                        nb::dict sim_dict = nb::cast<nb::dict>(data["sim"]);
                        if (sim_dict.contains("id")) {
                            resolved_sim_id = nb::cast<std::string>(sim_dict["id"]);
                        }
                    }
                    if (resolved_sim_id.empty()) {
                        throw nb::value_error(
                            "sim_id must be provided either as a parameter "
                            "or as data['sim']['id']");
                    }
                }
                sc_api::sim_data::SimDataUpdateBuilder builder(resolved_sim_id, activate);
                populate_sim_data_builder(builder, data);
                bool ok;
                {
                    nb::gil_scoped_release release;
                    ok = self.blockingReplaceSimData(builder);
                }
                if (!ok) {
                    throw nb::value_error("Failed to replace sim data");
                }
            },
            nb::arg("data"), nb::arg("sim_id") = nb::none(), nb::arg("activate") = true,
            "Replace the entire simulator state data record (blocks).\n\n"
            "All existing sim data fields are discarded and replaced with the "
            "contents of ``data``. Use ``update_sim_data`` to do a partial update.\n\n"
            ":param data: Dict of sim data fields. If ``sim_id`` is omitted,\n"
            "    ``data['sim']['id']`` is used as the simulator identifier.\n"
            ":param sim_id: Explicit simulator identifier. Overrides ``data['sim']['id']``.\n"
            ":param activate: If ``True``, mark this simulator as the active one in Tuner.\n\n"
            ":raises ValueError: If no sim_id can be resolved or the replace fails.")
        .def(
            "command",
            [](Session& self, const std::string& service, const std::string& command, std::optional<nb::dict> args,
               double timeout_s) {
                if (service.find('\0') != std::string::npos || command.find('\0') != std::string::npos) {
                    throw nb::value_error("service and command must not contain a null character");
                }
                if (!(timeout_s > 0.0)) {  // Also catches NaN.
                    throw nb::value_error("timeout_s must be a positive number");
                }
                if (!(timeout_s < max_timeout_seconds)) {  // Also catches positive infinity.
                    PyErr_SetString(PyExc_OverflowError, "timeout_s is too large");
                    throw nb::python_error();
                }
                const auto timeout =
                    std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::duration<double>(timeout_s));
                std::vector<uint8_t> body = bind_bson::encode(args ? *args : nb::dict());

                // blockingCommand runs on a worker so Ctrl-C can interrupt the wait below. It ends by its own timeout,
                // which also frees the pending slot, so an interrupted call holds the slot at most timeout_s.
                auto pending = std::make_shared<PendingCommand>();
                std::thread([session = self.shared_from_this(), pending, service, command, body = std::move(body),
                             timeout] {
                    sc_api::CommandRequest req;
                    req.initializeFrom(service, command, body.data());
                    pending->complete(session->blockingCommand(std::move(req), timeout));
                }).detach();

                while (true) {
                    bool done;
                    {
                        nb::gil_scoped_release release;
                        done = pending->waitUntil(std::chrono::steady_clock::now() + poll_interval);
                    }
                    if (done) break;
                    if (PyErr_CheckSignals() != 0) throw nb::python_error();
                }

                if (!pending->result.isSuccess()) {
                    throw_on_error(pending->result.getResultCode(), pending->result.getErrorMessage());
                }
                const std::vector<uint8_t>& payload = pending->result.getPayload();
                if (payload.empty()) return nb::dict();
                return bind_bson::decode(payload.data(), payload.size());
            },
            nb::arg("service"), nb::arg("command"), nb::arg("args") = nb::none(), nb::arg("timeout_s") = 1.0,
            "Send a command to a Tuner service and wait for its reply (blocks).\n\n"
            "A generic call for commands that have no dedicated method. Needs a session that is registered with\n"
            "``register_to_control()``. ``args`` and the reply are dicts of bool, int, float, str, bytes, None,\n"
            "dict and list values.\n\n"
            ":param service: Id name of the service that handles the command, such as ``\"core\"``.\n"
            ":param command: Command id within that service.\n"
            ":param args: Command arguments; an empty document when omitted.\n"
            ":param timeout_s: Seconds to wait for the reply.\n"
            ":returns: The reply data, or an empty dict when the reply has none.\n\n"
            ":raises TypeError: If ``args`` holds a value that cannot be sent.\n"
            ":raises TimeoutError: If no reply arrives within ``timeout_s``.\n"
            ":raises OverflowError: If ``timeout_s`` is too large.\n"
            ":raises SimucubeError: If Tuner rejects the command; the message carries Tuner's error message.")
        .def("__repr__", [](const Session& self) {
            return std::string("<Session state=") + session_state_str(self.getState()) +
                   " controller_id=" + std::to_string(self.getControllerId()) + ">";
        });

    // --- EventQueue ---

    nb::class_<EventQueueT>(m, "EventQueue",
                            "Thread-safe queue that delivers API events to the caller.\n\n"
                            "Obtained via ``Api.create_event_queue()``. Each queue receives "
                            "its own independent copy of every event. Close the queue when "
                            "done to free resources.")
        .def(
            "pop",
            [](EventQueueT& self, std::optional<double> timeout) -> nb::object {
                auto event = waitForEvent(self, toDeadline(timeout));
                if (!event) return nb::none();
                return unwrap_event(*event);
            },
            nb::arg("timeout") = nb::none(),
            "Remove and return the next event, optionally waiting up to ``timeout`` seconds.\n\n"
            "Without a timeout, blocks indefinitely until an event is available or the "
            "queue closes. If the queue closes while waiting, returns ``None``. "
            "With a timeout, returns ``None`` if the deadline expires before "
            "an event arrives (the queue remains open).\n\n"
            ":param timeout: Maximum seconds to wait. ``None`` (default) waits forever.\n\n"
            ":returns: An event object, or ``None`` on timeout or queue closure.\n\n"
            ":raises ValueError: If ``timeout`` is NaN.\n"
            ":raises OverflowError: If ``timeout`` is too large. Pass ``None`` to wait forever.")
        .def(
            "try_pop",
            [](EventQueueT& self) -> nb::object {
                auto event = self.tryPop();
                if (!event) return nb::none();
                return unwrap_event(*event);
            },
            "Remove and return the next event without blocking.\n\n"
            ":returns: An event object, or ``None`` if the queue is currently empty.")
        .def("close", &EventQueueT::close, "Close the queue, unblocking any threads waiting in ``pop()``.")
        .def("__repr__", [](const EventQueueT&) { return "<EventQueue>"; });

    // --- Event iterator ---

    nb::class_<EventIterator>(m, "_EventIterator")
        .def(
            "__iter__", [](EventIterator& self) -> EventIterator& { return self; }, nb::rv_policy::none)
        .def(
            "close", [](EventIterator& self) { self.queue->close(); },
            "Close the underlying queue, ending the iteration.\n\n"
            "Releases any thread waiting in the iterator. Call this, and join the thread, before "
            "the interpreter shuts down.")
        .def(
            "__enter__", [](EventIterator& self) -> EventIterator& { return self; }, nb::rv_policy::none)
        .def("__exit__",
             [](EventIterator& self, nb::args /*unused*/) {  // NOLINT(performance-unnecessary-value-param)
                 self.queue->close();
             })
        .def("__next__", [](EventIterator& self) -> nb::object {
            auto event = waitForEvent(*self.queue, toDeadline(self.timeout));
            if (!event && !self.queue->isOpen()) {
                // Closed: drain an event that a push landed just before the close.
                event = self.queue->tryPop();
                if (!event) throw nb::stop_iteration();
            }
            // No event within the per-item timeout. The queue is still open.
            if (!event) return nb::none();
            if (std::holds_alternative<NoEvent>(*event)) throw nb::stop_iteration();
            return unwrap_event(*event);
        });

    // --- Api ---

    nb::class_<PyApi>(m, "Api",
                      "Entry point for the Simucube API.\n\n"
                      "Manages a background thread that connects to Simucube Tuner via IPC "
                      "and maintains the current session. Use as a context manager to ensure "
                      "clean shutdown, or call ``close()`` explicitly.\n\n"
                      "The monitor-only constructor connects without requesting device control. "
                      "The control constructor additionally registers a ``NoAuthControlEnabler``.")
        .def(nb::init<>(), "Create an Api instance in monitor-only mode (no device control).")
        .def(nb::init<Session::ControlFlag, std::string, ApiUserInformation>(), nb::arg("control_flags"),
             nb::arg("id_name"), nb::arg("user_info"),
             "Create an Api instance that automatically requests device control.\n\n"
             "Uses ``NoAuthControlEnabler``.\n\n"
             ":param control_flags: ``ControlFlag`` bitmask of capabilities to request.\n"
             ":param id_name: Unique string identifier for this controller.\n"
             ":param user_info: Metadata about the application shown in Tuner.")
        .def(
            "__enter__", [](PyApi& self) -> PyApi& { return self; }, nb::rv_policy::none)
        .def("__exit__",
             [](PyApi& self, nb::args /*unused*/) { self.close(); })  // NOLINT(performance-unnecessary-value-param)
        .def_prop_ro(
            "session",
            [](PyApi& self) -> nb::object {
                auto session = self.api().getSession();
                if (!session) return nb::none();
                return nb::cast(session);
            },
            "The current session, or ``None`` if not yet connected to Tuner.")
        .def(
            "create_event_queue", [](PyApi& self) { return self.api().createEventQueue(); },
            "Create a new ``EventQueue`` that receives all subsequent API events.\n\n"
            ":returns: A new ``EventQueue`` instance. Each queue is independent.")
        .def(
            "wait_for_session",
            [](PyApi& self, std::optional<double> timeout) -> std::shared_ptr<Session> {
                auto deadline = toDeadline(timeout);
                auto queue    = self.api().createEventQueue();

                while (true) {
                    auto event = waitForEvent(*queue, deadline);
                    if (!event) {
                        if (queue->isOpen()) {
                            PyErr_SetString(PyExc_TimeoutError, "Timed out waiting for session");
                        } else {
                            PyErr_SetString(PyExc_RuntimeError, "Api was closed while waiting for a session");
                        }
                        throw nb::python_error();
                    }

                    auto* sse = std::get_if<SessionStateChanged>(&*event);
                    if (sse && sse->session) {
                        return sse->session;
                    }
                }
            },
            nb::arg("timeout") = nb::none(),
            "Block until a session is established with Tuner, then return it.\n\n"
            "Internally creates a temporary event queue and waits for a "
            "``SessionStateChanged`` event carrying a live session.\n\n"
            ":param timeout: Maximum seconds to wait. ``None`` (default) waits forever.\n\n"
            ":returns: The connected ``Session`` object.\n\n"
            ":raises TimeoutError: If no session is established within ``timeout`` seconds.\n"
            ":raises RuntimeError: If another thread closes the Api while this call waits.\n"
            ":raises ValueError: If ``timeout`` is NaN.\n"
            ":raises OverflowError: If ``timeout`` is too large. Pass ``None`` to wait forever.")
        .def(
            "events",
            [](PyApi& self, std::optional<double> timeout) -> EventIterator {
                // Report an unusable timeout here rather than on the first iteration.
                checkTimeout(timeout);
                auto queue = self.api().createEventQueue();
                return EventIterator{std::move(queue), timeout};
            },
            nb::arg("timeout") = nb::none(),
            "Return an iterator that yields API events as they arrive.\n\n"
            "With no timeout, the iterator blocks until the next event and raises "
            "``StopIteration`` when the queue closes. With a timeout, each iteration "
            "waits at most ``timeout`` seconds; if no event arrives, ``None`` is "
            "yielded and iteration continues (the queue is still open).\n\n"
            "The iterator owns its queue. Call ``close()`` on it, or use it as a "
            "context manager, to end the iteration from another thread.\n\n"
            ":param timeout: Per-item wait limit in seconds. ``None`` waits forever.\n\n"
            ":returns: An iterator of event objects or ``None`` (on per-item timeout).\n\n"
            ":raises ValueError: If ``timeout`` is NaN.\n"
            ":raises OverflowError: If ``timeout`` is too large. Pass ``None`` to wait forever.")
        .def_static("get_backend_version", &sc_api::ApiCore::getBackendVersion,
                    "Version of the running Tuner. Needs no session. Returns at once and is safe to call from any "
                    "thread. The value is what Tuner reports, not proof that it still runs.\n\n"
                    ":returns: The version; 0.0.0 if Tuner does not report its version. ``None`` if no version is "
                    "available now, for example when Tuner does not run or is starting. Check for ``None`` before "
                    "you compare the result with ``MIN_BACKEND_VERSION``.")
        .def("close", &PyApi::close,
             "Shut down the background thread and release all resources.\n\n"
             "Safe to call multiple times. Also called automatically by the context manager.")
        .def("__repr__", [](PyApi&) { return "<Api>"; });
}
