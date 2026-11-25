/**
 * @file
 * @brief Dashboard frame streaming API
 *
 * Provides a simple interface for streaming dashboard frames to devices using
 * shared memory for efficient large frame transfers.
 *
 * @see examples/dash_stream.cpp for a complete program.
 */

#ifndef SC_API_DASH_STREAM_H_
#define SC_API_DASH_STREAM_H_

#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

#include "sc-api/session.h"
#include "sc-api/time.h"
#include "sc-api/touch_areas.h"

namespace sc_api {

/**
 * @brief Outcome of a single @ref sc_api::DashStreamer::streamFrame "DashStreamer::streamFrame" call.
 */
enum class FrameResult {
    /** Frame was written to shared memory and published for the backend to poll. */
    delivered,
    /** Frame skipped because previous frame was not yet consumed by the backend. */
    dropped,
    /** Frame was not sent: a transport or state error, or an invalid argument (see streamFrame). */
    failed,
};

/**
 * @brief Backend-provided feedback for a dash stream, polled by the client.
 *
 * A snapshot of the most recent values the backend published into shared memory (see
 * @ref DashStreamer::getStreamFeedback). The counters are telemetry-rate - the device link adds
 * latency, especially with a wirelessly connected wheel - so they are good for pacing decisions,
 * not a hard per-frame sync.
 */
struct StreamFeedback {
    /** True if this streamer currently owns the device (its frames are being forwarded). */
    bool is_owner                 = false;
    /** Frames the device has decoded, from every sender. It also counts frames the device skipped to show
     *  a newer one. It reads 0 until the device reports its first count, and it starts again from 0 when
     *  the device restarts. */
    uint32_t device_frame_counter = 0;
    /** Frames/packets the device dropped. */
    uint32_t dropped_count        = 0;
    /** Backend monotonic timestamp of the last @c device_frame_counter advance. */
    Clock::time_point last_ack_time;
};

/**
 * @brief Dashboard frame streamer
 *
 * Manages shared memory buffer and streams dashboard frames to a device.
 * Handles buffer allocation, frame writing, and notification sending automatically.
 *
 * @note Not thread-safe. Serialize calls to @ref streamFrame externally when
 *       sharing a single instance across threads. The one exception is @ref setTargetFps,
 *       which another thread may call while @ref streamFrame runs.
 *
 * @note Streaming needs control access: register through @ref NoAuthControlEnabler with
 *       Session::control_display before streaming.
 *
 * @note Use one DashStreamer per device in a session. Two streamers of one session for the same
 *       device share one stream, and destroying one stops the other.
 *
 * The streamer connects lazily: the buffer is requested from the backend on the first
 * @ref streamFrame (and retried with backoff while the backend is unavailable). Call @ref open
 * up front only if you want to detect a connection failure before the first frame.
 *
 * Find stream-capable devices and their frame size and pixel format through
 * @ref sc_api::device_info::ScreenFeedback "device_info::ScreenFeedback"
 * (feedback type `screen` in device info).
 *
 * Example usage:
 * @code
 * DashStreamer streamer(session, device_session_id);
 *
 * // screen is the device_info::ScreenFeedback of the target display.
 * uint16_t* rgb565_pixels = ...;
 * if (streamer.streamFrame(screen.width, screen.height, rgb565_pixels) == FrameResult::failed) {
 *     // Not connected yet (retrying) or the frame was rejected.
 * }
 * @endcode
 */
class DashStreamer {
public:
    /**
     * @brief Construct a dash streamer for a device
     *
     * @param session Active API session
     * @param device_session_id Target device session ID
     */
    DashStreamer(std::shared_ptr<sc_api::Session> session, uint16_t device_session_id);

    /**
     * @brief Destructor - releases the backend buffer best-effort
     *
     * Issues a fire-and-forget `dash_stream:release_buffer` service command so
     * the backend can reclaim the shared memory immediately. The backend also
     * cleans up on controller disconnect, so failure here is not fatal.
     */
    ~DashStreamer();

    // Non-copyable
    DashStreamer(const DashStreamer&)            = delete;
    DashStreamer& operator=(const DashStreamer&) = delete;

    // Movable
    DashStreamer(DashStreamer&& other) noexcept;
    DashStreamer& operator=(DashStreamer&& other) noexcept;

    /**
     * @brief Whether the shared-memory buffer is open and the session is alive.
     *
     * False until the buffer has been opened - either by an explicit @ref open or lazily by the
     * first @ref streamFrame. Construction alone does not open the buffer. False again after the
     * session is lost: create a new streamer for the new session.
     *
     * @return true if the buffer is mapped and the session is alive.
     */
    bool isValid() const;

    /** @brief Attempt to open stream
     *
     *  Requests a shared memory buffer for sending stream data.
     *
     *  @return true, if backend responded to request and shared memory buffer could be obtained.
     */
    bool open();

