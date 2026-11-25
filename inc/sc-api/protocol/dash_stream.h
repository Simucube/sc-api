/**
 * @file
 * @brief Shared memory layout of the dashboard frame stream.
 *
 * These definitions describe the protocol. Application code uses sc-api/dash_stream.h instead.
 */

#ifndef SC_API_PROTOCOL_DASH_STREAM_H_
#define SC_API_PROTOCOL_DASH_STREAM_H_

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <type_traits>

#include "sc-api/touch_areas.h"

namespace sc_api {

/** Cache-line size used to separate the two writer domains in DashFrameShm onto distinct cache
 *  lines (avoids false sharing). Pinned ABI constant: the struct layout must be byte-identical
 *  across the separately-compiled client and backend, so this is hardcoded - NOT
 *  std::hardware_destructive_interference_size, whose value can differ between translation units. */
inline constexpr std::size_t k_dash_shm_cache_align = 64;

inline constexpr int32_t k_dash_frame_shm_version   = 0x01;

/** Offset of the RGB565 pixel data from the start of the frame buffer. */
inline constexpr std::size_t k_dash_frame_pixel_offset = 128;

/** Shared memory frame layout for dash streaming. Followed by frame pixel data (RGB565) starting
 *  at k_dash_frame_pixel_offset.
 *
 *  Two independent writers share this cross-process region:
 *    - the client writes the first cache line (the backend only advances revision there) and the pixel data;
 *    - the backend writes the feedback group every ~1 ms. */
struct alignas(k_dash_shm_cache_align) DashFrameShm {
    /** Must be written to k_dash_frame_shm_version */
    int32_t version;
    /** Compatibility flags. Write zero*/
    uint32_t feature_flags;

    /** Frame handshake - client writes, backend reads. `revision` is the publish handshake:
     * even = slot free for the client to write, odd = a new frame is published and not yet
     * consumed by the backend (backend bumps it back to even on copy-out). */
    std::atomic<uint32_t> revision;

    /** Position offset of this stream on the screen.
     *
     *  Currently partial streaming is not supported so these should be zero
     */
    uint16_t offset_x;
    uint16_t offset_y;

    /** Width of the frame data in pixels */
    uint16_t width;

    /** Height of the frame data in pixels */
    uint16_t height;

    /** Number of valid entries in mouse_suppressed_areas, at most k_max_mouse_suppressed_areas. Clients before
     *  0.7.0 zero this word as part of reserved_ at open, so 0 means a frame without areas. */
    uint32_t mouse_suppressed_area_count;

    /** Areas of this frame where a touch does not move the mouse */
    TouchArea mouse_suppressed_areas[k_max_mouse_suppressed_areas];

    /** Frame rate the client asks for with this frame, 0 = no target. Clients before 0.8.0 zero
     *  this field as part of reserved_ at open. */
    uint16_t target_fps;

    /** Pads the client-written handshake group out to the 64-byte feedback cache line. The alignas
     *  below would insert the same padding on its own; keeping it explicit reserves space for future
     *  handshake fields (shrink this array when adding one, to keep sizeof stable). Write zero. */
    uint8_t reserved_[6];

    alignas(k_dash_shm_cache_align) std::atomic<uint64_t> last_ack_time;
    std::atomic<uint32_t> is_owner;
    std::atomic<uint32_t> device_frame_counter;
    std::atomic<uint32_t> dropped_count;

    // This header is then followed by the actual pixel data in shared memory
};

// Pin SHM layout. Any accidental drift (alignment change, type swap, field
// reorder) trips the build on both client and backend.
static_assert(sizeof(DashFrameShm) == k_dash_frame_pixel_offset, "Pixel data must start right after the header");
static_assert(alignof(DashFrameShm) == 64, "DashFrameShm must be 64-byte aligned");
static_assert(std::atomic<uint32_t>::is_always_lock_free,
              "DashFrameShm 32-bit atomics must be lock-free for safe use in shared memory");
static_assert(std::atomic<uint64_t>::is_always_lock_free,
              "DashFrameShm::last_ack_time must be lock-free for safe use in shared memory");
// Field offsets are part of the cross-process ABI too: pin the load-bearing ones so a field reorder
// (which keeps sizeof at 128 and slips past the size check) can't silently corrupt reads on the
// other separately-compiled binary.
static_assert(offsetof(DashFrameShm, version) == 0, "version must be the first field");
static_assert(offsetof(DashFrameShm, revision) == 8, "revision offset is pinned");
static_assert(offsetof(DashFrameShm, width) == 16, "width offset is pinned");
static_assert(offsetof(DashFrameShm, mouse_suppressed_area_count) == 20, "area count offset is pinned");
static_assert(offsetof(DashFrameShm, mouse_suppressed_areas) == 24, "areas offset is pinned");
static_assert(sizeof(TouchArea) == 8, "TouchArea is part of the shared memory layout");
static_assert(std::is_trivially_copyable_v<TouchArea>, "TouchArea is copied in and out of shared memory");
static_assert(offsetof(TouchArea, x) == 0 && offsetof(TouchArea, y) == 2 && offsetof(TouchArea, w) == 4 &&
                  offsetof(TouchArea, h) == 6,
              "TouchArea member order is part of the shared memory layout");
static_assert(offsetof(DashFrameShm, target_fps) == 56, "target_fps offset is pinned");
static_assert(offsetof(DashFrameShm, reserved_) + sizeof(DashFrameShm::reserved_) ==
                  offsetof(DashFrameShm, last_ack_time),
              "the client handshake group must fill the first cache line");
static_assert(offsetof(DashFrameShm, last_ack_time) == 64, "feedback group must start on the second cache line");
static_assert(offsetof(DashFrameShm, is_owner) == 72, "is_owner offset is pinned");

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
// request_buffer arguments: the client's SHM ABI version (k_dash_frame_shm_version) and the pixel
// format it will write. The backend validates these at request time so an ABI/format mismatch
// fails fast with a clear error instead of silently dropping every frame.
inline constexpr std::string_view k_field_version           = "v";
inline constexpr std::string_view k_field_format            = "format";
inline constexpr std::string_view k_format_rgb565           = "rgb565";

}  // namespace dash_stream_protocol

}  // namespace sc_api

#endif  // SC_API_PROTOCOL_DASH_STREAM_H_
