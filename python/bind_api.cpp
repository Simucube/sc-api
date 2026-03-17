#include "bind_exceptions.h"
#include "bind_util.h"

#include <nanobind/nanobind.h>
#include <nanobind/stl/optional.h>
#include <nanobind/stl/shared_ptr.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/unique_ptr.h>

#include <chrono>
#include <variant>

#include <sc-api/core/api.h>
#include <sc-api/core/device.h>
#include <sc-api/core/device_info.h>
#include <sc-api/core/events.h>
#include <sc-api/core/led_control.h>
#include <sc-api/core/session.h>
#include <sc-api/core/util/event_queue.h>
#include <sc-api/core/telemetry.h>
#include <sc-api/core/variables.h>

namespace nb = nanobind;

using sc_api::ResultCode;
using sc_api::core::Api;
using sc_api::core::ApiUserInformation;
using sc_api::core::DeviceSessionId;
using sc_api::core::Event;
using sc_api::core::NoAuthControlEnabler;
using sc_api::core::NoEvent;
using sc_api::core::RgbColor;
using sc_api::core::Session;
using sc_api::core::SessionState;
using sc_api::core::session_event::DeviceInfoChanged;
using sc_api::core::session_event::SessionStateChanged;
using sc_api::core::session_event::SimDataChanged;
using sc_api::core::session_event::TelemetryDefinitionsChanged;
using sc_api::core::session_event::VariableDefinitionsChanged;

using EventQueueT = sc_api::core::util::EventQueue<Event>;

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

// --- PyApi wrapper (stores NoAuthControlEnabler alongside Api) ---

struct PyApi {
    std::unique_ptr<Api> impl;
    std::unique_ptr<NoAuthControlEnabler> control_enabler;

    PyApi() : impl(std::make_unique<Api>()) {}

