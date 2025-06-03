#include "compatibility.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <winnt.h>
#else
#include <sys/types.h>
#include <unistd.h>
#define INVALID_HANDLE_VALUE nullptr
#endif
#include <cstdlib>

namespace sc_api::core::internal {

// NOLINTBEGIN(readability-convert-member-functions-to-static)

SharedMemory::SharedMemory() noexcept : shm_handle_(INVALID_HANDLE_VALUE), shm_buffer_(nullptr) {}

SharedMemory::~SharedMemory() {}

SharedMemory::SharedMemory(SharedMemory&& s) noexcept
    : shm_handle_(s.shm_handle_), shm_buffer_(s.shm_buffer_), size_(s.size_) {
    s.shm_handle_ = nullptr;
    s.shm_buffer_ = nullptr;
    s.size_       = 0;
}

SharedMemory& SharedMemory::operator=(SharedMemory&& s) noexcept {
    if (&s == this) return *this;

    std::swap(s.shm_buffer_, shm_buffer_);
    std::swap(s.shm_handle_, shm_handle_);
    std::swap(s.size_, size_);
    return *this;
}

bool SharedMemory::openForReadOnly(const char* path, uint32_t size) {
#ifdef _WIN32
    shm_handle_ = OpenFileMappingA(FILE_MAP_READ, FALSE, path);

    if (shm_handle_ == INVALID_HANDLE_VALUE) {
        return false;
    }

    return mapBufferOrClose(size, FILE_MAP_READ);
#else
    return false;
#endif
}

bool SharedMemory::openForReadWrite(const char *path, size_t size)
{
#ifdef _WIN32
    shm_handle_ = OpenFileMappingA(FILE_MAP_ALL_ACCESS, FALSE, path);
    if (shm_handle_ == nullptr) {
        // WinAPI plz... Why this returns NULL on error
        shm_handle_ = INVALID_HANDLE_VALUE;
    }

    if (shm_handle_ == INVALID_HANDLE_VALUE) {
        return false;
    }

    return mapBufferOrClose(size, FILE_MAP_ALL_ACCESS);
#else
    return false;
#endif
}

bool SharedMemory::createForReadWrite(const char *path, size_t size)
{
#ifdef _WIN32
    shm_handle_ = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, (uint32_t)((uint64_t)size >> 32),
                                     (uint32_t)(size & 0xffffffffu), path);
    if (shm_handle_ == nullptr) {
        // WinAPI plz... Why this returns NULL on error
        shm_handle_ = INVALID_HANDLE_VALUE;
    }

    if (shm_handle_ == INVALID_HANDLE_VALUE) {
        // volatile uint32_t last_error = GetLastError();
        // std::cerr << "CreateFileMappingA for " << path << " failed: " << last_error;
        return false;
    }

    return mapBufferOrClose(size, FILE_MAP_ALL_ACCESS);
#else
    return false;
#endif
}

bool SharedMemory::openOrCreateForReadWrite(const char *path, size_t size)
{
#ifdef _WIN32
    if (createForReadWrite(path, size)) {
        return true;
    }

    return openForReadWrite(path, size);
#else
    return false;
#endif
}

void SharedMemory::close()
{
#ifdef _WIN32
    if (shm_buffer_) {
        UnmapViewOfFile(shm_buffer_);
        shm_buffer_ = NULL;
    }

    if (shm_handle_) {
        CloseHandle(shm_handle_);
        shm_handle_ = INVALID_HANDLE_VALUE;
    }
    size_ = 0;
#endif
}

bool SharedMemory::mapBufferOrClose(size_t size, uint32_t access)
{
#ifdef _WIN32
    shm_buffer_ = MapViewOfFile(shm_handle_, access, 0, 0, size);
    if (!shm_buffer_) {
        // std::cerr << "MapViewOfFile failed: " << GetLastError();
        CloseHandle(shm_handle_);
        shm_handle_ = INVALID_HANDLE_VALUE;
        return false;
    }

    size_ = (uint32_t)size;
    return true;
#else
    return false;
#endif
}

ShmBlock::ShmBlock() {}

ShmBlock::~ShmBlock()
{
    close();
}

bool ShmBlock::open(const SC_API_PROTOCOL_ShmBlockReference_t& reference) {
    // Verify null termination
    if (reference.shm_path[sizeof(reference.shm_path) - 1] != '\0') {
        return false;
    }

    if (!shm_.openForReadOnly(reference.shm_path, reference.size)) {
        return false;
    }

    shm_buffer_ = reinterpret_cast<const SC_API_PROTOCOL_ShmBlockHeader_t *>(shm_.getBuffer());

    return true;
}

void ShmBlock::close()
{
    shm_.close();
    shm_buffer_ = nullptr;
}

bool ShmBlock::isHeaderInitialized() const
{
    return ((const SC_API_PROTOCOL_ShmBlockHeader_t *) shm_buffer_)->data_revision_counter >= 2;
}

uint32_t getCurrentProcessId()
{
#ifdef _WIN32
    return GetCurrentProcessId();
#else
    return getpid();
#endif
}

void* alignedAlloc(std::size_t alignment, std::size_t size) {
    size = (size + alignment - 1) & ~(alignment - 1);

// MSVC doesn't support std::aligned_alloc because its std::free implementation can't handle that
// https://en.cppreference.com/w/cpp/memory/c/aligned_alloc
#ifdef _MSC_VER
    return _aligned_malloc(size, alignment);
#else
    return std::aligned_alloc(alignment, size);
#endif
}

void alignedFree(void* buf) {
#ifdef _MSC_VER
    return _aligned_free(buf);
#else
    return std::free(buf);
#endif
}

// NOLINTEND(readability-convert-member-functions-to-static)

}  // namespace sc_api::core::internal
