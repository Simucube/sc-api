#include <nanobind/nanobind.h>
#include <nanobind/ndarray.h>
#include <nanobind/stl/optional.h>
#include <nanobind/stl/shared_ptr.h>
#include <nanobind/stl/string.h>
#include <sc-api/dash_stream.h>
#include <sc-api/device.h>
#include <sc-api/session.h>

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

namespace {

/// Stream a raw RGB565 buffer, copying first if it is not 2-byte aligned.
///
/// `data` must hold `width * height` packed little-endian RGB565 pixels. Views into byte buffers
/// (a cv2 uint8 (H, W, 2) frame, a bytes slice) can start on an odd address, which `uint16_t` reads
/// do not allow. The GIL is released only around the backend call; no Python object is touched
/// while it is released.
FrameResult streamRaw(DashStreamer& self, uint16_t width, uint16_t height, const void* data) {
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
    return self.streamFrame(width, height, pixels);
}

}  // namespace

void bind_dash_stream(nb::module_& m) {
    // --- FrameResult ---

    nb::enum_<FrameResult>(m, "FrameResult", "Outcome of a single DashStreamer.stream_frame() call.")
        .value("delivered", FrameResult::delivered,
               "Frame was written to shared memory and published for the backend to poll.")
        .value("dropped", FrameResult::dropped,
               "Frame skipped because the previous frame was not yet consumed by the backend.")
        .value("failed", FrameResult::failed, "Frame could not be sent due to a transport or state error.");

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
            "Frames the device has actually decoded and displayed (firmware counter).")
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
        "across threads. Use as a context manager to send the stop signal on exit.")
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
            [](DashStreamer& self, const nb::ndarray<nb::ro, nb::c_contig, nb::device::cpu>& frame) {
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

                return streamRaw(self, static_cast<uint16_t>(width), static_cast<uint16_t>(height), frame.data());
            },
            nb::arg("frame").noconvert(),
            "Stream one dashboard frame from a numpy array.\n\n"
            ":param frame: A C-contiguous uint16 array of shape (height, width) holding packed RGB565\n"
            "    pixels, or a C-contiguous uint8 array of shape (height, width, 2) as produced by\n"
            "    cv2.cvtColor(img, cv2.COLOR_BGR2BGR565). The array is read without a copy unless its\n"
            "    data is not 2-byte aligned. A strided array is rejected, not copied; run\n"
            "    numpy.ascontiguousarray on it first.\n"
            ":returns: FrameResult. Raises TypeError for any other array shape, dtype or memory\n"
            "    layout, and ValueError if the width or height does not fit in 16 bits.")
        .def(
            "stream_frame",
            [](DashStreamer& self, uint16_t width, uint16_t height, const nb::bytes& data) {
                const size_t expected = static_cast<size_t>(width) * static_cast<size_t>(height) * 2;
                if (data.size() != expected) {
                    std::string msg = "data has " + std::to_string(data.size()) + " bytes but " +
                                      std::to_string(width) + "x" + std::to_string(height) + " RGB565 needs " +
                                      std::to_string(expected);
                    throw nb::value_error(msg.c_str());
                }

                return streamRaw(self, width, height, data.c_str());
            },
            nb::arg("width"), nb::arg("height"), nb::arg("data"),
            "Stream one dashboard frame from a bytes object.\n\n"
            ":param width: Frame width in pixels.\n"
            ":param height: Frame height in pixels.\n"
            ":param data: Packed RGB565 pixels, width * height * 2 bytes, row-major.\n"
            ":returns: FrameResult. Raises ValueError if the buffer size does not match the frame size.")
        .def("stop", &DashStreamer::stop,
             "Signal a controlled stop to the device so it leaves streaming immediately "
             "instead of waiting out its inter-frame timeout. Call it after the last frame. "
             "Fire-and-forget: correctness never depends on it.")
        .def("get_stream_feedback", &DashStreamer::getStreamFeedback,
             "Read the latest backend feedback for this stream.\n\n"
             ":returns: A StreamFeedback snapshot. Default values (is_owner False, counters 0) "
             "if the streamer is not valid.")
        .def_prop_ro("is_valid", &DashStreamer::isValid,
                     "True if the shared memory buffer is open and ready to stream. "
                     "False until open() or the first stream_frame() has opened it.")
        .def_prop_ro("is_owner", &DashStreamer::isOwner,
                     "True if this streamer's frames are being forwarded to the device. "
                     "Ownership is first-writer-wins and is released on stop, teardown or inactivity.")
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
