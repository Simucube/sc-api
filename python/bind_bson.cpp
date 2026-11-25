#include "bind_bson.h"

#include <nanobind/stl/string.h>
#include <nanobind/stl/string_view.h>
#include <sc-api/util/bson_builder.h>
#include <sc-api/util/bson_reader.h>

#include <limits>
#include <string>
#include <string_view>

#include "bind_exceptions.h"

namespace bind_bson {

using sc_api::util::BsonBuilder;
using sc_api::util::BsonReader;

namespace {

constexpr int         k_max_nesting   = 32;  ///< Dicts and lists inside the arguments; also stops a cyclic value
constexpr std::size_t k_max_value_len = std::numeric_limits<int32_t>::max() / 2;  ///< BSON sizes are int32

void encodeDocument(BsonBuilder& b, const nb::dict& doc, int depth);
void encodeArray(BsonBuilder& b, const nb::handle& seq, int depth);

std::string_view keyOf(const nb::handle& key) {
    if (!nb::isinstance<nb::str>(key)) {
        throw nb::type_error("BSON document keys must be str");
    }
    const auto name = nb::cast<std::string_view>(key);
    if (name.find('\0') != std::string_view::npos) {
        throw nb::value_error("BSON document keys must not contain a null character");
    }
    return name;
}

void checkLength(std::size_t size) {
    if (size > k_max_value_len) {
        throw nb::value_error("str or bytes value is too large to send");
    }
}

void checkAdded(bool added) {
    if (!added) throw_internal_error("Could not encode the command arguments as BSON");
}

[[noreturn]] void throwUnsupported(const nb::handle& value) {
    const std::string type_name = nb::cast<std::string>(nb::handle(value.type()).attr("__name__"));
    throw nb::type_error(("Cannot encode a value of type " + type_name + " to BSON").c_str());
}

/// Appends one value under `key`, or as the next array element when `key` is null.
void encodeValue(BsonBuilder& b, const std::string_view* key, const nb::handle& value, int depth) {
    if (value.is_none()) {
        checkAdded(key ? b.docAddElement(*key, nullptr) : b.arrayAddElement(nullptr));
    } else if (nb::isinstance<nb::bool_>(value)) {
        const bool v = nb::cast<bool>(value);
        checkAdded(key ? b.docAddElement(*key, v) : b.arrayAddElement(v));
    } else if (nb::isinstance<nb::int_>(value)) {
        int64_t v = 0;
        if (!nb::try_cast(value, v)) {
            throw nb::value_error("int does not fit in a signed 64-bit BSON integer");
        }
        if (v >= std::numeric_limits<int32_t>::min() && v <= std::numeric_limits<int32_t>::max()) {
            const auto v32 = static_cast<int32_t>(v);
            checkAdded(key ? b.docAddElement(*key, v32) : b.arrayAddElement(v32));
        } else {
            checkAdded(key ? b.docAddElement(*key, v) : b.arrayAddElement(v));
        }
    } else if (nb::isinstance<nb::float_>(value)) {
        const double v = nb::cast<double>(value);
        checkAdded(key ? b.docAddElement(*key, v) : b.arrayAddElement(v));
    } else if (nb::isinstance<nb::str>(value)) {
        const auto v = nb::cast<std::string_view>(value);
        checkLength(v.size());
        checkAdded(key ? b.docAddElement(*key, v) : b.arrayAddElement(v));
    } else if (nb::isinstance<nb::bytes>(value)) {
        const nb::bytes v = nb::borrow<nb::bytes>(value);
        checkLength(v.size());
        const auto* p = static_cast<const uint8_t*>(static_cast<const void*>(v.c_str()));
        const auto  n = static_cast<int32_t>(v.size());
        checkAdded(key ? b.docAddElement(*key, p, n) : b.arrayAddElement(p, n));
    } else if (nb::isinstance<nb::dict>(value) || nb::isinstance<nb::list>(value) || nb::isinstance<nb::tuple>(value)) {
        if (depth >= k_max_nesting) {
            throw nb::value_error("Command arguments are nested too deeply, or contain themselves");
        }
        if (nb::isinstance<nb::dict>(value)) {
            checkAdded(key ? b.docBeginSubDoc(*key) : b.arrayBeginSubDoc());
            encodeDocument(b, nb::borrow<nb::dict>(value), depth + 1);
            b.endDocument();
        } else {
            if (key) {
                const std::string name(*key);  // docBeginSubArray takes a null-terminated name
                checkAdded(b.docBeginSubArray(name.c_str()));
            } else {
                checkAdded(b.arrayBeginSubArray());
            }
            encodeArray(b, value, depth + 1);
            b.endArray();
        }
    } else {
        throwUnsupported(value);
    }
}

void encodeDocument(BsonBuilder& b, const nb::dict& doc, int depth) {
    for (auto [key, value] : doc) {
        const std::string_view name = keyOf(key);
        encodeValue(b, &name, value, depth);
    }
}

void encodeArray(BsonBuilder& b, const nb::handle& seq, int depth) {
    for (nb::handle item : seq) {
        encodeValue(b, nullptr, item, depth);
    }
}

nb::object decodeCurrent(BsonReader& r, BsonReader::ElementType type);

nb::dict decodeDocumentBody(BsonReader& r) {
    nb::dict out;
    for (BsonReader::ElementType e = r.next(); !r.atEnd(); e = r.next()) {
        nb::str key(r.key().data(), r.key().size());  // read before decoding, which moves into a sub-document
        out[key] = decodeCurrent(r, e);
    }
    if (r.error()) throw_internal_error("Malformed BSON in the command reply");
    return out;
}

nb::list decodeArrayBody(BsonReader& r) {
    nb::list out;
    for (BsonReader::ElementType e = r.next(); !r.atEnd(); e = r.next()) {
        out.append(decodeCurrent(r, e));
    }
    if (r.error()) throw_internal_error("Malformed BSON in the command reply");
    return out;
}

nb::object decodeCurrent(BsonReader& r, BsonReader::ElementType type) {
    switch (type) {
        case BsonReader::ELEMENT_DOUBLE:
            return nb::float_(r.doubleValue());
        case BsonReader::ELEMENT_STR: {
            const std::string_view s = r.stringValue();
            return nb::str(s.data(), s.size());
        }
        case BsonReader::ELEMENT_BOOL:
            return nb::bool_(r.boolValue());
        case BsonReader::ELEMENT_NULL:
            return nb::none();
        case BsonReader::ELEMENT_I32:
            return nb::int_(r.int32Value());
        case BsonReader::ELEMENT_I64:
            return nb::int_(r.int64Value());
        case BsonReader::ELEMENT_BINARY: {
            const auto [p, n] = r.binaryValue();
            return nb::bytes(p, n);
        }
        case BsonReader::ELEMENT_DOC: {
            if (!r.beginSub()) throw_internal_error("The command reply is nested too deeply to read");
            nb::dict d = decodeDocumentBody(r);
            r.endSub();
            return d;
        }
        case BsonReader::ELEMENT_ARRAY: {
            if (!r.beginSub()) throw_internal_error("The command reply is nested too deeply to read");
            nb::list l = decodeArrayBody(r);
            r.endSub();
            return l;
        }
        default:
            throw_internal_error("Unsupported BSON element type in the command reply");
    }
}

}  // namespace

std::vector<uint8_t> encode(const nb::dict& doc) {
    std::vector<uint8_t> buffer;
    BsonBuilder          b(&buffer);
    encodeDocument(b, doc, 0);
    const auto [ptr, size] = b.finish();
    if (ptr == nullptr) throw_internal_error("Could not encode the command arguments as BSON");
    return {ptr, ptr + size};
}

nb::dict decode(const uint8_t* bson, std::size_t size) {
    BsonReader r(bson, size);
    return decodeDocumentBody(r);
}

}  // namespace bind_bson
