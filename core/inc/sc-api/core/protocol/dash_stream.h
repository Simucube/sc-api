#ifndef SC_API_PROTOCOL_DASH_STREAM_H_
#define SC_API_PROTOCOL_DASH_STREAM_H_

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace sc_api::core {

/** Cache-line size used to separate the two writer domains in DashFrameShm onto distinct cache
 *  lines (avoids false sharing). Pinned ABI constant: the struct layout must be byte-identical
 *  across the separately-compiled client and backend, so this is hardcoded — NOT
 *  std::hardware_destructive_interference_size, whose value can differ between translation units. */
inline constexpr std::size_t k_dash_shm_cache_line_size = 64;

/** Shared memory frame layout for dash streaming. Followed by frame pixel data (RGB565) starting
 *  at offset sizeof(DashFrameShm).
 *
 *  Two independent writers share this cross-process region:
 *    - the client writes the frame-handshake group (revision/width/height) and the pixel data;
 *    - the backend writes the feedback group every ~1 ms.
 *  The groups sit on separate cache lines (alignas) so the backend's high-rate feedback stores
 *  do not false-share with the client's frame handshake or the large per-frame pixel memcpy.
 *  Every cross-writer field is a lock-free atomic following the single-writer release/acquire
 *  discipline (writer: release store; reader: acquire load). */
struct alignas(k_dash_shm_cache_line_size) DashFrameShm {
    // Frame handshake — client writes, backend reads. `revision` is the publish handshake:
    // even = slot free for the client to write, odd = a new frame is published and not yet
    // consumed by the backend (backend bumps it back to even on copy-out).
    std::atomic<uint32_t> revision;
    uint16_t              width;
    uint16_t              height;

    // Feedback — backend writes, client reads (see DashStreamer::getStreamFeedback). Placed on
    // its own cache line so the ~1 ms backend writes stay isolated from the client side.
    alignas(k_dash_shm_cache_line_size) std::atomic<uint64_t> last_ack_time_us;
    std::atomic<uint32_t> is_owner;
    std::atomic<uint32_t> device_frame_counter;
    std::atomic<uint32_t> dropped_count;
};

// Pin SHM layout. Any accidental drift (alignment change, type swap, field
// reorder) trips the build on both client and backend.
static_assert(sizeof(DashFrameShm) == 128, "DashFrameShm must be exactly 128 bytes");
static_assert(alignof(DashFrameShm) == 64, "DashFrameShm must be 64-byte aligned");
static_assert(std::atomic<uint32_t>::is_always_lock_free,
              "DashFrameShm 32-bit atomics must be lock-free for safe use in shared memory");
static_assert(std::atomic<uint64_t>::is_always_lock_free,
              "DashFrameShm::last_ack_time_us must be lock-free for safe use in shared memory");

/** Protocol-level magic strings for the dash_stream service.
 *  Shared between the DashStreamer client and the backend service handler. */
namespace dash_stream_protocol {

inline constexpr std::string_view k_service_name            = "dash_stream";
inline constexpr std::string_view k_cmd_request_buffer      = "request_buffer";
inline constexpr std::string_view k_cmd_release_buffer      = "release_buffer";
inline constexpr std::string_view k_cmd_stop_stream         = "stop_stream";

inline constexpr std::string_view k_field_device_session_id = "device_session_id";
inline constexpr std::string_view k_field_shm_path          = "shm_path";
inline constexpr std::string_view k_field_buffer_size       = "buffer_size";

}  // namespace dash_stream_protocol

}  // namespace sc_api::core

#endif  // SC_API_PROTOCOL_DASH_STREAM_H_
