/**
 * @file
 * @brief Read access to variable values in shared memory.
 *
 * Variables are the values that change often: device state, such as pedal force and position,
 * and the active telemetry values. They are read-only for the API user.
 *
 * Session::getVariables returns a VariableDefinitions snapshot. Each VariableDefinition holds
 * a direct pointer into shared memory, so a read costs one dereference and no command.
 *
 * The backend writes each variable atomically. It does not sample different variables at the
 * same instant. Two values can therefore be up to 2 ms apart.
 *
 * Variable definitions are session specific. During one session the backend only adds
 * definitions, and it never changes or removes an existing one.
 *
 * @see sc-api/variable_references.h for the names of the known variables.
 * @see examples/variable_definitions.cpp and examples/pedal_state.cpp for complete programs.
 */

#ifndef SC_API_VARIABLES_H
#define SC_API_VARIABLES_H

#include <atomic>
#include <cstdint>
#include <cstring>
#include <type_traits>
#include <vector>

#include "compatibility.h"
#include "device.h"
#include "events.h"
#include "type.h"

namespace sc_api {

/** Name of a known variable, without a value type */
struct VariableReferenceBase {
    std::string_view name;
};

/** Name and value type of a known variable
 *
 * variable_references.h holds the references for the known variables. A typed reference removes
 * the need to give the type at every lookup.
 */
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
 * An array variable value starts with a revision counter field. The counter detects a write by
 * the backend and therefore makes an atomic read of the whole array possible.
 *
 * Use atomicCopy to read the array. A direct read of value_array can catch a partial write.
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

    /** Counter that the backend increases before and after every write to the array
     *
     * An odd value therefore means that a write is in progress, and an even value means that the
     * array is idle. atomicCopy uses this to detect a concurrent write.
     */
    volatile const uint32_t& rev_counter;

    /** Direct pointer to the array in shared memory. Prefer atomicCopy over a direct read */
    const T* value_array;

