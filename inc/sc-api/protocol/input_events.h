/**
 * @file
 * @brief Shared memory layout and codec of the input event ring.
 *
 * These definitions describe the protocol. Application code uses sc-api/input_events.h instead.
 *
 * The block holds one ring of fixed size event records. The backend is the only writer. Any number
 * of reader processes consume the ring with private cursors. The writer never waits on a reader, so
 * a reader that falls a full ring behind loses events and resyncs to the live edge.
 *
 * The codec is pure functions over a caller provided buffer. It does not map, allocate or free.
 */

#ifndef SC_API_PROTOCOL_INPUT_EVENTS_H_
#define SC_API_PROTOCOL_INPUT_EVENTS_H_

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "core.h"

#define SC_API_PROTOCOL_INPUT_EVENT_RING_SHM_ID      0x8c41b7d5u
#define SC_API_PROTOCOL_INPUT_EVENT_RING_SHM_VERSION 0x00000001u

namespace sc_api {

/** Cache line size that separates the ring parameters from the commit counter. Pinned ABI constant:
 *  the layout must be byte identical in the separately compiled client and backend, so this is
 *  hardcoded and not std::hardware_destructive_interference_size. */
inline constexpr std::size_t k_input_event_ring_cache_align     = 64;

/** Offset of the first slot from the start of the block. */
inline constexpr std::uint32_t k_input_event_ring_slot_offset   = 128;

/** Capacity that the v1 backend formats the ring with. */
inline constexpr std::uint32_t k_input_event_ring_capacity      = 4096;

/** Accepted capacity range. A power of two in this range keeps capacity x record_size below 2^32. */
inline constexpr std::uint32_t k_input_event_ring_min_capacity  = 64;
inline constexpr std::uint32_t k_input_event_ring_max_capacity  = 65536;

/** Accepted record stride range. The stride grows in multiples of 32 bytes. */
inline constexpr std::uint32_t k_input_event_record_granularity = 32;
inline constexpr std::uint32_t k_input_event_min_record_size    = 32;
inline constexpr std::uint32_t k_input_event_max_record_size    = 4096;

/** Kind of an input event. Append only: a value is never reused or given a new meaning, and the
 *  payload layout of a value never changes. A reader ignores values it does not know. */
enum class InputEventType : std::uint16_t {
    /** Not a valid event. */
    invalid         = 0,
    /** The input went from released to pressed. Empty payload. */
    button_pressed  = 1,
    /** The input went from pressed to released. Empty payload. */
    button_released = 2,
};

/** True for the event types of this API version. */
constexpr bool isKnownInputEventType(InputEventType type) {
    return type == InputEventType::button_pressed || type == InputEventType::button_released;
}

/** Value of `InputEvent::hid_index` when the input has no HID input. */
inline constexpr std::uint16_t k_input_event_no_hid_index = 0xffff;

/** One event record. Events carry absolute state, never a toggle. */
struct InputEvent {
    /** PC-side time of the event in sc_api::Clock nanoseconds. Events processed together can share
     *  a timestamp. 0 is not a valid timestamp. */
    std::int64_t timestamp;

    /** Device that owns the input in device_info. For a wireless wheel this is the wheel, not the
     *  SC-link Hub. */
    std::uint16_t device_session_id;

    /** InputEventType. Values outside the enum are reachable and must be ignored. */
    InputEventType type;

    /** Identifies the input within the device. The meaning is scoped to `type`. For the button
     *  types it is the `event_id` of an `Input` of `device_session_id` in device_info. It does not
     *  change when the user remaps the button. */
    std::uint16_t input_id;

    /** The HID input that the input was mapped to when the event happened, or
     *  `k_input_event_no_hid_index`. For the button types: the 0-based index into
     *  `DeviceInfo::getHidButtonInput()` of the event's device. If that list is empty, use the list
     *  of the device that `getParentSessionId()` names: the SC-link Hub reports the buttons of a
     *  wireless wheel. */
    std::uint16_t hid_index;