    PyApi(Session::ControlFlag flags, std::string id_name,
          ApiUserInformation user_info)
        : impl(std::make_unique<Api>()) {
        control_enabler = std::make_unique<NoAuthControlEnabler>(
            impl.get(), static_cast<uint32_t>(flags), std::move(id_name),
            std::move(user_info));
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

    Api& api() {
        if (!impl) {
            throw nb::value_error("Api has been closed");
        }
        return *impl;
    }
};

// --- Event iterator for Api.events() ---

struct EventIterator {
    std::unique_ptr<EventQueueT> queue;
    std::optional<double> timeout;
};

void bind_api(nb::module_& m) {
    // --- Value types (bound first so they can be used in constructor signatures) ---

    nb::class_<DeviceSessionId>(m, "DeviceSessionId")
        .def("__init__",
             [](DeviceSessionId* self, uint16_t id) {
                 new (self) DeviceSessionId{id};
             },
             nb::arg("id") = 0)
        .def_prop_ro("id", [](const DeviceSessionId& d) { return d.id; })
        .def("__eq__",
             [](const DeviceSessionId& a, const DeviceSessionId& b) {
                 return a == b;
             })
        .def("__lt__",
             [](const DeviceSessionId& a, const DeviceSessionId& b) {
                 return a < b;
             })
        .def("__hash__",
             [](const DeviceSessionId& d) {
                 return std::hash<uint16_t>()(d.id);
             })
        .def("__bool__",
             [](const DeviceSessionId& d) { return static_cast<bool>(d); })
        .def("__int__", [](const DeviceSessionId& d) { return d.id; })
        .def("__repr__", [](const DeviceSessionId& d) {
            return "DeviceSessionId(" + std::to_string(d.id) + ")";
        });

    nb::class_<ApiUserInformation>(m, "ApiUserInformation")
        .def(
            "__init__",
            [](ApiUserInformation* self, std::string display_name,
               std::string type, std::string path, std::string author,
               std::string version_string) {
                new (self) ApiUserInformation{
                    std::move(display_name), std::move(type), std::move(path),
                    std::move(author), std::move(version_string)};
            },
            nb::arg("display_name") = "", nb::arg("type") = "",
            nb::arg("path") = "", nb::arg("author") = "",
            nb::arg("version_string") = "")
        .def_rw("display_name", &ApiUserInformation::display_name)
        .def_rw("type", &ApiUserInformation::type)
        .def_rw("path", &ApiUserInformation::path)
        .def_rw("author", &ApiUserInformation::author)
        .def_rw("version_string", &ApiUserInformation::version_string)
        .def("__repr__", [](const ApiUserInformation& self) {
            return "ApiUserInformation(display_name='" + self.display_name +
                   "')";
        });

    nb::class_<RgbColor>(m, "RgbColor")
        .def(
            "__init__",
            [](RgbColor* self, uint8_t r, uint8_t g, uint8_t b) {
                new (self) RgbColor(r, g, b);
            },
            nb::arg("r") = 0, nb::arg("g") = 0, nb::arg("b") = 0)
        .def_prop_ro("r", [](const RgbColor& c) { return c.r; })
        .def_prop_ro("g", [](const RgbColor& c) { return c.g; })
        .def_prop_ro("b", [](const RgbColor& c) { return c.b; })
        .def("__eq__",
             [](const RgbColor& a, const RgbColor& b) { return a == b; })
        .def("__repr__", [](const RgbColor& c) {
            return "RgbColor(" + std::to_string(c.r) + ", " +
                   std::to_string(c.g) + ", " + std::to_string(c.b) + ")";
        });

    // --- Session ---

    nb::class_<Session>(m, "Session")
        .def_prop_ro("state", &Session::getState)
        .def_prop_ro("controller_id", &Session::getControllerId)
        .def_prop_ro("control_flags", &Session::getControlFlags)
        .def(
            "register_to_control",
            [](Session& self, Session::ControlFlag flags,
               const std::string& id_name,
               const ApiUserInformation& user_info) {
                ResultCode rc;
                {
                    nb::gil_scoped_release release;
                    rc = self.registerToControl(
                        static_cast<uint32_t>(flags), id_name, user_info);
                }
                throw_on_error(rc);
            },
            nb::arg("flags"), nb::arg("id_name"), nb::arg("user_info"))
        .def(
            "close",
            [](Session& self) {
                ResultCode rc;
                {
                    nb::gil_scoped_release release;
                    rc = self.close();
                }
                throw_on_error(rc);
            })
        .def_prop_ro("device_info",
                      [](Session& self) -> nb::object {
                          auto info = self.getDeviceInfo();
                          if (!info) return nb::none();
                          return nb::cast(info);
                      })
        .def_prop_ro("variables",
                      [](Session& self) { return self.getVariables(); })
        .def_prop_ro("telemetries",
                      [](Session& self) { return self.getTelemetries(); })
        .def("__repr__", [](const Session& self) {
            return std::string("<Session state=") +
                   session_state_str(self.getState()) +
                   " controller_id=" +
                   std::to_string(self.getControllerId()) + ">";
        });

    // --- EventQueue ---

    nb::class_<EventQueueT>(m, "EventQueue")
        .def(
            "pop",
            [](EventQueueT& self,
               std::optional<double> timeout) -> nb::object {
                if (timeout) {
                    auto dur = std::chrono::duration<double>(*timeout);
                    std::optional<Event> event;
                    {
                        nb::gil_scoped_release release;
                        event = self.tryPopFor(dur);
                    }
                    if (!event) return nb::none();
                    return unwrap_event(*event);
                } else {
                    Event event;
                    {
                        nb::gil_scoped_release release;
                        event = self.pop();
                    }
                    return unwrap_event(event);
                }
            },
            nb::arg("timeout") = nb::none())
        .def(
            "try_pop",
            [](EventQueueT& self) -> nb::object {
                auto event = self.tryPop();
                if (!event) return nb::none();
                return unwrap_event(*event);
            })
        .def("close", &EventQueueT::close)
        .def("__repr__",
             [](const EventQueueT&) { return "<EventQueue>"; });

    // --- Event iterator ---

    nb::class_<EventIterator>(m, "_EventIterator")
        .def(
            "__iter__",
            [](EventIterator& self) -> EventIterator& { return self; },
            nb::rv_policy::none)
        .def("__next__", [](EventIterator& self) -> nb::object {
            if (self.timeout) {
                auto dur = std::chrono::duration<double>(*self.timeout);
                std::optional<Event> event;
                {
                    nb::gil_scoped_release release;
                    event = self.queue->tryPopFor(dur);
                }
                if (event) return unwrap_event(*event);
                if (!self.queue->isOpen()) throw nb::stop_iteration();
                return nb::none();
            } else {
                Event event;
                {
                    nb::gil_scoped_release release;
                    event = self.queue->pop();
                }
                if (std::holds_alternative<NoEvent>(event)) {
                    throw nb::stop_iteration();
                }
                return unwrap_event(event);
            }
        });

    // --- Api ---

    nb::class_<PyApi>(m, "Api")
        .def(nb::init<>())
        .def(nb::init<Session::ControlFlag, std::string, ApiUserInformation>(),
             nb::arg("control_flags"), nb::arg("id_name"),
             nb::arg("user_info"))
        .def(
            "__enter__",
            [](PyApi& self) -> PyApi& { return self; },
            nb::rv_policy::none)
        .def("__exit__",
             [](PyApi& self, nb::args) { self.close(); })
        .def_prop_ro(
            "session",
            [](PyApi& self) -> nb::object {
                auto session = self.api().getSession();
                if (!session) return nb::none();
                return nb::cast(session);
            })
        .def(
            "create_event_queue",
            [](PyApi& self) { return self.api().createEventQueue(); })
        .def(
            "wait_for_session",
            [](PyApi& self,
               double timeout) -> std::shared_ptr<Session> {
                auto queue = self.api().createEventQueue();
                auto deadline = std::chrono::steady_clock::now() +
                                std::chrono::duration<double>(timeout);

                while (true) {
                    auto remaining =
                        deadline - std::chrono::steady_clock::now();
                    if (remaining <=
                        std::chrono::steady_clock::duration::zero()) {
                        PyErr_SetString(PyExc_TimeoutError,
                                        "Timed out waiting for session");
                        throw nb::python_error();
                    }

                    std::optional<Event> event;
                    {
                        nb::gil_scoped_release release;
                        event = queue->tryPopUntil(deadline);
                    }

                    if (!event) {
                        PyErr_SetString(PyExc_TimeoutError,
                                        "Timed out waiting for session");
                        throw nb::python_error();
                    }

                    auto* sse =
                        std::get_if<SessionStateChanged>(&*event);
                    if (sse && sse->session) {
                        return sse->session;
                    }
                }
            },
            nb::arg("timeout"))
        .def(
            "events",
            [](PyApi& self,
               std::optional<double> timeout) -> EventIterator {
                auto queue = self.api().createEventQueue();
                return EventIterator{std::move(queue), timeout};
            },
            nb::arg("timeout") = nb::none())
        .def("close", &PyApi::close)
        .def("__repr__", [](PyApi&) { return "<Api>"; });
}
