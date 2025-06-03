#include "sc-api/core/util/bson_reader.h"

#include <cassert>
#include <cstring>

namespace sc_api::core::util {

template <typename T>
static inline T getm(const uint8_t* buf) {
    T val;
    std::memcpy(&val, buf, sizeof(val));
    return val;
}

static int32_t geti32(const uint8_t* buf) { return getm<int32_t>(buf); }

BsonReader::BsonReader(const uint8_t* buffer) noexcept : buffer_(buffer) {
    if (!buffer) {
        // NOT VALID
        cur_type_ = ELEMENT_FORMAT_ERROR;
        return;
    }

    size_ = getTotalDocumentSize(buffer);

    if (size_ < 5) {
        // NOT VALID
        cur_type_ = ELEMENT_FORMAT_ERROR;
        return;
    }

    cur_type_      = ELEMENT_INTERNAL_DOC_BEGIN;
    buf_offset_    = 4;
    cur_doc_begin_ = buf_offset_;
    cur_doc_end_   = (int32_t)size_;
}

BsonReader::BsonReader(const uint8_t* buffer, size_t size) noexcept : buffer_(buffer), size_(size) {
    if (size_ < 5 || !buffer) {
        // NOT VALID
        cur_type_ = ELEMENT_FORMAT_ERROR;
        return;
    }

    int32_t doc_size = geti32(buffer);
    if (doc_size < 0 || doc_size > size_) {
        cur_type_ = ELEMENT_FORMAT_ERROR;
        return;
    }

    cur_type_      = ELEMENT_INTERNAL_DOC_BEGIN;
    buf_offset_    = 4;
    cur_doc_begin_ = buf_offset_;
    cur_doc_end_   = doc_size;
}

static constexpr uint8_t k_empty_but_valid_bson[5] = {0x5, 0, 0, 0, 0};
BsonReader::BsonReader() noexcept : BsonReader(k_empty_but_valid_bson) {}

BsonReader::ElementType BsonReader::next() noexcept {
    // Remove current value
    switch (cur_type_) {
        case ELEMENT_END:
        case ELEMENT_FORMAT_ERROR:
        case ELEMENT_UNSUPPORTED:
            return cur_type_;
        case ELEMENT_DOUBLE:
        case ELEMENT_I64:
            buf_offset_ += 8;
            break;
        case ELEMENT_I32:
            buf_offset_ += 4;
            break;
        case ELEMENT_BOOL:
            buf_offset_ += 1;
            break;
        case ELEMENT_NULL:
            break;
        case ELEMENT_STR: {
            int32_t byte_count = geti32(&buffer_[buf_offset_]);
            buf_offset_ += byte_count + 4;
            break;
        }

        case ELEMENT_BINARY: {
            int32_t byte_count = geti32(&buffer_[buf_offset_]);
            buf_offset_ += byte_count + 5;
            break;
        }
        case ELEMENT_DOC:
        case ELEMENT_ARRAY: {
            int32_t byte_count = geti32(&buffer_[buf_offset_]);
            buf_offset_ += byte_count;
            break;
        }

        case ELEMENT_INTERNAL_DOC_BEGIN:
            break;
    }

    uint8_t element_type  = buffer_[buf_offset_];
    element_start_offset_ = buf_offset_;
    switch (element_type) {
        case ELEMENT_END:
            ++buf_offset_;
            cur_type_ = ELEMENT_END;
            return ELEMENT_END;
        case ELEMENT_DOUBLE:
        case ELEMENT_STR:
        case ELEMENT_DOC:
        case ELEMENT_ARRAY:
        case ELEMENT_BINARY:
        case ELEMENT_BOOL:
        case ELEMENT_NULL:
        case ELEMENT_I32:
        case ELEMENT_I64:
            cur_type_ = (ElementType)element_type;
            break;
        default:
            cur_type_ = ELEMENT_UNSUPPORTED;
            break;
    }
    ++buf_offset_;

    if (!readKey()) {
        cur_type_ = ELEMENT_FORMAT_ERROR;
        return cur_type_;
    }

    // Validate that there is space for the actual value
    int32_t value_size = -1;
    switch (element_type) {
        case ELEMENT_DOUBLE:
        case ELEMENT_I64:
            value_size = 9;
            break;
        case ELEMENT_I32:
            value_size = 4;
            break;
        case ELEMENT_BOOL:
            value_size = 1;
            break;
        case ELEMENT_STR:
            value_size = geti32(&buffer_[buf_offset_]) + 4;
            break;
        case ELEMENT_BINARY:
            value_size = geti32(&buffer_[buf_offset_]) + 5;
            break;
        case ELEMENT_DOC:
        case ELEMENT_ARRAY:
            value_size = geti32(&buffer_[buf_offset_]);
            break;
        case ELEMENT_NULL:
            value_size = 0;
            break;
        default:
            assert(false);
    }

    if (value_size < 0 || (int64_t)buf_offset_ + value_size > cur_doc_end_) {
        cur_type_ = ELEMENT_FORMAT_ERROR;
        return cur_type_;
    }

    // TODO: we don't validate that array keys match the spec in terms of the key strings
    if (arr_index_ >= -1) {
        ++arr_index_;
    }

    return cur_type_;
}

BsonReader::ElementType BsonReader::seekKey(std::string_view search_key) noexcept {
    ElementOffset offset = elementOffset();

    seekBegin();
    ElementType next_e_type = next();
    while (next_e_type > 0) {
        if (key() == search_key) {
            return next_e_type;
        }

        next_e_type = next();
    }

    if (next_e_type == ELEMENT_END) {
        seek(offset);
    }
    return next_e_type;
}

BsonReader::ElementType BsonReader::seekNextKey(std::string_view search_key) noexcept {
    ElementOffset offset      = elementOffset();
    ElementType   next_e_type = next();
    while (next_e_type > 0) {
        if (key() == search_key) {
            return next_e_type;
        }

        next_e_type = next();
    }

    if (next_e_type == ELEMENT_END) {
        seek(offset);
    }
    return next_e_type;
}

BsonReader::ElementType BsonReader::seekIndex(int32_t index) noexcept {
    ElementOffset offset = elementOffset();

    if (index < 0) {
        return ELEMENT_END;
    }
    if (arr_index_ == -2) {
        // This isn't array
        return ELEMENT_END;
    }

    if (arr_index_ > index) {
        seekBegin();
    }

    ElementType next_e_type = next();
    while (next_e_type > 0) {
        if (arr_index_ == index) {
            return next_e_type;
        }
        next_e_type = next();
    }
    if (next_e_type == ELEMENT_END) {
        seek(offset);
    }
    return next_e_type;
}

BsonReader::ElementType BsonReader::seek(ElementOffset offset) noexcept {
    if (offset.offset >= cur_doc_begin_ && offset.offset < cur_doc_end_) {
        cur_type_   = ELEMENT_INTERNAL_DOC_BEGIN;
        buf_offset_ = offset.offset;
        arr_index_  = offset.arr_index;
        return next();
    }

    return ELEMENT_UNSUPPORTED;
}

void BsonReader::seekBegin() noexcept {
    buf_offset_ = cur_doc_begin_;
    cur_type_   = ELEMENT_INTERNAL_DOC_BEGIN;
    if (arr_index_ > -2) {
        arr_index_ = -1;
    }
}

std::pair<const uint8_t*, std::size_t> BsonReader::subdocument() const noexcept {
    if (cur_type_ != ELEMENT_DOC && cur_type_ != ELEMENT_ARRAY) {
        return {nullptr, 0};
    }

    return {&buffer_[buf_offset_], (std::size_t)geti32(&buffer_[buf_offset_])};
}

bool BsonReader::beginSub() {
    if (cur_type_ != ELEMENT_DOC && cur_type_ != ELEMENT_ARRAY) {
        return false;
    }

    if (last_doc_layer_ + 1 == k_max_subdocs) {
        return false;
    }

    int32_t doc_size = geti32(&buffer_[buf_offset_]);
    ++last_doc_layer_;
    doc_layers_[last_doc_layer_] = DocState{cur_doc_begin_, cur_doc_end_, arr_index_};
    cur_doc_end_                 = buf_offset_ + doc_size;

    buf_offset_ += 4;
    cur_doc_begin_ = buf_offset_;

    if (cur_type_ == ELEMENT_ARRAY) {
        arr_index_ = -1;
    } else {
        arr_index_ = -2;
    }
    cur_type_ = ELEMENT_INTERNAL_DOC_BEGIN;
    return true;
}

bool BsonReader::endSub() {
    if (last_doc_layer_ == -1) return false;
    const DocState& state = doc_layers_[last_doc_layer_];
    buf_offset_           = cur_doc_end_;
    cur_doc_end_          = state.end_offset;
    arr_index_            = state.arr_index_;
    --last_doc_layer_;
    cur_type_ = ELEMENT_INTERNAL_DOC_BEGIN;
    return true;
}

double BsonReader::doubleValue() const noexcept {
    assert(cur_type_ == ELEMENT_DOUBLE);
    return getm<double>(&buffer_[buf_offset_]);
}

std::string_view BsonReader::stringValue() const noexcept {
    assert(cur_type_ == ELEMENT_STR);
    int32_t byte_size = geti32(&buffer_[buf_offset_]);
    return std::string_view((const char*)&buffer_[buf_offset_ + 4], byte_size - 1);
}

bool BsonReader::boolValue() const noexcept {
    assert(cur_type_ == ELEMENT_BOOL);
    return buffer_[buf_offset_] == 0x01;
}

int64_t BsonReader::int64Value() const noexcept {
    assert(cur_type_ == ELEMENT_I64);
    return getm<int64_t>(&buffer_[buf_offset_]);
}

int32_t BsonReader::int32Value() const noexcept {
    assert(cur_type_ == ELEMENT_I32);
    return getm<int32_t>(&buffer_[buf_offset_]);
}

double BsonReader::asDouble() const noexcept {
    switch (cur_type_) {
        case ELEMENT_DOUBLE:
            return doubleValue();
        case ELEMENT_BOOL:
            return boolValue() ? 1.0 : 0.0;
        case ELEMENT_I32:
            return (double)int32Value();
        case ELEMENT_I64:
            return (double)int64Value();
        default:
            return 0.0;
    }
}

std::string BsonReader::asString() const {
    switch (cur_type_) {
        case ELEMENT_DOUBLE:
            return std::to_string(doubleValue());
        case ELEMENT_BOOL:
            return boolValue() ? "true" : "false";
        case ELEMENT_I32:
            return std::to_string(int32Value());
        case ELEMENT_I64:
            return std::to_string(int64Value());
        case ELEMENT_STR:
            return std::string(stringValue());
        case ELEMENT_BINARY: {
            std::string data;
            auto        val = binaryValue();
            data.resize(val.second);
            std::memcpy(data.data(), val.first, val.second);
            return data;
        }
        default:
            return {};
    }
}

int32_t BsonReader::asInt32() const noexcept {
    switch (cur_type_) {
        case ELEMENT_DOUBLE:
            return (int32_t)doubleValue();
        case ELEMENT_BOOL:
            return boolValue() ? 1 : 0;
        case ELEMENT_I32:
            return int32Value();
        case ELEMENT_I64:
            return (int32_t)int64Value();
        default:
            return 0;
    }
}

uint8_t BsonReader::binaryType() const noexcept { return buffer_[buf_offset_ + 4]; }

std::pair<const uint8_t*, size_t> BsonReader::rawElement() const noexcept {
    int32_t value_size = 0;
    switch (cur_type_) {
        case ELEMENT_DOUBLE:
        case ELEMENT_I64:
            value_size = 8;
            break;
        case ELEMENT_I32:
            value_size = 4;
            break;
        case ELEMENT_BOOL:
            value_size = 1;
            break;
        case ELEMENT_STR:
            value_size = geti32(&buffer_[buf_offset_]) + 4;  // int32_t length
            break;
        case ELEMENT_BINARY:
            value_size = geti32(&buffer_[buf_offset_]) + 5;  // int32_t size + uint8_t binary data type
            break;
        case ELEMENT_DOC:
        case ELEMENT_ARRAY:
            value_size = geti32(&buffer_[buf_offset_]);
            break;
        case ELEMENT_NULL:
            value_size = 0;
            break;
        case ELEMENT_END:
            return {&buffer_[element_start_offset_], 1};
        default:
            return {nullptr, 0};
    }

    return {&buffer_[element_start_offset_], buf_offset_ - element_start_offset_ + value_size};
}

std::pair<const uint8_t*, size_t> BsonReader::binaryValue() const noexcept {
    int32_t byte_size = geti32(&buffer_[buf_offset_]);
    return {&buffer_[buf_offset_ + 5], byte_size};
}

bool BsonReader::tryGetValue(double& v) const noexcept {
    switch (cur_type_) {
        case ELEMENT_I32:
            v = (double)int32Value();
            return true;
        case ELEMENT_I64:
            v = (double)int64Value();
            return true;

        case ELEMENT_DOUBLE:
            v = doubleValue();
            return true;
        default:
            return false;
    }
}
bool BsonReader::tryGetValue(int32_t& v) const noexcept {
    switch (cur_type_) {
        case ELEMENT_I32:
            v = int32Value();
            return true;
        case ELEMENT_I64:
            v = (int32_t)int64Value();
            return true;

        case ELEMENT_DOUBLE:
            v = (int32_t)doubleValue();
            return true;
        default:
            return false;
    }
}
bool BsonReader::tryGetValue(int64_t& v) const noexcept {
    switch (cur_type_) {
        case ELEMENT_I32:
            v = int32Value();
            return true;
        case ELEMENT_I64:
            v = int64Value();
            return true;

        case ELEMENT_DOUBLE:
            v = (int64_t)doubleValue();
            return true;
        default:
            return false;
    }
}
bool BsonReader::tryGetValue(bool& v) const noexcept {
    switch (cur_type_) {
        case ELEMENT_I32:
            v = int32Value() != 0;
            return true;
        case ELEMENT_I64:
            v = int64Value() != 0;
            return true;

        case ELEMENT_DOUBLE:
            v = doubleValue() != 0;
            return true;
        case ELEMENT_BOOL:
            v = boolValue();
            return true;
        default:
            return false;
    }
}
bool BsonReader::tryGetValue(std::string_view& v) const noexcept {
    if (cur_type_ == ELEMENT_STR) {
        v = stringValue();
        return true;
    }
    return false;
}
bool BsonReader::tryGetValue(std::string& v) const {
    if (cur_type_ == ELEMENT_STR) {
        v = stringValue();
        return true;
    }
    return false;
}

bool BsonReader::tryGetValue(std::pair<const uint8_t*, size_t>& v) const noexcept {
    if (cur_type_ == ELEMENT_BINARY) {
        v = binaryValue();
        return true;
    }
    return false;
}

bool BsonReader::tryGetValue(std::vector<uint8_t>& v) const {
    if (cur_type_ == ELEMENT_BINARY) {
        auto bin_data = binaryValue();
        v.resize(bin_data.second);
        std::memcpy(v.data(), bin_data.first, bin_data.second);
        return true;
    }
    return false;
}

int32_t BsonReader::getTotalDocumentSize(const uint8_t* buf) { return geti32(buf); }

bool BsonReader::validate(const uint8_t* buf, std::size_t s) {
    if (s < 5) return false;
    if (getTotalDocumentSize(buf) > s) return false;

    BsonReader r(buf, s);
    return r.validate();
}

bool BsonReader::validate() {
    int depth = 0;
    while (true) {
        BsonReader::ElementType e = next();
        if (e == BsonReader::ELEMENT_FORMAT_ERROR) {
            return false;
        }

        if (e == BsonReader::ELEMENT_END) {
            if (depth > 0) {
                if (!endSub()) {
                    return false;
                }
                depth--;
            } else {
                return true;
            }
        }
        if (e == BsonReader::ELEMENT_DOC || e == BsonReader::ELEMENT_ARRAY) {
            if (!beginSub()) return false;
            depth++;
        }
    }
}

bool BsonReader::readKey() {
    int32_t start_offset = buf_offset_;
    for (; buf_offset_ < cur_doc_end_; ++buf_offset_) {
        if (buffer_[buf_offset_] == '\0') {
            key_ = std::string_view((const char*)&buffer_[start_offset], buf_offset_ - start_offset);
            ++buf_offset_;
            return true;
        }
    }
    return false;
}
}  // namespace sc_api::core::util