    /** Payload of `type`. */
    std::uint8_t payload[16];
};

static_assert(sizeof(InputEvent) == 32, "InputEvent must be exactly 32 bytes");
static_assert(alignof(InputEvent) == 8, "InputEvent must be 8-byte aligned so its slot words are aligned atomics");
// Field offsets are part of the ABI.
static_assert(offsetof(InputEvent, timestamp) == 0, "timestamp must be the first field");
static_assert(offsetof(InputEvent, device_session_id) == 8, "device_session_id offset is pinned");
static_assert(offsetof(InputEvent, type) == 10, "type offset is pinned");
static_assert(offsetof(InputEvent, input_id) == 12, "input_id offset is pinned");
static_assert(offsetof(InputEvent, hid_index) == 14, "hid_index offset is pinned");
static_assert(offsetof(InputEvent, payload) == 16, "payload offset is pinned");

/** Size of the record this build reads and writes. The stride in the block can differ. */
inline constexpr std::uint32_t k_input_event_size           = static_cast<std::uint32_t>(sizeof(InputEvent));

/** Slots are read and written as arrays of 64-bit words. */
inline constexpr std::uint32_t k_input_event_slot_word_size = static_cast<std::uint32_t>(sizeof(std::uint64_t));

/** Number of words in an InputEvent. Tracks sizeof(InputEvent), never the block stride. */
inline constexpr std::uint32_t k_input_event_word_count     = k_input_event_size / k_input_event_slot_word_size;

/** Header of the input event ring block. The slots follow at k_input_event_ring_slot_offset.
 *  Event e lives in slot e & (capacity - 1). */
struct alignas(k_input_event_ring_cache_align) InputEventRingShm {
    SC_API_PROTOCOL_ShmBlockHeader_t header;

    /** Slot stride in bytes. Readers MUST step by this, never by sizeof(InputEvent). */
    std::uint32_t record_size;

    /** Number of slots. Power of two. */
    std::uint32_t capacity;

    /** Pads the parameter group out to the commit counter cache line. Write zero. */
    std::uint32_t reserved_[11];

    /** Count of committed events. The only sequence counter of the ring. */
    alignas(k_input_event_ring_cache_align) std::atomic<std::uint64_t> write_seq;
};

static_assert(sizeof(InputEventRingShm) == k_input_event_ring_slot_offset, "Slots must start right after the header");
static_assert(alignof(InputEventRingShm) == 64, "InputEventRingShm must be 64-byte aligned");
static_assert(std::atomic<std::uint64_t>::is_always_lock_free,
              "The ring needs lock-free 64-bit atomics: a lock in shared memory is not usable across processes");
// Slots are indexed as arrays of atomic words, so the word must be exactly the integer it holds.
// Lock freedom alone does not promise that.
static_assert(sizeof(std::atomic<std::uint64_t>) == 8 && alignof(std::atomic<std::uint64_t>) == 8,
              "An atomic slot word must have the size and alignment of uint64_t");
static_assert(offsetof(InputEventRingShm, header) == 0, "The standard block header must come first");
static_assert(offsetof(InputEventRingShm, record_size) == 12, "record_size offset is pinned");
static_assert(offsetof(InputEventRingShm, capacity) == 16, "capacity offset is pinned");
static_assert(offsetof(InputEventRingShm, write_seq) == 64, "write_seq must own its own cache line");

/** Block size in bytes for a ring of `capacity` slots of `record_size` bytes. */
constexpr std::uint64_t inputEventRingBlockSize(std::uint32_t capacity,
                                                std::uint32_t record_size = k_input_event_size) {
    return static_cast<std::uint64_t>(k_input_event_ring_slot_offset) +
           static_cast<std::uint64_t>(capacity) * static_cast<std::uint64_t>(record_size);
}

/** Validated read-only view of a ring. capacity and record_size are latched at open time and are
 *  never read again, because the backend can write session shared memory for the life of the
 *  session. */
struct InputEventRingReadView {
    const std::atomic<std::uint64_t>* write_seq   = nullptr;
    const std::uint8_t*               slot_area   = nullptr;
    std::uint32_t                     capacity    = 0;
    std::uint32_t                     mask        = 0;
    std::uint32_t                     record_size = 0;

    bool isValid() const { return write_seq != nullptr; }
};

/** Validated writable view of a ring. Only the backend holds one. */
struct InputEventRingWriteView {
    std::atomic<std::uint64_t>* write_seq   = nullptr;
    std::uint8_t*               slot_area   = nullptr;
    std::uint32_t               capacity    = 0;
    std::uint32_t               mask        = 0;
    std::uint32_t               record_size = 0;

