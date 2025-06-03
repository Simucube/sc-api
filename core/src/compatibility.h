/**
 * @file
 * @brief
 *
 */

#ifndef SC_API_INTERNAL_COMPATIBILITYR_H_
#define SC_API_INTERNAL_COMPATIBILITYR_H_
#include <atomic>
#include <memory>

#include "sc-api/core/protocol/core.h"

namespace sc_api::core::internal {

void alignedFree(void* buf);

void* alignedAlloc(std::size_t alignment, std::size_t size);

struct AlignedDeleter {
    void operator()(void* buf) const { alignedFree(buf); }
};

template <typename T>
using AlignedUniquePtr = std::unique_ptr<T, AlignedDeleter>;

class SharedMemory {
public:
    SharedMemory() noexcept;
    ~SharedMemory();
    SharedMemory(SharedMemory&& s) noexcept;

    SharedMemory& operator=(SharedMemory&& s) noexcept;

    bool openForReadOnly(const char* path, uint32_t size);
    bool openForReadWrite(const char* path, std::size_t required_min_size);
    bool createForReadWrite(const char* path, std::size_t size);
    bool openOrCreateForReadWrite(const char* path, std::size_t size);
    void close();

    bool isOpen() const { return shm_buffer_ != nullptr; }

    void* getBuffer() const { return shm_buffer_; }

    uint32_t getSize() const { return size_; }

protected:
    bool mapBufferOrClose(std::size_t size, uint32_t access);

    void*    shm_handle_;
    void*    shm_buffer_;
    uint32_t size_ = 0;
};

class ShmBlock {
public:
    ShmBlock();
    ~ShmBlock();

    bool open(const SC_API_PROTOCOL_ShmBlockReference_t& reference);
    void close();

    bool isHeaderInitialized() const;

    /** Helper to try to execute some code with atomic access to the buffer
     *
     * @param f Functor bool(const void* buf, uint32_t size) that should be executed with atomic access.
     *          Does not guarantee atomic access, but only detects if the atomic wasn't actually atomic.
     *          May not be called at all, if the shared memory was just under modification
     *          when this function is called.
     * @return true, if atomic access succeeded, f returned true and  and the function was executed with consistent data
     */
    template <typename Func>
    bool tryAtomicDataAccess(Func&& f) const {
        SC_API_PROTOCOL_ShmBlockHeader_t hdr       = *shm_buffer_;
        uint32_t                         start_rev = hdr.data_revision_counter;
        std::atomic_thread_fence(std::memory_order_acquire);

        if (start_rev & 1 || start_rev < 2) {
            return false;
        }

        if (!f(shm_buffer_, hdr.shm_size)) {
            return false;
        }
        std::atomic_thread_fence(std::memory_order_acq_rel);
        uint32_t end_rev = shm_buffer_->data_revision_counter;

        return start_rev == end_rev;
    }

    const void* getBuffer() const { return shm_.getBuffer(); }

    uint32_t getSize() const { return shm_.getSize(); }

private:
    SharedMemory                            shm_;
    const SC_API_PROTOCOL_ShmBlockHeader_t* shm_buffer_;
};

/** Helper to try to execute some code with atomic access to the buffer
 *
 * @param f Functor bool(const void* buf, uint32_t size) that should be executed with atomic access.
 *          Does not guarantee atomic access, but only detects if the atomic wasn't actually atomic.
 *          May not be called at all, if the shared memory was just under modification
 *          when this function is called.
 * @return true, if atomic access succeeded, f returned true and the function was executed with consistent data
 */
template <typename Func>
static bool shmTryAtomicDataAccess(const SC_API_PROTOCOL_ShmBlockHeader_t* shm_buffer, Func&& f) {
    SC_API_PROTOCOL_ShmBlockHeader_t hdr       = *shm_buffer;
    uint32_t                         start_rev = hdr.data_revision_counter;
    std::atomic_thread_fence(std::memory_order_acquire);

    if (start_rev & 1 || start_rev < 2) {
        return false;
    }

    if (!f(shm_buffer, hdr.shm_size)) {
        return false;
    }
    std::atomic_thread_fence(std::memory_order_acq_rel);
    uint32_t end_rev = shm_buffer->data_revision_counter;

    return start_rev == end_rev;
}

uint32_t getCurrentProcessId();

}  // namespace sc_api::core::internal

#endif  // SC_API_INTERNAL_COMPATIBILITYR_H_
