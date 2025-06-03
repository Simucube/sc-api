/**
 * @file
 * @brief
 *
 */

#ifndef SC_API_CORE_VARIABLES_H
#define SC_API_CORE_VARIABLES_H

#include <atomic>
#include <cstdint>
#include <cstring>
#include <vector>

#include "compatibility.h"
#include "device.h"
#include "events.h"
#include "type.h"

namespace sc_api::core {

struct VariableReferenceBase {
    std::string_view name;
};

template <typename T>
struct VariableReference : VariableReferenceBase {
    using type                       = T;
    static constexpr Type type_value = get_base_type<T>::value;
};

/** Reference to a variable that never is part of device and has device_session_id == 0 */
template <typename T>
struct GlobalVariableReference : VariableReference<T> {};

/** Reference to a variable that always belongs to some device,. device_session_id != 0 */
template <typename T>
struct DeviceVariableReference : VariableReference<T> {};

template <typename T>
struct ArrayVariableReference : VariableReferenceBase {
    using type                       = T;
    static constexpr Type type_value = Type::Array(get_base_type<T>::value, 0);
};

template <typename T>
struct GlobalArrayVariableReference : VariableReference<T> {};

template <typename T>
struct DeviceArrayVariableReference : VariableReference<T> {};

/** Helper for accessing array variable value
 *
 * Array variable values are have revision counter field at the start of the value pointer
 * that can be used to detect modifications and handle getting value atomically
 */
template <typename T>
struct RevisionCountedArrayRef {
    RevisionCountedArrayRef(uint32_t size, const void* value_ptr)
        : array_size(size),
          rev_counter(*reinterpret_cast<const uint32_t*>(value_ptr)),
          value_array(reinterpret_cast<const T*>(reinterpret_cast<const uint8_t*>(value_ptr) + 8)) {}

    /** Number of elements in value_array
     *
     * Will stay same for the variable for the whole duration of the session
     */
    uint32_t                 array_size;
    volatile const uint32_t& rev_counter;
    const T*                 value_array;

    bool atomicCopy(T* buf, std::size_t buf_size) const {
        buf_size            = (std::min)((std::size_t)array_size, buf_size);
        int timeout_counter = 100000;

        do {
            uint32_t start_rev_count = rev_counter;
            if (start_rev_count & 2) {
                // Backend is just modifying this array, spin to wait for it to complete
                compatibility::spinlockPauseInstr();
                continue;
            }
            std::atomic_thread_fence(std::memory_order_acquire);
            std::memcpy(buf, value_array, sizeof(T) * buf_size);
            std::atomic_thread_fence(std::memory_order_release);
            if (rev_counter == start_rev_count) {
                // Revision count didn't change during copying so the value should be valid
                return true;
            }

        } while (--timeout_counter > 0);
        return false;
    }

    std::vector<T> atomicCopy() const {
        std::vector<T> result(array_size);
        if (atomicCopy(result.data(), result.size())) {
            return result;
        }
        return {};
    }
};

class VariableDefinitions;

/** VariableDefinition allows most direct access to the shared memory data definition with least overhead
 *
 * All data is only guaranteed to be valid as long as the VariableDefinitions object, which was used to fetch this
 * definition, exists. Value pointer is guaranteed to stay valid as long as the session is alive.
 */
struct VariableDefinition {
    /** null-terminated name of the variable
     *
     * This is only guaranteed to stay valid as long as the VariableDefinitions object is alive
     */
    std::string_view name;

    /** Pointer to the value in the shared memory. Valid pointer as long as the session instance is alive. */
    const void*     value_ptr = nullptr;
    Type            type      = Type::invalid;
    uint16_t        flags     = 0;
    DeviceSessionId device_session_id;

    constexpr explicit operator bool() const { return value_ptr != nullptr; }
};

namespace internal {
class VariableProvider;
}

/** Warpper around variable definition data that allows easily getting list of all available variables
 *
 * Instance won't be updated or invalidated even if VariableProvider::updateDefinitions is called. VariableDefinitions
 * always represents the state as of calling VariableProvider::definitions
 *
 * Holds handle to the session so that all variable value pointers will stay valid even if session is lost.
 */
class VariableDefinitions {
    friend class internal::VariableProvider;

public:
    class iterator {
        friend class VariableDefinitions;

    public:
        iterator& operator++() {
            ++idx_;
            return *this;
        };

        iterator operator++(int) {
            iterator ret = *this;
            ++idx_;
            return ret;
        }

