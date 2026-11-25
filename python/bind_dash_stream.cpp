#include <nanobind/nanobind.h>
#include <nanobind/ndarray.h>
#include <nanobind/stl/optional.h>
#include <nanobind/stl/shared_ptr.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/vector.h>
#include <sc-api/dash_stream.h>
#include <sc-api/device.h>
#include <sc-api/session.h>
#include <sc-api/touch_areas.h>

#include <cstdint>
#include <cstring>
#include <optional>
#include <string>
#include <vector>

namespace nb = nanobind;

using sc_api::DashStreamer;
using sc_api::DeviceSessionId;
using sc_api::FrameResult;
using sc_api::Session;
using sc_api::StreamFeedback;
using sc_api::TouchArea;

namespace {

/// Stream a raw RGB565 buffer, copying first if it is not 2-byte aligned.
///
/// `data` must hold `width * height` packed little-endian RGB565 pixels. Views into byte buffers
/// (a cv2 uint8 (H, W, 2) frame, a bytes slice) can start on an odd address, which `uint16_t` reads
/// do not allow. The GIL is released only around the backend call; no Python object is touched
/// while it is released.
FrameResult streamRaw(DashStreamer& self, uint16_t width, uint16_t height, const void* data,
                      const std::vector<TouchArea>& areas) {
    std::vector<uint16_t> aligned;
    const uint16_t*       pixels = nullptr;

    if (reinterpret_cast<uintptr_t>(data) % alignof(uint16_t) == 0) {
        pixels = static_cast<const uint16_t*>(data);
    } else {
        aligned.resize(static_cast<size_t>(width) * static_cast<size_t>(height));
        std::memcpy(aligned.data(), data, aligned.size() * sizeof(uint16_t));
        pixels = aligned.data();
    }

    nb::gil_scoped_release release;
    return self.streamFrame(width, height, pixels, areas);
}

}  // namespace

