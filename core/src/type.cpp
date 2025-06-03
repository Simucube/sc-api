#include "sc-api/core/type.h"

namespace sc_api::core {

std::string Type::toString() const {
    std::string s(baseTypeToString(getBaseType()));
    if (isArray()) {
        s += "x";
        s += std::to_string(getArraySize());
    } else if (isBit()) {
        s += ".";
        s += std::to_string(getBitIndex());
    }

    return s;
}

std::string_view Type::baseTypeToString(BaseType type) {
    switch (type) {
        case boolean:
            return "boolean";
        case i8:
            return "i8";
        case u8:
            return "u8";
        case i16:
            return "i16";
        case u16:
            return "u16";
        case i32:
            return "i32";
        case u32:
            return "u32";
        case f32:
            return "f32";
        case f64:
            return "f64";
        case i64:
            return "i64";
        case cstring:
            return "cstring";

        case invalid:
        default:
            return "invalid";
    }
}

}  // namespace sc_api::core