        bool operator==(const iterator& it) const { return idx_ == it.idx_; }
        bool operator!=(const iterator& it) const { return idx_ != it.idx_; }
        bool operator<(const iterator& it) const { return idx_ < it.idx_; }
        bool operator<=(const iterator& it) const { return idx_ <= it.idx_; }
        bool operator>(const iterator& it) const { return idx_ > it.idx_; }
        bool operator>=(const iterator& it) const { return idx_ >= it.idx_; }

        iterator& operator+=(int i) {
            idx_ += i;
            return *this;
        }
        iterator& operator-=(int i) {
            idx_ -= i;
            return *this;
        }

        iterator operator+(int i) {
            iterator ret = *this;
            return ret += i;
        }

        iterator operator-(int i) {
            iterator ret = *this;
            return ret -= i;
        }

        VariableDefinition operator*() const;

        const VariableDefinitions* getDefinitions() const { return defs_; }

    private:
        iterator(const VariableDefinitions* defs, uint32_t idx) : defs_(defs), idx_(idx) {}

        const VariableDefinitions* defs_ = nullptr;
        uint32_t                   idx_  = 0;
    };

    VariableDefinitions();
    VariableDefinitions(const VariableDefinitions&)            = default;
    VariableDefinitions(VariableDefinitions&&)                 = default;

    VariableDefinitions& operator=(const VariableDefinitions&) = default;
    VariableDefinitions& operator=(VariableDefinitions&&)      = default;

    iterator begin() const { return iterator(this, 0); }
    iterator end() const { return iterator(this, count_); }

    VariableDefinition operator[](uint32_t idx) const;

    uint32_t size() const { return count_; }

    /** Tries to find variable definition by name and device session id */
    VariableDefinition find(std::string_view name, DeviceSessionId device = k_invalid_device_session_id) const;
    VariableDefinition find(std::string_view name, Type type,
                            DeviceSessionId device = k_invalid_device_session_id) const;

    template <typename T>
    VariableDefinition find(const DeviceVariableReference<T>& ref, DeviceSessionId device) const {
        return find(ref.name, ref.type, device);
    }

    template <typename T>
    VariableDefinition find(const GlobalVariableReference<T>& ref) const {
        return find(ref.name, ref.type);
    }

    /** Find pointer to variable value by type, name and device session id
     *
     * @param type Type of the variable value
     * @param name Name of the variable
     * @param device_session_id Device session id of the device this variable is related or 0 if it is global scope
     *                          variable
     * @returns Direct pointer to the variable value in shared memory
     *          nullptr, if matching variable isn't found
     */
    const void* findValuePointer(Type type, const std::string_view& name,
                                 DeviceSessionId device_session_id = k_invalid_device_session_id) const;

    template <typename T>
    const T* findValuePointer(const std::string_view& name,
                              DeviceSessionId         device_session_id = k_invalid_device_session_id) {
        return reinterpret_cast<const T*>(findValuePointer(get_base_type<T>::value, name, device_session_id));
    }

    /** Find pointer to a variable using given reference and device session id
     *
     * @param ref Reference definition of the variable. @see variable_references.h
     * @param device_session_id Id of the device which variable should be fetched
     * @returns Direct pointer to the variable value in shared memory
     *          or nullptr if matching variable cannot be found
     */
    template <typename T>
    const T* findValuePointer(const DeviceVariableReference<T>& ref, DeviceSessionId device_session_id) {
        return reinterpret_cast<const T*>(findValuePointer(ref.type_value, ref.name, device_session_id));
    }

    /** Find pointer to a variable using given reference
     *
     * @param ref Reference definition of the variable. @see variable_references.h
     * @returns Direct pointer to the variable value in shared memory
     *          or nullptr if matching variable cannot be found
     */
    template <typename T>
    const T* findValuePointer(const GlobalVariableReference<T>& ref) {
        return reinterpret_cast<const T*>(findValuePointer(ref.type_value, ref.name));
    }

    std::shared_ptr<Session> getSession() const { return session_; }

private:
    struct VariableDefChunk;

    VariableDefinitions(std::shared_ptr<VariableDefChunk> chunk, std::shared_ptr<Session> session);

    std::shared_ptr<VariableDefChunk> def_chunk_;
    std::shared_ptr<Session>          session_;
    uint32_t                          count_;
};

}  // namespace sc_api

#endif  // SC_API_CORE_VARIABLES_H
