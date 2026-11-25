#pragma once

#include <nanobind/nanobind.h>

#include <cstdint>
#include <vector>

namespace nb = nanobind;

namespace bind_bson {

/// Encodes a dict as a BSON document.
///
/// Supports bool, int (64-bit signed), float, str, bytes, None, dict with str keys, and list or tuple.
/// Raises TypeError for any other value and OverflowError for an int that does not fit 64 bits.
std::vector<uint8_t> encode(const nb::dict& doc);

/// Decodes a BSON document into a dict; `bson` must hold a complete document.
nb::dict decode(const uint8_t* bson, std::size_t size);

}  // namespace bind_bson
