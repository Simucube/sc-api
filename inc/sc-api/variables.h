/**
 * @file
 * @brief
 *
 */

#ifndef SC_API_INTERNAL_VARIABLES_H_
#define SC_API_INTERNAL_VARIABLES_H_
#include <sc-api/core/variable_references.h>
#include <sc-api/core/variables.h>

namespace sc_api {

using core::Type;

using core::RevisionCountedArrayRef;
using core::VariableDefinition;
using core::VariableDefinitions;

/** Helper for knowing if type is RevisionCounterArrayRef */
template <typename T>
struct TypeIsRevisionCountedArrayRef : std::false_type {};
template <typename T>
struct TypeIsRevisionCountedArrayRef<RevisionCountedArrayRef<T>> : std::true_type {};

template <typename T>
constexpr bool isRevisionCountedArrayRef(const T& v) {
    return TypeIsRevisionCountedArrayRef<T>::value;
}

template <typename Fn>
auto invokeWithValueType(sc_api::core::Type type, const void* ptr, Fn f) {
    using sc_api::core::Type;
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

}  // namespace sc_api

#endif  // SC_API_INTERNAL_VARIABLES_H_