    bool isValid() const { return write_seq != nullptr; }
};

/** Result of one read call. */
struct InputEventReadResult {
    /** Events written to the caller array, oldest first. */
    std::uint32_t count = 0;

    /** Events skipped before those. They are never delivered. Saturates at UINT32_MAX, so the exact
     *  value is for diagnostics only. */
    std::uint32_t lost  = 0;
};

namespace detail {

/** Ring parameters that survived validation. */
struct InputEventRingLayout {
    std::uint32_t capacity    = 0;
    std::uint32_t record_size = 0;
    bool          valid       = false;
};

/** Ring parameters that this build accepts, whatever wrote them. */
constexpr bool inputEventRingParamsValid(std::uint32_t capacity, std::uint32_t record_size) {
    return capacity >= k_input_event_ring_min_capacity && capacity <= k_input_event_ring_max_capacity &&
           (capacity & (capacity - 1u)) == 0u && record_size >= k_input_event_min_record_size &&
           record_size <= k_input_event_max_record_size && (record_size % k_input_event_record_granularity) == 0u;
}

/** Address of a slot as an array of atomic words. Shared memory holds no C++ objects across
 *  processes, so the words are reached by offset. The stride is a multiple of 32 and the slot area
 *  starts at 128, so every slot base is 8-byte aligned. */
inline std::atomic<std::uint64_t>* inputEventRingSlot(std::uint8_t* slot_area, std::uint32_t index,
                                                      std::uint32_t record_size) {
    return reinterpret_cast<std::atomic<std::uint64_t>*>(slot_area + static_cast<std::size_t>(index) * record_size);
}

inline const std::atomic<std::uint64_t>* inputEventRingSlot(const std::uint8_t* slot_area, std::uint32_t index,
                                                            std::uint32_t record_size) {
    return reinterpret_cast<const std::atomic<std::uint64_t>*>(slot_area +
                                                               static_cast<std::size_t>(index) * record_size);
}

/** True if the block starts on a cache line. A shared memory mapping is page aligned, so only a
 *  synthetic buffer can fail this. */
inline bool inputEventRingBufferAligned(const void* buffer) {
    return (reinterpret_cast<std::uintptr_t>(buffer) % k_input_event_ring_cache_align) == 0u;
}

/** Check the block header before any slot is touched. */
inline InputEventRingLayout validateInputEventRing(const void* buffer, std::size_t buffer_size) {
    InputEventRingLayout layout;
    if (buffer == nullptr || !inputEventRingBufferAligned(buffer) || buffer_size < sizeof(InputEventRingShm)) {
        return layout;
    }

    const InputEventRingShm* ring = reinterpret_cast<const InputEventRingShm*>(buffer);

    // The backend bumps data_revision_counter to 2 once, after the parameters are in place. It is a
    // readiness flag here, not a seqlock: the ring has its own commit counter.
    if (ring->header.data_revision_counter < 2) {
        return layout;
    }

    // The readiness flag is published behind a release fence, so the parameters behind it are read
    // behind an acquire.
    std::atomic_thread_fence(std::memory_order_acquire);

    if (!SC_API_PROTOCOL_IS_SHM_VERSION_COMPATIBLE(SC_API_PROTOCOL_INPUT_EVENT_RING_SHM_VERSION,
                                                   ring->header.version)) {
        return layout;
    }

    // Another process writes the mapping, so each parameter is fetched once into a local copy and
    // the checks below run on the copy. A plain re-read of the field can return a value that the
    // checks never saw.
    std::uint32_t record_size;
    std::uint32_t capacity;
    std::uint32_t shm_size;
    std::memcpy(&record_size, &ring->record_size, sizeof(record_size));
    std::memcpy(&capacity, &ring->capacity, sizeof(capacity));
    std::memcpy(&shm_size, &ring->header.shm_size, sizeof(shm_size));

    if (!inputEventRingParamsValid(capacity, record_size)) {
        return layout;
    }

    // The two ceilings keep this product below 2^32, but the sum is still formed in uint64_t
    // so a hostile header cannot wrap it past the mapping bounds.
    const std::uint64_t needed = inputEventRingBlockSize(capacity, record_size);
    if (shm_size > buffer_size || needed > shm_size) {
        return layout;
    }

    layout.capacity    = capacity;
    layout.record_size = record_size;
    layout.valid       = true;
    return layout;
}

}  // namespace detail

/** Open a ring for reading. Returns an invalid view if the header does not pass the checks. */
inline InputEventRingReadView inputEventRingOpen(const void* buffer, std::size_t buffer_size) {
    InputEventRingReadView             view;
    const detail::InputEventRingLayout layout = detail::validateInputEventRing(buffer, buffer_size);
    if (!layout.valid) {
        return view;
    }

    const InputEventRingShm* ring = reinterpret_cast<const InputEventRingShm*>(buffer);

    view.write_seq                = &ring->write_seq;
    view.slot_area                = reinterpret_cast<const std::uint8_t*>(buffer) + k_input_event_ring_slot_offset;
    view.capacity                 = layout.capacity;
    view.mask                     = layout.capacity - 1u;
    view.record_size              = layout.record_size;
    return view;
}

/** Open an already formatted ring for writing. Returns an invalid view if the header does not pass
 *  the checks. */
inline InputEventRingWriteView inputEventRingOpenForWrite(void* buffer, std::size_t buffer_size) {
    InputEventRingWriteView            view;
    const detail::InputEventRingLayout layout = detail::validateInputEventRing(buffer, buffer_size);
    if (!layout.valid) {
        return view;
    }

    InputEventRingShm* ring = reinterpret_cast<InputEventRingShm*>(buffer);

    view.write_seq          = &ring->write_seq;
    view.slot_area          = reinterpret_cast<std::uint8_t*>(buffer) + k_input_event_ring_slot_offset;
    view.capacity           = layout.capacity;
    view.mask               = layout.capacity - 1u;
    view.record_size        = layout.record_size;
    return view;
}

/** Write the header of a fresh block and open it for writing. The slot area is left as the caller
 *  gave it, because a cursor starts at the live edge and no reader ever sees a slot that the writer
 *  has not filled. The buffer must start on a 64-byte boundary. Returns an invalid view if the
 *  parameters do not fit the buffer. */
inline InputEventRingWriteView inputEventRingFormat(void* buffer, std::size_t buffer_size, std::uint32_t capacity,
                                                    std::uint32_t record_size = k_input_event_size) {
    // The parameters are checked before anything is written, so a rejected call leaves the block
    // exactly as it was.
    if (buffer == nullptr || !detail::inputEventRingBufferAligned(buffer) ||
        !detail::inputEventRingParamsValid(capacity, record_size) ||
        inputEventRingBlockSize(capacity, record_size) > buffer_size) {
        return InputEventRingWriteView{};
    }

    InputEventRingShm* ring            = reinterpret_cast<InputEventRingShm*>(buffer);

    ring->header.version               = SC_API_PROTOCOL_INPUT_EVENT_RING_SHM_VERSION;
    ring->header.data_revision_counter = 0;
    ring->header.shm_size              = static_cast<std::uint32_t>(inputEventRingBlockSize(capacity, record_size));
    ring->record_size                  = record_size;
    ring->capacity                     = capacity;
    std::memset(ring->reserved_, 0, sizeof(ring->reserved_));
    ring->write_seq.store(0, std::memory_order_relaxed);

    // Readiness is published last, so a reader that sees the flag sees the parameters behind it.
    std::atomic_thread_fence(std::memory_order_release);
    ring->header.data_revision_counter = 2;

    return inputEventRingOpenForWrite(buffer, buffer_size);
}

/** Commit one event. Single writer only.
 *
 *  The step order and the memory orders of this function and of inputEventRingRead are the
 *  protocol. Do not change them.
 *
 *  1. The release fence keeps the slot stores below the commit store of the previous event. A
 *     release store only holds earlier accesses above itself. Without this fence the stores that
 *     overwrite a slot can become visible before the counter says that the slot is being reused. A
 *     reader then delivers a torn record instead of detecting the overrun.
 *  2. The word stores are relaxed. They carry no order of their own.
 *  3. The commit store is the release that publishes the slot to the acquire load of the reader. */
inline void inputEventRingWrite(const InputEventRingWriteView& ring, const InputEvent& event) {
    if (!ring.isValid()) {
        return;
    }

    // Relaxed is enough: the writer is the only one that advances this counter.
    const std::uint64_t         w = ring.write_seq->load(std::memory_order_relaxed);
    std::atomic<std::uint64_t>* slot =
        detail::inputEventRingSlot(ring.slot_area, static_cast<std::uint32_t>(w & ring.mask), ring.record_size);

    std::uint64_t words[k_input_event_word_count];
    std::memcpy(words, &event, sizeof(event));

    const std::uint32_t copy_words =
        (ring.record_size < k_input_event_size ? ring.record_size : k_input_event_size) / k_input_event_slot_word_size;
    const std::uint32_t stride_words = ring.record_size / k_input_event_slot_word_size;

    std::atomic_thread_fence(std::memory_order_release);
    for (std::uint32_t i = 0; i < copy_words; ++i) {
        slot[i].store(words[i], std::memory_order_relaxed);
    }
    // A stride wider than the record keeps zero in the part this build does not know, because zero
    // is the absent value of every field that a later stride bump adds.
    for (std::uint32_t i = copy_words; i < stride_words; ++i) {
        slot[i].store(0, std::memory_order_relaxed);
    }
    ring.write_seq->store(w + 1u, std::memory_order_release);
}

/** Sequence number that the next committed event gets. A cursor set to this value delivers no
 *  history. The acquire also anchors a baseline that the caller reads after this call. */
inline std::uint64_t inputEventRingLiveEdge(const InputEventRingReadView& ring) {
    if (!ring.isValid()) {
        return 0;
    }
    return ring.write_seq->load(std::memory_order_acquire);
}

/** Copy up to `max_events` events into `out`, oldest first, and advance `cursor`. Never blocks.
 *  A call with `max_events` 0 still runs the overrun check and can move `cursor` to the live edge.
 *
 *  Every returned event is newer than every skipped event, so a skip found in the middle of a scan
 *  ends the call and the next call reports it.
 *
 *  The step order and the memory orders are the protocol. Do not change them:
 *
 *  - W1 is acquired first. It bounds the batch, and its acquire covers every event below it,
 *    including the words of the slot the reader is about to copy.
 *  - The window is strict. At a gap of exactly `capacity` the writer can already be inside the
 *    reader's slot, so the usable depth is capacity - 1.
 *  - copy, acquire fence, W2 cannot be reordered against each other. The fence pairs with the
 *    release fence of the writer and is what makes a torn copy visible.
 *  - W2 is relaxed and is never a bound of the batch. It only answers whether the writer passed the
 *    copied slot while the copy was running. */
inline InputEventReadResult inputEventRingRead(const InputEventRingReadView& ring, std::uint64_t& cursor,
                                               InputEvent* out, std::uint32_t max_events) {
    InputEventReadResult result;
    if (!ring.isValid() || (out == nullptr && max_events > 0)) {
        return result;
    }

    const std::uint64_t w1 = ring.write_seq->load(std::memory_order_acquire);
    if (cursor == w1) {
        return result;
    }

    if (w1 - cursor >= ring.capacity) {
        const std::uint64_t lost = w1 - cursor;
        result.lost              = lost > UINT32_MAX ? UINT32_MAX : static_cast<std::uint32_t>(lost);
        cursor                   = w1;
        return result;
    }

    // A stride narrower than the record leaves the tail of the copy zero, because zero is the absent
    // value of every field that the block does not carry.
    const std::uint32_t copy_words =
        (ring.record_size < k_input_event_size ? ring.record_size : k_input_event_size) / k_input_event_slot_word_size;

    while (result.count < max_events && cursor != w1) {
        const std::atomic<std::uint64_t>* slot = detail::inputEventRingSlot(
            ring.slot_area, static_cast<std::uint32_t>(cursor & ring.mask), ring.record_size);

        std::uint64_t words[k_input_event_word_count] = {};
        for (std::uint32_t i = 0; i < copy_words; ++i) {
            words[i] = slot[i].load(std::memory_order_relaxed);
        }

        std::atomic_thread_fence(std::memory_order_acquire);
        const std::uint64_t w2 = ring.write_seq->load(std::memory_order_relaxed);
        if (w2 - cursor >= ring.capacity) {
            // The writer reached this slot during the copy. Drop it and do not advance, so the overrun
            // check of the next call reports the loss.
            break;
        }

        std::memcpy(&out[result.count], words, sizeof(InputEvent));
        ++result.count;
        ++cursor;
    }

    return result;
}

}  // namespace sc_api

#endif  // SC_API_PROTOCOL_INPUT_EVENTS_H_