void bind_dash_stream(nb::module_& m) {
    // --- FrameResult ---

    nb::enum_<FrameResult>(m, "FrameResult", "Outcome of a single DashStreamer.stream_frame() call.")
        .value("delivered", FrameResult::delivered,
               "Frame was written to shared memory and published for the backend to poll.")
        .value("dropped", FrameResult::dropped,
               "Frame skipped because the previous frame was not yet consumed by the backend.")
        .value("failed", FrameResult::failed,
               "Frame was not sent: a transport or state error, or an invalid argument (see stream_frame).");

    // --- StreamFeedback ---

    nb::class_<StreamFeedback>(m, "StreamFeedback",
                               "Backend-provided feedback for a dash stream, polled from shared memory. "
                               "The counters are telemetry-rate and the device link adds latency, so they "
                               "are good for pacing decisions, not a hard per-frame sync.")
        .def_prop_ro(
            "is_owner", [](const StreamFeedback& self) { return self.is_owner; },
            "True if the streamer currently owns the device, meaning its frames are being forwarded.")
        .def_prop_ro(
            "device_frame_counter", [](const StreamFeedback& self) { return self.device_frame_counter; },
            "Frames the device has decoded, from every sender. It also counts frames the device skipped to "
            "show a newer one. It reads 0 until the device reports its first count, and it starts again from 0 "
            "when the device restarts.")
        .def_prop_ro(
            "dropped_count", [](const StreamFeedback& self) { return self.dropped_count; },
            "Frames or packets the device dropped.")
        .def_prop_ro(
            "last_ack_time_ns",
            [](const StreamFeedback& self) -> int64_t { return self.last_ack_time.time_since_epoch().count(); },
            "Backend monotonic timestamp of the last device_frame_counter advance, in nanoseconds. "
            "Same time base as Clock.now_ns().")
        .def("__repr__", [](const StreamFeedback& self) {
            return "<StreamFeedback is_owner=" + std::string(self.is_owner ? "True" : "False") +
                   " device_frame_counter=" + std::to_string(self.device_frame_counter) +
                   " dropped_count=" + std::to_string(self.dropped_count) + ">";
        });

    // --- TouchArea ---

    m.attr("MAX_MOUSE_SUPPRESSED_AREAS") = sc_api::k_max_mouse_suppressed_areas;

    nb::class_<TouchArea>(m, "TouchArea",
                          "A rectangle of the screen in screen pixels, the same as the frame and the touch "
                          "events: origin at the top left corner, x to the right, y down. It covers x to "
                          "x + w - 1 and y to y + h - 1. An area with w or h 0 is empty. An area can overlap other "
                          "areas and the screen edges. TouchArea(0, 0, screen.width, screen.height) with a "
                          "ScreenFeedback covers the full screen.")
        .def(
            "__init__",
            [](TouchArea* self, int16_t x, int16_t y, uint16_t w, uint16_t h) { new (self) TouchArea{x, y, w, h}; },
            nb::arg("x") = 0, nb::arg("y") = 0, nb::arg("w") = 0, nb::arg("h") = 0,
            "Construct a TouchArea. x and y are -32768 to 32767, w and h 0 to 65535.")
        .def_rw("x", &TouchArea::x, "Left edge in pixels.")
        .def_rw("y", &TouchArea::y, "Top edge in pixels.")
        .def_rw("w", &TouchArea::w, "Width in pixels.")
        .def_rw("h", &TouchArea::h, "Height in pixels.")
        .def("contains", &TouchArea::contains, nb::arg("x"), nb::arg("y"),
             "True if the pixel (x, y) is inside the area, with the same edge rule as the device.")
        .def(
            "__eq__", [](const TouchArea& a, const TouchArea& b) { return a == b; }, nb::is_operator())
        .def("__repr__", [](const TouchArea& a) {
            return "TouchArea(" + std::to_string(a.x) + ", " + std::to_string(a.y) + ", " + std::to_string(a.w) + ", " +
                   std::to_string(a.h) + ")";
        });
    // The fields are writable, so equal areas must not be dict keys; nanobind keeps object.__hash__ after __eq__
    m.attr("TouchArea").attr("__hash__") = nb::none();

    // --- DashStreamer ---

    nb::class_<DashStreamer>(
        m, "DashStreamer",
        "Streams dashboard frames to a device over shared memory. Requires control session state.\n\n"
        "The streamer connects lazily: the shared memory buffer is requested on the first "
        "stream_frame(). Call open() up front only to detect a connection failure early.\n\n"
        "A frame is accepted in three forms: a C-contiguous uint16 array of shape (height, width) "
        "holding packed RGB565 pixels, a C-contiguous uint8 array of shape (height, width, 2) as "
        "produced by cv2.cvtColor(img, cv2.COLOR_BGR2BGR565), or explicit width, height and a bytes "
        "object of width * height * 2 bytes.\n\n"
        "Not thread-safe. Serialize stream_frame() calls externally when sharing one instance "
        "across threads; set_target_fps() is the one method another thread may call while "
        "stream_frame() runs. Use as a context manager to send the stop signal on exit.\n\n"
        "Use one DashStreamer per device in a session. Two streamers of one session for the same "
        "device share one stream, and destroying one stops the other.")
        .def(
            "__init__",
            [](DashStreamer* self, const std::shared_ptr<Session>& session, DeviceSessionId device) {
                new (self) DashStreamer(session, device.id);
            },
            nb::arg("session"), nb::arg("device"), "Create a dash streamer for the given session and device.")
        .def(
            "open",
            [](DashStreamer& self) {
                nb::gil_scoped_release release;
                return self.open();
            },
            "Request the shared memory buffer from the backend (blocking).\n\n"
            ":returns: True if the backend responded and the buffer could be obtained. "
            "Calling this is optional; the first stream_frame() opens the buffer too.")
        .def(
            "stream_frame",
            [](DashStreamer& self, const nb::ndarray<nb::ro, nb::c_contig, nb::device::cpu>& frame,
               const std::vector<TouchArea>& areas) {
                const size_t ndim   = frame.ndim();
                size_t       height = 0;
                size_t       width  = 0;

                if (ndim == 2 && frame.dtype() == nb::dtype<uint16_t>()) {  // NOLINT(bugprone-branch-clone)
                    height = frame.shape(0);
                    width  = frame.shape(1);
                } else if (ndim == 3 && frame.dtype() == nb::dtype<uint8_t>() && frame.shape(2) == 2) {
                    // cv2 COLOR_BGR2BGR565 output: two bytes per pixel that are the packed RGB565
                    // value in little-endian order. Windows is little-endian, so no swap is needed.
                    height = frame.shape(0);
                    width  = frame.shape(1);
                } else {
                    throw nb::type_error(
                        "frame must be a C-contiguous uint16 array of shape (height, width) holding packed "
                        "RGB565 pixels, or a C-contiguous uint8 array of shape (height, width, 2) as produced "
                        "by cv2.cvtColor(img, cv2.COLOR_BGR2BGR565)");
                }

                if (width > UINT16_MAX || height > UINT16_MAX) {
                    std::string msg = "frame is too large: width " + std::to_string(width) + " and height " +
                                      std::to_string(height) + " must both fit in 16 bits";
                    throw nb::value_error(msg.c_str());
                }

                return streamRaw(self, static_cast<uint16_t>(width), static_cast<uint16_t>(height), frame.data(),
                                 areas);
            },
            nb::arg("frame").noconvert(), nb::arg("areas") = std::vector<TouchArea>{},
            "Stream one dashboard frame from a numpy array.\n\n"
            "The areas go to the device with this frame and apply while the device shows it. A later frame "
            "without areas removes them. A touch that starts in an area does not move the mouse until all its "
            "contact points lift, and its touch events carry TOUCH_EVENT_MOUSE_SUPPRESSED. One area can cover a "
            "group of buttons. Every entry counts toward the limit, also an empty one.\n\n"
            ":param frame: A C-contiguous uint16 array of shape (height, width) holding packed RGB565\n"
            "    pixels, or a C-contiguous uint8 array of shape (height, width, 2) as produced by\n"
            "    cv2.cvtColor(img, cv2.COLOR_BGR2BGR565). The array is read without a copy unless its\n"
            "    data is not 2-byte aligned. A strided array is rejected, not copied; run\n"
            "    numpy.ascontiguousarray on it first.\n"
            ":param areas: TouchArea list of the screen areas where a touch does not move the PC mouse\n"
            "    pointer. The device applies the first ScreenFeedback.max_mouse_suppressed_areas of them, at\n"
            "    most MAX_MOUSE_SUPPRESSED_AREAS; when that value is 0 the frame shows without the areas.\n"
            ":returns: FrameResult. FrameResult.failed also when areas has more than\n"
            "    MAX_MOUSE_SUPPRESSED_AREAS entries; then the frame is not sent. Raises TypeError for any other\n"
            "    array shape, dtype or memory layout, and ValueError if the width or height does not fit in\n"
            "    16 bits.")
        .def(
            "stream_frame",
            [](DashStreamer& self, uint16_t width, uint16_t height, const nb::bytes& data,
               const std::vector<TouchArea>& areas) {
                const size_t expected = static_cast<size_t>(width) * static_cast<size_t>(height) * 2;
                if (data.size() != expected) {
                    std::string msg = "data has " + std::to_string(data.size()) + " bytes but " +
                                      std::to_string(width) + "x" + std::to_string(height) + " RGB565 needs " +
                                      std::to_string(expected);
                    throw nb::value_error(msg.c_str());
                }

                return streamRaw(self, width, height, data.c_str(), areas);
            },
            nb::arg("width"), nb::arg("height"), nb::arg("data"), nb::arg("areas") = std::vector<TouchArea>{},
            "Stream one dashboard frame from a bytes object.\n\n"
            ":param width: Frame width in pixels.\n"
            ":param height: Frame height in pixels.\n"
            ":param data: Packed RGB565 pixels, width * height * 2 bytes, row-major.\n"
            ":param areas: As for the array form.\n"
            ":returns: FrameResult. FrameResult.failed also when areas has more entries than\n"
            "    MAX_MOUSE_SUPPRESSED_AREAS; then the frame is not sent. Raises ValueError if the buffer size\n"
            "    does not match the frame size.")
        .def("set_target_fps", &DashStreamer::setTargetFps, nb::arg("fps"),
             "Set the target frame rate of this streamer.\n\n"
             "A higher target trades picture quality for frame rate. It is a hint: the device frame rate also "
             "depends on the content and the link. Measure that rate with StreamFeedback.device_frame_counter "
             "while this streamer owns the device. The target does not pace or limit stream_frame(), and "
             "the device cannot show more frames than you submit.\n\n"
             "The target applies from the next stream_frame() call. A set_target_fps() call during a running "
             "stream_frame() applies to that frame or the next. A new streamer starts without a target.\n\n"
             ":param fps: Frames per second; 0 = no target (default). A target above the display refresh\n"
             "    rate counts as the refresh rate.\n"
             "    A float, even 30.0, a negative value or a value above 65535 raises TypeError.")
        .def("stop", &DashStreamer::stop,
             "Stop this streamer. If another source is still streaming, the display falls back "
             "to it. Otherwise the device leaves streaming immediately instead of waiting out "
             "its inter-frame timeout. Call it after the last frame. Fire-and-forget: "
             "correctness never depends on it.")
        .def("get_stream_feedback", &DashStreamer::getStreamFeedback,
             "Read the latest backend feedback for this stream.\n\n"
             ":returns: A StreamFeedback snapshot. Default values (is_owner False, counters 0) "
             "if the streamer is not valid.")
        .def_prop_ro("is_valid", &DashStreamer::isValid,
                     "True if the shared memory buffer is open and the session is alive. "
                     "False until open() or the first stream_frame() has opened it, and after the "
                     "session is lost: create a new streamer for the new session.")
        .def_prop_ro("is_owner", &DashStreamer::isOwner,
                     "True if this streamer's frames are being forwarded to the device. "
                     "Ownership is newest-wins: a streamer that resumes after being idle preempts "
                     "the current owner. When the owner stops, the display falls back to the most "
                     "recently active remaining streamer. A demoted streamer keeps its place in "
                     "that order only by continuing to send frames.")
        .def_prop_ro(
            "device_session_id",
            [](const DashStreamer& self) -> std::optional<DeviceSessionId> {
                std::optional<uint16_t> id = self.getDeviceSessionId();
                if (!id) {
                    return std::nullopt;
                }
                return DeviceSessionId{*id};
            },
            "The device session ID this streamer targets, or None if the streamer was moved from.")
        .def(
            "__enter__", [](DashStreamer& self) -> DashStreamer& { return self; }, nb::rv_policy::none)
        .def("__exit__",
             [](DashStreamer& self, nb::args /*unused*/) {  // NOLINT(performance-unnecessary-value-param)
                 self.stop();
             })
        .def("__repr__", [](const DashStreamer& self) {
            std::optional<uint16_t> id = self.getDeviceSessionId();
            return "<DashStreamer device=" + (id ? std::to_string(*id) : std::string("None")) +
                   " valid=" + (self.isValid() ? "True" : "False") + ">";
        });
}
