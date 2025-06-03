#ifndef SC_API_CORE_PROTOCOL_TYPES_H
#define SC_API_CORE_PROTOCOL_TYPES_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// NOLINTBEGIN(hicpp-deprecated-headers,modernize-deprecated-headers,modernize-use-using) C compatible code

/** Variable base type */
typedef enum SC_API_PROTOCOL_BaseType_e {
    SC_API_TYPE_INVALID = 0x00,
    SC_API_TYPE_BOOL    = 0x01,
    SC_API_TYPE_I8      = 0x02,
    SC_API_TYPE_U8      = 0x03,
    SC_API_TYPE_I16     = 0x04,
    SC_API_TYPE_U16     = 0x05,
    SC_API_TYPE_I32     = 0x06,
    SC_API_TYPE_U32     = 0x07,
    SC_API_TYPE_I64     = 0x08,
    SC_API_TYPE_F32     = 0x09,
    SC_API_TYPE_F64     = 0x0A,

    /** Always array type thats size defines maximum length of the string and is always constant and null terminated */
    SC_API_TYPE_CSTRING = 0x20,
} SC_API_PROTOCOL_BaseType_t;

typedef enum SC_API_PROTOCOL_TypeVariant_e {
    /** Base numeric type */
    SC_API_TYPE_VARIANT_BASE  = 0,

    /** Array with the size defined in the variant specific data */
    SC_API_TYPE_VARIANT_ARRAY = 1,

    /** Single bit of the base type, variant specific data defines the bit index
     *
     *  Only used with integer base types
     */
    SC_API_TYPE_VARIANT_BIT   = 2,
} SC_API_PROTOCOL_TypeVariant_t;

/**
 * 0 - 7: base type
 * 8 - 15: variant
 */
typedef uint16_t SC_API_PROTOCOL_Type_t;
typedef uint16_t SC_API_PROTOCOL_TypeVariantData_t;

#define SC_API_TYPE_GET_BASE_TYPE(type) ((SC_API_PROTOCOL_BaseType_t)((type) & 0xffu))
#define SC_API_TYPE_GET_VARIANT(type)   (((type) & 0xff00u) >> 8u)
#define SC_API_TYPE_IS_ARRAY(type)      (SC_API_TYPE_GET_VARIANT(type) == SC_API_TYPE_VARIANT_ARRAY)
#define SC_API_TYPE_IS_BASE_TYPE(type)  (SC_API_TYPE_GET_VARIANT(type) == SC_API_TYPE_VARIANT_BASE)
#define SC_API_TYPE_IS_BIT(type)        (SC_API_TYPE_GET_VARIANT(type) == SC_API_TYPE_VARIANT_BIT)

#define SC_API_TYPE_ARRAY(base_type) ((base_type) | (SC_API_TYPE_VARIANT_ARRAY << 8u))
#define SC_API_TYPE_BIT(base_type)   ((base_type) | (SC_API_TYPE_VARIANT_BIT << 8u))

// NOLINTEND(hicpp-deprecated-headers,modernize-deprecated-headers,modernize-use-using)

#ifdef __cplusplus
}
#endif

#endif  // SC_API_CORE_PROTOCOL_TYPES_H
