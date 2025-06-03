/**
 * @file
 * @brief
 *
 */

#ifndef SC_API_CORE_TYPE_H_
#define SC_API_CORE_TYPE_H_

#include <string>

#include "sc-api/core/protocol/types.h"

namespace sc_api::core {

/** Represents type of a variable or telemetry value
 *
 * Type consist of the base type, variant type and possibly variant specific data.
 *
 */
struct Type {
    enum BaseType {
        invalid = SC_API_TYPE_INVALID,
        boolean = SC_API_TYPE_BOOL,
        i8      = SC_API_TYPE_I8,
        u8      = SC_API_TYPE_U8,
        i16     = SC_API_TYPE_I16,
        u16     = SC_API_TYPE_U16,
        i32     = SC_API_TYPE_I32,
        u32     = SC_API_TYPE_U32,
        i64     = SC_API_TYPE_I64,
        f32     = SC_API_TYPE_F32,
        f64     = SC_API_TYPE_F64,

        /** Always array type thats size defines maximum length of the string and is always constant and null terminated
         */
        cstring = SC_API_TYPE_CSTRING,
    };

    constexpr Type() : type(BaseType::invalid) {}
    explicit constexpr Type(SC_API_PROTOCOL_Type_t val, SC_API_PROTOCOL_TypeVariantData_t d)
        : type(val), variant_data(d) {}
    constexpr Type(BaseType type) : type(type) {}
    constexpr Type(BaseType base, uint32_t array_size) : type(SC_API_TYPE_ARRAY(base)) {}

    static constexpr Type Array(BaseType base, uint32_t array_size) {
        return Type(SC_API_TYPE_ARRAY(base), array_size);
    }

    static constexpr Type Bit(BaseType base, uint32_t bit_idx) { return Type(SC_API_TYPE_BIT(base), bit_idx); }

    SC_API_PROTOCOL_Type_t            type;
    SC_API_PROTOCOL_TypeVariantData_t variant_data = 0;

    constexpr bool isInvalid() const { return type == SC_API_TYPE_INVALID; }

    constexpr BaseType getBaseType() const { return (BaseType)SC_API_TYPE_GET_BASE_TYPE(type); }
    constexpr bool     isBaseType() const { return SC_API_TYPE_IS_BASE_TYPE(type); }

    constexpr bool     isArray() const { return SC_API_TYPE_IS_ARRAY(type); }
    constexpr uint32_t getArraySize() const { return variant_data; }

    constexpr bool isBit() const { return SC_API_TYPE_IS_BIT(type); }
    constexpr unsigned getBitIndex() const { return variant_data; }

    static constexpr uint32_t getBaseTypeByteSize(BaseType type) {
        switch (type) {
            case invalid:
                return 0;
            case i8:
            case u8:
                return 1;
            case i16:
            case u16:
                return 2;
            case i32:
            case u32:
            case f32:
                return 4;
            case f64:
            case i64:
                return 8;
            default:
                return 0;
        }
    }

    constexpr uint32_t getValueByteSize() const {
        switch (type) {
            case invalid:
                return 0;
            case i8:
            case u8:
                return 1;
            case i16:
            case u16:
                return 2;
            case i32:
            case u32:
            case f32:
                return 4;
            case f64:
            case i64:
                return 8;

            default:
                if (getBaseType() == BaseType::cstring) {
                    // cstring isn't synchronized so it is always same size as the number of elements
                    return getArraySize();
                }

                if (isArray()) {
                    // Arrays have 8 byte space reserved for revision counter
                    uint32_t element_size = getBaseTypeByteSize(getBaseType());
                    return element_size * getArraySize() + 8;
                }
                // Not supported?
                return 0;
        }
    }

    /** Textual representation of the type for debugging purposes */
    std::string             toString() const;
    static std::string_view baseTypeToString(BaseType type);

    constexpr bool operator==(Type b) const { return type == b.type && variant_data == b.variant_data; }
    constexpr bool operator!=(Type b) const { return type != b.type || variant_data != b.variant_data; }

    friend constexpr bool operator==(Type a, BaseType b) { return a.type == b && a.isBaseType(); }
    friend constexpr bool operator==(BaseType b, Type a) { return a.type == b && a.isBaseType(); }
    friend constexpr bool operator!=(Type a, BaseType b) { return a.type != b || !a.isBaseType(); }
    friend constexpr bool operator!=(BaseType b, Type a) { return a.type != b || !a.isBaseType(); }
};

/** Helper template for finding BaseType from C++ type */
template <typename T>
struct get_base_type {
    static_assert(sizeof(T) == 0, "Base type mapping isn't defined for this type");
    using type                            = T;
    static constexpr Type::BaseType value = Type::invalid;
};

template <>
struct get_base_type<int32_t> {
    using type                            = int32_t;
    static constexpr Type::BaseType value = Type::i32;
};
template <>
struct get_base_type<uint32_t> {
    using type                            = uint32_t;
    static constexpr Type::BaseType value = Type::u32;
};
template <>
struct get_base_type<int64_t> {
    using type                            = int64_t;
    static constexpr Type::BaseType value = Type::i64;
};
template <>
struct get_base_type<uint8_t> {
    using type                            = uint8_t;
    static constexpr Type::BaseType value = Type::u8;
};
template <>
struct get_base_type<int8_t> {
    using type                            = int8_t;
    static constexpr Type::BaseType value = Type::i8;
};
template <>
struct get_base_type<uint16_t> {
    using type                            = uint16_t;
    static constexpr Type::BaseType value = Type::u16;
};
template <>
struct get_base_type<int16_t> {
    using type                            = int16_t;
    static constexpr Type::BaseType value = Type::i16;
};
template <>
struct get_base_type<bool> {
    using type                            = bool;
    static constexpr Type::BaseType value = Type::boolean;
};
template <>
struct get_base_type<float> {
    using type                            = float;
    static constexpr Type::BaseType value = Type::f32;
};

template <>
struct get_base_type<double> {
    using type                            = double;
    static constexpr Type::BaseType value = Type::f64;
};

}  // namespace sc_api::core

#endif  // SC_API_TYPE_H_
