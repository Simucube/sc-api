/**
 * @file
 * @brief Dashboard frame streaming API
 *
 * Provides a simple interface for streaming dashboard frames to devices using
 * shared memory for efficient large frame transfers.
 */

#ifndef SC_API_CORE_DASH_STREAM_H_
#define SC_API_CORE_DASH_STREAM_H_

#include <cstdint>
#include <memory>
#include <optional>

#include "sc-api/core/session.h"
#include "sc-api/core/time.h"

namespace sc_api::core {

/**
 * @brief Outcome of a single @ref DashStreamer::streamFrame call.
 */
enum class FrameResult {
    /** Frame was written to shared memory and published for the backend to poll. */
    delivered,
    /** Frame skipped because previous frame was not yet consumed by the backend. */
    dropped,
    /** Frame could not be sent due to transport or state error. */
    failed,
};

/**
 * @brief Backend-provided feedback for a dash stream, polled by the client.
 *
 * A snapshot of the most recent values the backend published into shared memory (see
 * @ref DashStreamer::getStreamFeedback). The counters are telemetry-rate — the device link adds
 * latency, especially over WirQr — so they are good for pacing decisions, not a hard per-frame sync.
 */
struct StreamFeedback {
    /** True if this streamer currently owns the device (its frames are being forwarded). */
    bool is_owner                 = false;
    /** Frames the device has actually decoded/displayed (firmware counter, curated passthrough). */
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
 *       sharing a single instance across threads.
 *
 * The streamer connects lazily: the buffer is requested from the backend on the first
 * @ref streamFrame (and retried with backoff while the backend is unavailable). Call @ref open
 * up front only if you want to detect a connection failure before the first frame.
 *
 * Example usage:
 * @code
 * DashStreamer streamer(session, device_session_id);
 *
 * uint16_t* rgb565_pixels = ...;
 * if (streamer.streamFrame(800, 480, rgb565_pixels) == FrameResult::failed) {
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
    DashStreamer(std::shared_ptr<sc_api::core::Session> session, uint16_t device_session_id);

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
     * @brief Whether the shared-memory buffer is open and ready to stream.
     *
     * False until the buffer has been opened — either by an explicit @ref open or lazily by the
     * first @ref streamFrame. Construction alone does not open the buffer.
     *
     * @return true if the buffer is allocated and mapped.
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
     * @return @ref FrameResult::delivered on success,
     *         @ref FrameResult::dropped if the previous frame has not yet been
     *         consumed by the backend,
     *         @ref FrameResult::failed on transport/state error or if the
     *         requested frame size exceeds the allocated shared-memory buffer.
     */
    FrameResult streamFrame(uint16_t width, uint16_t height, const uint16_t* rgb565_data);

    /**
     * @brief Signal a controlled stop to the device
     *
     * Fire-and-forget `dash_stream:stop_stream` service command. The backend emits an explicit
     * dash-stream stop packet so the wheel leaves streaming immediately instead of waiting out its
     * inter-frame fallback timeout (~3 s). Purely a latency optimisation: if it is lost or never
     * sent, the firmware's autonomous timeout still falls back, so correctness never depends on it.
     * Call after the last frame and before destroying the streamer.
     */
    void stop();

    /**
     * @brief Whether this streamer currently owns its target device.
     *
     * Ownership is first-writer-wins: the first streamer to deliver a frame to a device owns it,
     * and other senders' frames are dropped until ownership is released (explicit stop/teardown or
     * an inactivity timeout). Polled from shared memory.
     *
     * @return true if this streamer's frames are being forwarded to the device.
     */
    bool isOwner() const;

    /**
     * @brief Latest backend feedback for this stream (ownership + device progress telemetry).
     *
     * Use it to derive lag (submitted - device_frame_counter), effective device fps, and to drive
     * your own pacing policy. Returns default-constructed feedback (is_owner=false, counters 0) if
     * the streamer is invalid.
     */
    StreamFeedback getStreamFeedback() const;

    /**
     * @brief Get the device session ID this streamer is targeting
     *
     * @return The target device session ID, or @c std::nullopt if the streamer
     *         is in a moved-from state (@ref isValid returns false does not
     *         guarantee @c std::nullopt — buffer allocation may have failed).
     */
    std::optional<uint16_t> getDeviceSessionId() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;

    bool tryOpenIfNecessary();
};

}  // namespace sc_api::core

#endif  // SC_API_CORE_DASH_STREAM_H_