    /** Copy the array into buf without a concurrent write
     *
     * Copies the smaller of array_size and buf_size elements.
     *
     * @return true, if the copy succeeded
     *         false, if the backend kept the array busy for the whole retry limit
     */
    bool atomicCopy(T* buf, std::size_t buf_size) const {
        buf_size            = (std::min)((std::size_t)array_size, buf_size);
        int timeout_counter = 100000;

        do {
            uint32_t start_rev_count = rev_counter;
            if (start_rev_count & 1) {
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

    /** Copy the array into a new vector
     *
     * @return The array contents, or an empty vector if the copy failed
     */
    std::vector<T> atomicCopy() const {
        std::vector<T> result(array_size);
        if (atomicCopy(result.data(), result.size())) {
            return result;
        }
        return {};
    }
};

/** Helper for knowing if type is RevisionCounterArrayRef */
template <typename T>
struct TypeIsRevisionCountedArrayRef : std::false_type {};
template <typename T>
struct TypeIsRevisionCountedArrayRef<RevisionCountedArrayRef<T>> : std::true_type {};

template <typename T>
constexpr bool isRevisionCountedArrayRef(const T& v) {
    return TypeIsRevisionCountedArrayRef<T>::value;
}

/** Call f with the variable value read from ptr as the type described by type
 *
 * Bit types are passed as bool, array types as RevisionCountedArrayRef and unsupported types as nullptr.
 */
template <typename Fn>
auto invokeWithValueType(Type type, const void* ptr, Fn f) {
    if (type.isBaseType()) {
        switch (type.getBaseType()) {
            case Type::boolean:
                return f(*(const bool*)ptr);

            case Type::i8:
                return f(*(const int8_t*)ptr);

            case Type::u8:
                return f(*(const uint8_t*)ptr);

            case Type::i16:
                return f(*(const int16_t*)ptr);

            case Type::u16:
                return f(*(const uint16_t*)ptr);

            case Type::i32:
                return f(*(const int32_t*)ptr);

            case Type::u32:
                return f(*(const uint32_t*)ptr);

            case Type::i64:
                return f(*(const int64_t*)ptr);

            case Type::f32:
                return f(*(const float*)ptr);

            case Type::f64:
                return f(*(const double*)ptr);

            case Type::cstring:
                return f((const char*)ptr);

            default:
                break;
        }
    } else if (type.isBit()) {
        switch (type.getBaseType()) {
            case Type::boolean:
            case Type::i8:
            case Type::u8:
                return f((*(const uint8_t*)ptr & (1u << type.getBitIndex())) != 0);

            case Type::i16:
            case Type::u16:
                return f((*(const uint16_t*)ptr & (1u << type.getBitIndex())) != 0);

            case Type::i32:
            case Type::u32:
                return f((*(const uint32_t*)ptr & (1u << type.getBitIndex())) != 0);

            case Type::i64:
                return f((*(const uint64_t*)ptr & (1ull << type.getBitIndex())) != 0);
            default:
                break;
        }
    } else if (type.isArray()) {
        switch (type.getBaseType()) {
            case Type::boolean:
                return f(RevisionCountedArrayRef<bool>(type.getArraySize(), ptr));

            case Type::i8:
                return f(RevisionCountedArrayRef<int8_t>(type.getArraySize(), ptr));

            case Type::u8:
                return f(RevisionCountedArrayRef<uint8_t>(type.getArraySize(), ptr));

            case Type::i16:
                return f(RevisionCountedArrayRef<int16_t>(type.getArraySize(), ptr));

            case Type::u16:
                return f(RevisionCountedArrayRef<uint16_t>(type.getArraySize(), ptr));

            case Type::i32:
                return f(RevisionCountedArrayRef<int32_t>(type.getArraySize(), ptr));

            case Type::u32:
                return f(RevisionCountedArrayRef<uint32_t>(type.getArraySize(), ptr));

            case Type::i64:
                return f(RevisionCountedArrayRef<int64_t>(type.getArraySize(), ptr));

            case Type::f32:
                return f(RevisionCountedArrayRef<float>(type.getArraySize(), ptr));

            case Type::f64:
                return f(RevisionCountedArrayRef<double>(type.getArraySize(), ptr));

            default:
                break;
        }
    }
    return f(nullptr);
}

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

namespace detail {
class VariableProvider;
}

/** Wrapper around variable definition data that allows easily getting list of all available variables
 *
 * An instance is a snapshot. It never changes after Session::getVariables returns it. To see
 * variables that were added later, call Session::getVariables again.
 *
 * The instance holds a handle to the session. Therefore all variable value pointers stay valid
 * even after the session is lost. The values then keep their last value.
 */
class VariableDefinitions {
    friend class detail::VariableProvider;

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

    /** Number of variable definitions in this snapshot */
    uint32_t size() const { return count_; }

    /** Tries to find variable definition by name and device session id
     *
     * Omit device to find a variable that does not belong to a device.
     *
     * @return The definition. Test it with operator bool, because a definition that is not
     *         found has a null value_ptr.
     */
    VariableDefinition find(std::string_view name, DeviceSessionId device = k_invalid_device_session_id) const;

    /** Tries to find variable definition by name, type and device session id
     *
     * @return The definition, or a definition with a null value_ptr if there is no match
     */
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

    /** Session that these definitions came from
     *
     * The value pointers stay valid as long as this session object exists.
     */
    std::shared_ptr<Session> getSession() const { return session_; }

private:
    struct VariableDefChunk;

    VariableDefinitions(std::shared_ptr<VariableDefChunk> chunk, std::shared_ptr<Session> session);

    std::shared_ptr<VariableDefChunk> def_chunk_;
    std::shared_ptr<Session>          session_;
    uint32_t                          count_;
};

}  // namespace sc_api

#endif  // SC_API_VARIABLES_H