    /**
     * @brief Stream a dashboard frame
     *
     * Writes the frame to shared memory; the backend polls the shared-memory revision and pulls
     * the frame on its packet timer (no per-frame notification). The frame data is expected to be
     * in RGB565 format (2 bytes per pixel).
     *
     * @param width Frame width in pixels
     * @param height Frame height in pixels
     * @param rgb565_data Pointer to RGB565 pixel data (width * height * 2 bytes)
     * @return @ref FrameResult::delivered on success, whether or not this streamer owns the
     *         device - non-owner frames are consumed too, so @ref isOwner, not this return
     *         value, is the way to check ownership.
     *         @ref FrameResult::dropped if the previous frame has not yet been
     *         consumed by the backend,
     *         @ref FrameResult::failed on transport/state error or if the
     *         requested frame size exceeds the allocated shared-memory buffer.
     */
    FrameResult streamFrame(uint16_t width, uint16_t height, const uint16_t* rgb565_data);

    /**
     * @brief Stream a dashboard frame with the screen areas where a touch does not move the PC mouse pointer
     *
     * The areas go to the device with this frame and apply while the device shows it. A later frame
     * without areas removes them. A touch that starts in an area does not move the mouse until all
     * its contact points lift, and its touch events carry @ref k_touch_event_mouse_suppressed. One area
     * can cover a group of buttons. Every entry counts toward the limit, also an empty one.
     *
     * @param width,height,rgb565_data As for the overload without areas.
     * @param areas Screen areas where a touch must not move the PC mouse pointer. The device applies the
     *        first device_info::ScreenFeedback::max_mouse_suppressed_areas of them, at most
     *        @ref k_max_mouse_suppressed_areas; when that value is 0 the frame shows without the areas.
     * @return As for the overload without areas. @ref FrameResult::failed also when @p areas has
     *         more than @ref k_max_mouse_suppressed_areas entries; then the frame is not sent.
     */
    FrameResult streamFrame(uint16_t width, uint16_t height, const uint16_t* rgb565_data,
                            const std::vector<TouchArea>& areas);

    /**
     * @brief Set the target frame rate of this streamer
     *
     * A higher target trades picture quality for frame rate. It is a hint: the device frame rate also
     * depends on the content and the link. Measure that rate with @ref StreamFeedback::device_frame_counter
     * while this streamer owns the device. The target does not pace or limit @ref streamFrame, and the
     * device cannot show more frames than you submit.
     *
     * The target applies from the next @ref streamFrame call. A setTargetFps call during a running
     * @ref streamFrame applies to that frame or the next. A new streamer starts without a target.
     *
     * @param fps Frames per second; 0 = no target (default). A target above the display refresh rate
     *            counts as the refresh rate.
     */
    void setTargetFps(uint16_t fps);

    /**
     * @brief Stop this streamer
     *
     * If another source is still streaming, the display falls back to it. Otherwise, this sends a
     * fire-and-forget `dash_stream:stop_stream` service command: the backend emits an explicit
     * dash-stream stop packet so the wheel leaves streaming immediately instead of waiting out its
     * inter-frame fallback timeout (~3 s). Purely a latency optimisation: if it is lost or never
     * sent, the firmware's autonomous timeout still falls back, so correctness never depends on it.
     * Call after the last frame and before destroying the streamer.
     */
    void stop();

    /**
     * @brief Whether this streamer currently owns its target device.
     *
     * Ownership is newest-wins: a streamer that resumes sending frames after a period of not
     * streaming preempts the current owner immediately. When the owner stops (explicit stop,
     * disconnect, or ~2.5 s without a frame), the display falls back to the most recently active
     * remaining streamer instead of leaving streaming mode. A demoted streamer that keeps sending
     * frames keeps its place in that fallback order and does not re-preempt; pausing while demoted
     * drops it out of the order, so it preempts again on resume. Polled from shared memory.
     *
     * @return true if this streamer's frames are being forwarded to the device.
     */
    bool isOwner() const;

    /**
     * @brief Latest backend feedback for this stream (ownership + device progress telemetry).
     *
     * Use it to measure the device frame rate and to drive your own pacing. Delivered frames minus the
     * counter does not measure lag, because not every delivered frame reaches the device. Returns
     * default-constructed feedback (is_owner=false, counters 0) if the streamer is invalid.
     */
    StreamFeedback getStreamFeedback() const;

    /**
     * @brief Get the device session ID this streamer is targeting
     *
     * @return The target device session ID, or @c std::nullopt if the streamer
     *         is in a moved-from state (@ref isValid returns false does not
     *         guarantee @c std::nullopt - buffer allocation may have failed).
     */
    std::optional<uint16_t> getDeviceSessionId() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;

    bool tryOpenIfNecessary();
};

}  // namespace sc_api

#endif  // SC_API_DASH_STREAM_H_
