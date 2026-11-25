#include <nanobind/nanobind.h>
#include <nanobind/stl/optional.h>
#include <nanobind/stl/pair.h>
#include <nanobind/stl/shared_ptr.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/vector.h>
#include <sc-api/device.h>
#include <sc-api/input_events.h>
#include <sc-api/session.h>

#include <algorithm>
#include <cstdint>
#include <iterator>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace nb = nanobind;

using sc_api::DeviceSessionId;
using sc_api::InputEvent;
using sc_api::InputEventReader;
using sc_api::InputEventType;
using sc_api::Session;
using sc_api::TouchEventPayload;

namespace {

struct PyInputEventReadResult {
    nb::typed<nb::list, InputEvent> events;
    uint32_t                        lost;
};

// Holds the reader so close() can destroy it before garbage collection.
struct PyInputEventReader {
    std::optional<InputEventReader> reader;

    explicit PyInputEventReader(std::shared_ptr<Session> session) : reader(std::in_place, std::move(session)) {}

    InputEventReader& get() {
        if (!reader) {
            throw nb::value_error("InputEventReader has been closed");
        }
        return *reader;
    }
};

}  // namespace

void bind_input_events(nb::module_& m) {
    nb::enum_<InputEventType>(m, "InputEventType", "Kind of an input event.")
        .value("invalid", InputEventType::invalid, "Not a valid event. A reader never delivers it.")
        .value("button_pressed", InputEventType::button_pressed, "The input went from released to pressed.")
        .value("button_released", InputEventType::button_released, "The input went from pressed to released.")
        .value("touch", InputEventType::touch, "The state of one touch panel of the device.");

    m.attr("TOUCH_EVENT_MAX_POINTS")        = sc_api::k_touch_event_max_points;
    m.attr("TOUCH_EVENT_MOUSE_SUPPRESSED")  = sc_api::k_touch_event_mouse_suppressed;
    m.attr("TOUCH_EVENT_CANCELLED")         = sc_api::k_touch_event_cancelled;
    m.attr("TOUCH_EVENT_SINGLE_FINGER_TAP") = sc_api::k_touch_event_single_finger_tap;
    m.attr("TOUCH_EVENT_TAP_SHIFT")         = sc_api::k_touch_event_tap_shift;

    nb::class_<TouchEventPayload>(m, "TouchEventPayload",
                                  "The state of a touch panel at the time of a touch event. Holds up to "
                                  "``TOUCH_EVENT_MAX_POINTS`` contact points; point N is index N of ``x`` and ``y`` "
                                  "and bit N of ``points_up`` and ``points_down``.")
        .def(nb::init<>(), "A released panel: no contact point, every field 0.")
        .def_prop_ro(
            "x",
            [](const TouchEventPayload& self) { return std::vector<int16_t>(std::begin(self.x), std::end(self.x)); },
            "X position of each contact point in pixels of the device screen, from the left edge. A lifted point "
            "gives the position where it was lifted. A point that is neither down nor lifted reads 0.")
        .def_prop_ro(
            "y",
            [](const TouchEventPayload& self) { return std::vector<int16_t>(std::begin(self.y), std::end(self.y)); },
            "Y position of each contact point in pixels of the device screen, from the top edge. Same rules as ``x``.")
        .def_ro("points_up", &TouchEventPayload::points_up,
                "Bit N is set on the event where contact point N is lifted. Bit N + ``TOUCH_EVENT_TAP_SHIFT`` is also "
                "set if that contact was a tap: a short touch that stayed near the position where it went down.")
        .def_ro("points_down", &TouchEventPayload::points_down, "Bit N is set while contact point N is down.")
        .def_ro("flags", &TouchEventPayload::flags,
                "``TOUCH_EVENT_MOUSE_SUPPRESSED``: the device can move the PC mouse pointer with a touch, and this "
                "touch did not, for example because it started in a mouse suppressed area. ``TOUCH_EVENT_CANCELLED``: "
                "the device became unavailable and the contact points that "
                "were down were not lifted; end the touch actions of the panel. ``TOUCH_EVENT_SINGLE_FINGER_TAP``: the "
                "tap of this event had no other contact point down from its press to its lift. Ignore a bit you do "
                "not know.")
        .def("__repr__", [](const TouchEventPayload& self) {
            return "<TouchEventPayload points_down=" + std::to_string(self.points_down) +
                   " points_up=" + std::to_string(self.points_up) + " flags=" + std::to_string(self.flags) + ">";
        });

    nb::class_<InputEvent>(m, "InputEvent", "One input event. A copy that stays valid after the reader is gone.")
        .def_ro("timestamp_ns", &InputEvent::timestamp,
                "Time of the event in nanoseconds, on the clock of ``Clock.now_ns()``. Events processed together can "
                "share a timestamp.")
        .def_prop_ro(
            "device_session_id", [](const InputEvent& self) { return DeviceSessionId{self.device_session_id}; },
            "Device that owns the input. For a wireless wheel this is the wheel, not the SC-link Hub.")
        .def_ro("type", &InputEvent::type, "Kind of the event.")
        .def_ro("input_id", &InputEvent::input_id,
                "For a button event: the ``Input.event_id`` of the input. It does not change when the user remaps "
                "the button. Resolve it with ``DeviceInfo.get_input_by_event_id``. For a touch event: the index of "
                "the touch panel within the device, 0 for the first.")
        .def_prop_ro(
            "hid_index",
            [](const InputEvent& self) -> std::optional<uint16_t> {
                if (self.hid_index == sc_api::k_input_event_no_hid_index) {
                    return std::nullopt;
                }
                return self.hid_index;
            },
            "For a button event: the HID button that the input was mapped to when the event happened, as a 0-based "
            "index into ``DeviceInfo.hid_buttons`` of the event's device. If that list is empty, use the list of the "
            "device that ``parent_session_id`` names. ``None`` if the input has no HID input, and for a touch event. "
            "A remap between a press and its release changes the ``hid_index`` of the release, so key a press action "
            "by ``input_id``.")
        .def_prop_ro(
            "touch",
            [](const InputEvent& self) -> std::optional<TouchEventPayload> {
                const TouchEventPayload* touch = sc_api::getEventPayload<TouchEventPayload>(&self);
                if (!touch) {
                    return std::nullopt;
                }
                return *touch;
            },
            "The panel state if ``type`` is ``InputEventType.touch``, else ``None``.")
        .def_prop_ro(
            "single_finger_tap",
            [](const InputEvent& self) -> std::optional<std::pair<int16_t, int16_t>> {
                const std::optional<sc_api::TouchPosition> tap = sc_api::getSingleFingerTap(self);
                if (!tap) {
                    return std::nullopt;
                }
                return std::pair{tap->x, tap->y};
            },
            "``(x, y)`` where the single-finger tap of this touch event was lifted, or ``None``. Other taps are "
            "only in ``TouchEventPayload.points_up``.")
        .def("__repr__", [](const InputEvent& self) {
            return nb::str("<InputEvent type={} device={} input_id={}>")
                .format(nb::cast(self.type).attr("name"), self.device_session_id, self.input_id);
        });

    nb::class_<PyInputEventReadResult>(m, "InputEventReadResult", "Result of one ``InputEventReader.read()`` call.")
        .def_ro("events", &PyInputEventReadResult::events, "The events, oldest first.")
        .def_ro("lost", &PyInputEventReadResult::lost,
                "Events of any type that the reader skipped because it fell too far behind. Check it before "
                "``events``. Lost events are never delivered, and a result that reports a loss has no events. The "
                "value saturates, so it is an indication, not an exact number.")
        .def("__repr__", [](const PyInputEventReadResult& self) {
            return "<InputEventReadResult events=" + std::to_string(self.events.size()) +
                   " lost=" + std::to_string(self.lost) + ">";
        });

    nb::class_<PyInputEventReader>(
        m, "InputEventReader",
        "Reads the input events of a session: button presses and releases, and touch panel states of the devices "
        "in device info. Polling the input variables misses a press and a release between two reads; this stream "
        "does not.\n\n"
        "A button event reports a change of one input. To know the state of every input, read the baseline. For a "
        "button with ``Input.event_id`` N, the state is bit ``N % 32`` of word ``N // 32`` of the ``digital_inputs0`` "
        "to ``digital_inputs3`` variables of the device that ``Input.variable`` names (``ww.digital_inputs0`` to "
        "``ww.digital_inputs3`` on the SC-link Hub for a wireless wheel). Read the baseline after ``open()``, when a "
        "device appears in device info, and after a read that reports a loss.\n\n"
        "A reader delivers no history: ``open()`` starts after the newest event. Use as a context manager to close "
        "the reader on exit; ``with`` does not open it.")
        .def(nb::init<std::shared_ptr<Session>>(), nb::arg("session"),
             "Construct a reader for a session. Construction does not open the stream; call ``open()``.\n\n"
             ":param session: Active API session.")
        .def(
            "open", [](PyInputEventReader& self) { return self.get().open(); },
            "Open the stream and start after the newest event. Does not block.\n\n"
            "A call on a reader that is already open changes nothing and returns ``True``. Construct a new reader "
            "to start again from the newest event.\n\n"
            ":returns: True if ``read()`` can deliver events. False when the session is lost or the stream is not "
            "available.\n\n"
            ":raises ValueError: If the reader is closed.")
        .def_prop_ro(
            "is_valid", [](const PyInputEventReader& self) { return self.reader && self.reader->isValid(); },
            "True if this reader can deliver events. False before a successful ``open()``, after the session is "
            "lost and after ``close()``. A lost session never recovers: get a new session and construct a new "
            "reader.")
        .def(
            "read",
            [](PyInputEventReader& self, int max_events) {
                InputEventReader& reader = self.get();
                if (max_events < 1) {
                    throw nb::value_error("max_events must be at least 1");
                }
                const uint32_t count =
                    std::min(static_cast<uint32_t>(max_events), sc_api::k_input_event_ring_max_capacity);
                std::vector<InputEvent>            events(count);
                const InputEventReader::ReadResult result = reader.read(events.data(), count);
                nb::typed<nb::list, InputEvent>    list;
                for (uint32_t i = 0; i < result.count; ++i) {
                    list.append(nb::cast(events[i]));
                }
                return PyInputEventReadResult{std::move(list), result.lost};
            },
            nb::arg("max_events") = 64,
            "Copy the events that this reader has not read yet, oldest first. Never blocks.\n\n"
            "Check ``lost`` before ``events``. A call that reports a loss returns no events, and each event of a "
            "later call is newer than each lost event.\n\n"
            "Events of a type that this version of the API does not know, such as a type that a newer Tuner adds, "
            "are filtered out and never reach Python. They count in neither ``events`` nor ``lost``, so a call can "
            "return no events while more wait; poll again. A reader that is not open, or whose session is lost, "
            "returns no events and no loss, so check ``is_valid`` when the stream stays silent.\n\n"
            ":param max_events: Most events to return in one call. Values above 65536 count as 65536.\n\n"
            ":returns: An ``InputEventReadResult``.\n\n"
            ":raises ValueError: If ``max_events`` is less than 1 or the reader is closed.")
        .def(
            "close", [](PyInputEventReader& self) { self.reader.reset(); },
            "Close the reader. Safe to call more than once. After it, ``is_valid`` is "
            "False and ``open()`` and ``read()`` raise ``ValueError``.")
        .def(
            "__enter__", [](PyInputEventReader& self) -> PyInputEventReader& { return self; }, nb::rv_policy::none)
        .def("__exit__",
             [](PyInputEventReader& self, nb::args /*unused*/) {  // NOLINT(performance-unnecessary-value-param)
                 self.reader.reset();
             })
        .def("__repr__", [](const PyInputEventReader& self) {
            if (!self.reader) {
                return std::string("<InputEventReader closed>");
            }
            return "<InputEventReader valid=" + std::string(self.reader->isValid() ? "True" : "False") + ">";
        });
}
