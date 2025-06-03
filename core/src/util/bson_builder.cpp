#include "sc-api/core/util/bson_builder.h"

#include <cassert>
#include <cstring>

namespace sc_api::core::util {

template <typename T>
static inline T getm(const uint8_t* buf) {
    T val;
    std::memcpy(&val, buf, sizeof(val));
    return val;
}

enum ElementTypes : uint8_t {
    ELEMENT_DOUBLE = 0x01,
    ELEMENT_STR    = 0x02,
    ELEMENT_DOC    = 0x03,
    ELEMENT_ARRAY  = 0x04,
    ELEMENT_BINARY = 0x05,
    ELEMENT_BOOL   = 0x08,
    ELEMENT_NULL   = 0x0A,
    ELEMENT_I32    = 0x10,
    ELEMENT_I64    = 0x12,
};

BsonBuilder::BsonBuilder()
    : buffer_ptr_{nullptr}, offset_{0}, current_usage_{0}, capacity_{0}, container_{nullptr}, start_offset_{0} {}

BsonBuilder::BsonBuilder(uint8_t* buffer, uint32_t max_size)
    : buffer_ptr_(buffer),
      offset_(0),
      current_usage_(0),
      capacity_((int32_t)max_size),
      container_(nullptr),
      start_offset_(0) {
    initialize(buffer, max_size);
}

BsonBuilder::BsonBuilder(std::vector<uint8_t>* buffer, uint32_t start_offset, uint32_t reserved_extra_footer) {
    initialize(buffer, start_offset, reserved_extra_footer);
}

void BsonBuilder::initialize(uint8_t* buffer, uint32_t max_size) {
    buffer_ptr_    = buffer;
    offset_        = 0;
    current_usage_ = 0;
    capacity_      = (int32_t)max_size;
    container_     = nullptr;
    start_offset_  = 0;

    reserveMainDocumentHeader();
}

void BsonBuilder::initialize(std::vector<uint8_t>* buffer, uint32_t start_offset, uint32_t reserved_extra_footer) {
    if (buffer->size() < 5 + start_offset + reserved_extra_footer) {
        buffer->resize(10 + start_offset + reserved_extra_footer);
    }

    container_     = buffer;
    buffer_ptr_    = buffer->data();
    capacity_      = (int32_t)buffer->size();
    start_offset_  = (int32_t)start_offset;
    offset_        = start_offset_;
    current_usage_ = offset_ + (int32_t)reserved_extra_footer;

    reserveMainDocumentHeader();

    reallocate_ = [](void* container, int32_t used_bytes, int32_t min_required_bytes) -> std::pair<uint8_t*, int32_t> {
        std::vector<uint8_t>* vec = reinterpret_cast<std::vector<uint8_t>*>(container);
        vec->reserve(min_required_bytes);

        // vector likely actually allocates more than required so lets just resize the container to the actually
        // allocated size
        vec->resize(vec->capacity());
        return {vec->data(), (int32_t)vec->size()};
    };
}

BsonBuilder::BsonBuilder(BsonBuilder&& b) noexcept {
    document_size_offsets_ = std::move(b.document_size_offsets_);
    array_idx_counters_    = std::move(b.array_idx_counters_);

    buffer_ptr_            = b.buffer_ptr_;
    offset_                = b.offset_;
    current_usage_         = b.current_usage_;
    capacity_              = b.capacity_;
    container_             = b.container_;

    doc_is_array_bits_     = b.doc_is_array_bits_;
    depth_                 = b.depth_;
    start_offset_          = b.start_offset_;
    error_flag_            = b.error_flag_;
    reallocate_            = std::move(b.reallocate_);
}

BsonBuilder& BsonBuilder::operator=(BsonBuilder&& b) noexcept {
    if (&b == this) return *this;

    std::swap(array_idx_counters_, b.array_idx_counters_);
    std::swap(buffer_ptr_, b.buffer_ptr_);
    std::swap(offset_, b.offset_);
    std::swap(current_usage_, b.current_usage_);
    std::swap(capacity_, b.capacity_);
    std::swap(container_, b.container_);
    std::swap(doc_is_array_bits_, b.doc_is_array_bits_);
    std::swap(depth_, b.depth_);
    std::swap(start_offset_, b.start_offset_);
    std::swap(error_flag_, b.error_flag_);
    std::swap(reallocate_, b.reallocate_);
    return *this;
}

// Get pointer with 2 characters of value [0, 100[
constexpr const char* digits2(std::size_t value) {
    return &"0001020304050607080910111213141516171819"
        "2021222324252627282930313233343536373839"
        "4041424344454647484950515253545556575859"
        "6061626364656667686970717273747576777879"
        "8081828384858687888990919293949596979899"[value * 2];
}

static constexpr unsigned    k_array_doc_key_buf_size = 8;
std::pair<uint8_t*, int32_t> fillArrayDocKey(uint8_t* buf, int32_t idx) {
    assert(idx < 1000000 && "We support max 1M array elements");

    auto copy2 = [](uint8_t* b, const char* digits) { std::memcpy(b, digits, 2); };

    if (idx < 10) {
        buf[0] = '0' + idx;
        buf[1] = 0;
        return {buf, 2};
    }
    if (idx < 100) {
        const char* digits = digits2((unsigned)idx);
        copy2(buf, digits);
        buf[2] = 0;
        return {buf, 3};
    }

    char* out = (char*)(buf + k_array_doc_key_buf_size - 1);

    out[0]    = 0;
    while (idx >= 100) {
        out -= 2;

        const char* digits = digits2((unsigned)idx % 100);
        copy2(buf, digits);
        idx /= 100;
    }

    if (idx < 10) {
        --out;
        *out = (char)('0' + idx);
    } else {
        const char* digits = digits2((unsigned)idx);
        copy2(buf, digits);
    }

    return {(uint8_t*)out, (int32_t)((buf + k_array_doc_key_buf_size) - (uint8_t*)out)};
}

bool BsonBuilder::arrayBeginSubDoc() {
    assert(currentDocIsArray());

    if (!verifyCapacityAndInsertArrayKey(ELEMENT_DOC, 5)) {
        return false;
    }

    document_size_offsets_.push_back(offset_);
    offset_ += 4;
    ++depth_;
    doc_is_array_bits_ &= ~(1ull << depth_);
    return true;
}

bool BsonBuilder::docBeginSubDoc(std::string_view name) {
    assert(!currentDocIsArray());

    // Reserve space for doc size and null-terminator that is set by endDocument
    if (!verifyCapacityAndInsertKey(ELEMENT_DOC, name, 5)) {
        return false;
    }

    document_size_offsets_.push_back(offset_);
    offset_ += 4;
    ++depth_;
    doc_is_array_bits_ &= ~(1ull << depth_);
    return true;
}

bool BsonBuilder::docBeginSubDoc(std::string_view name, const uint8_t* sub_document) {
    assert(!currentDocIsArray());
    assert(sub_document);

    int32_t sub_doc_size = getm<int32_t>(sub_document);
    assert(sub_doc_size >= 5);

    // Reserve space for doc size and null-terminator that is set by endDocument
    if (!verifyCapacityAndInsertKey(ELEMENT_DOC, name, sub_doc_size)) {
        return false;
    }

    document_size_offsets_.push_back(offset_);
    offset_ += 4;
    ++depth_;
    doc_is_array_bits_ &= ~(1ull << depth_);

    // Copy all data from the give sub document. Skip doc size as that is set in endDocument
    std::memcpy(&buffer_ptr_[offset_], sub_document + 4, sub_doc_size - 5);
    offset_ += sub_doc_size - 5;
    return true;
}

void BsonBuilder::endDocument() {
    assert(!document_size_offsets_.empty());
    assert(!currentDocIsArray());
    int32_t start_offset = document_size_offsets_.back();
    document_size_offsets_.pop_back();

    // Null-terminate
    buffer_ptr_[offset_] = 0;
    ++offset_;

    int32_t size = offset_ - start_offset;
    std::memcpy(&buffer_ptr_[start_offset], &size, sizeof(int32_t));
    --depth_;
}

bool BsonBuilder::docAddElement(std::string_view name, bool value) {
    if (!verifyCapacityAndInsertKey(ELEMENT_BOOL, name, 1)) {
        return false;
    }
    buffer_ptr_[offset_] = value ? 0x01 : 0x00;
    ++offset_;
    return true;
}

bool BsonBuilder::docAddElement(std::string_view name, double value) {
    if (!verifyCapacityAndInsertKey(ELEMENT_DOUBLE, name, sizeof(value))) {
        return false;
    }
    std::memcpy(&buffer_ptr_[offset_], &value, sizeof(value));
    offset_ += sizeof(value);
    return true;
}

bool BsonBuilder::docAddElement(std::string_view name, int32_t value) {
    if (!verifyCapacityAndInsertKey(ELEMENT_I32, name, sizeof(value))) {
        return false;
    }
    std::memcpy(&buffer_ptr_[offset_], &value, sizeof(value));
    offset_ += sizeof(value);
    return true;
}

bool BsonBuilder::docAddElement(std::string_view name, int64_t value) {
    if (!verifyCapacityAndInsertKey(ELEMENT_I64, name, sizeof(value))) {
        return false;
    }
    std::memcpy(&buffer_ptr_[offset_], &value, sizeof(value));
    offset_ += sizeof(value);
    return true;
}

bool BsonBuilder::docAddElement(std::string_view name, std::nullptr_t) {
    return verifyCapacityAndInsertKey(ELEMENT_NULL, name, 0);
}

bool BsonBuilder::docAddElement(std::string_view name, std::string_view value) {
    // String value must always by null-terminated
    int32_t string_data_size = (int32_t)value.size() + 1;
    int32_t value_size       = string_data_size + (int32_t)sizeof(int32_t);
    if (!verifyCapacityAndInsertKey(ELEMENT_STR, name, value_size)) {
        return false;
    }
    std::memcpy(&buffer_ptr_[offset_], &string_data_size, sizeof(int32_t));
    std::memcpy(&buffer_ptr_[offset_ + sizeof(int32_t)], value.data(), value.size());
    offset_ += value_size;

    // Manually handle null-termination as string_view isn't guaranteed to be null-terminated
    buffer_ptr_[offset_ - 1] = 0;
    return true;
}

bool BsonBuilder::docAddElement(std::string_view name, const std::vector<uint8_t>& data, uint8_t subtype) {
    return docAddElement(name, data.data(), (int32_t)data.size(), subtype);
}

bool BsonBuilder::docAddElement(std::string_view name, const uint8_t* bytes, int32_t byte_count, uint8_t subtype) {
    if (byte_count < 0) return false;

    int32_t value_size = byte_count + (int32_t)sizeof(int32_t) + 1;
    if (!verifyCapacityAndInsertKey(ELEMENT_BINARY, name, value_size)) {
        return false;
    }
    std::memcpy(&buffer_ptr_[offset_], &byte_count, sizeof(int32_t));
    buffer_ptr_[offset_ + sizeof(int32_t)] = subtype;
    std::memcpy(&buffer_ptr_[offset_ + sizeof(int32_t) + 1], bytes, byte_count);

    offset_ += value_size;
    return true;
}

bool BsonBuilder::docAddSubDoc(std::string_view name, const uint8_t* sub_document) {
    int32_t doc_size;
    std::memcpy(&doc_size, sub_document, sizeof(doc_size));
    if (doc_size < 0) return false;

    if (!verifyCapacityAndInsertKey(ELEMENT_DOC, name, doc_size)) {
        return false;
    }

    std::memcpy(&buffer_ptr_[offset_], sub_document, doc_size);
    offset_ += doc_size;
    return true;
}

bool BsonBuilder::docBeginSubArray(const char* name) {
    assert(!currentDocIsArray());

    // Reserve space for doc size and null-terminator that is set by endDocument
    if (!verifyCapacityAndInsertKey(ELEMENT_ARRAY, name, 5)) {
        return false;
    }

    document_size_offsets_.push_back(offset_);
    array_idx_counters_.push_back(0);
    offset_ += 4;
    ++depth_;
    doc_is_array_bits_ |= 1ull << depth_;
    return true;
}

bool BsonBuilder::arrayBeginSubArray() {
    assert(currentDocIsArray());

    // Reserve space for doc size and null-terminator that is set by endDocument
    if (!verifyCapacityAndInsertArrayKey(ELEMENT_ARRAY, 5)) {
        return false;
    }

    document_size_offsets_.push_back(offset_);
    array_idx_counters_.push_back(0);
    offset_ += 4;
    ++depth_;
    doc_is_array_bits_ |= 1ull << depth_;
    return true;
}

bool BsonBuilder::arrayAddElement(bool value) {
    if (!verifyCapacityAndInsertArrayKey(ELEMENT_BOOL, 1)) {
        return false;
    }
    buffer_ptr_[offset_] = value ? 0x00 : 0x01;
    ++offset_;
    return true;
}

bool BsonBuilder::arrayAddElement(double value) {
    if (!verifyCapacityAndInsertArrayKey(ELEMENT_DOUBLE, sizeof(value))) {
        return false;
    }
    std::memcpy(&buffer_ptr_[offset_], &value, sizeof(value));
    offset_ += sizeof(value);
    return true;
}

bool BsonBuilder::arrayAddElement(int32_t value) {
    if (!verifyCapacityAndInsertArrayKey(ELEMENT_I32, sizeof(value))) {
        return false;
    }
    std::memcpy(&buffer_ptr_[offset_], &value, sizeof(value));
    offset_ += sizeof(value);
    return true;
}

bool BsonBuilder::arrayAddElement(int64_t value) {
    if (!verifyCapacityAndInsertArrayKey(ELEMENT_I64, sizeof(value))) {
        return false;
    }
    std::memcpy(&buffer_ptr_[offset_], &value, sizeof(value));
    offset_ += sizeof(value);
    return true;
}

bool BsonBuilder::arrayAddElement(std::string_view value) {
    // String value must always by null-terminated
    int32_t string_data_size = (int32_t)value.size() + 1;
    int32_t value_size       = string_data_size + (int32_t)sizeof(int32_t);
    if (!verifyCapacityAndInsertArrayKey(ELEMENT_STR, value_size)) {
        return false;
    }
    std::memcpy(&buffer_ptr_[offset_], &string_data_size, sizeof(int32_t));
    std::memcpy(&buffer_ptr_[offset_ + sizeof(int32_t)], value.data(), value.size());
    offset_ += value_size;

    // Manually handle null-termination as string_view isn't guaranteed to be null-terminated
    buffer_ptr_[offset_ - 1] = 0;
    return true;
}

bool BsonBuilder::arrayAddElement(std::nullptr_t) { return verifyCapacityAndInsertArrayKey(ELEMENT_NULL, 0); }
bool BsonBuilder::arrayAddElement(const std::vector<uint8_t>& data, uint8_t subtype) {
    return arrayAddElement(data.data(), (int32_t)data.size(), subtype);
}

bool BsonBuilder::arrayAddElement(const uint8_t* bytes, int32_t byte_count, uint8_t subtype) {
    if (byte_count < 0) return false;

    int32_t value_size = byte_count + (int32_t)sizeof(int32_t) + 1;
    if (!verifyCapacityAndInsertArrayKey(ELEMENT_BINARY, value_size)) {
        return false;
    }
    std::memcpy(&buffer_ptr_[offset_], &byte_count, sizeof(int32_t));
    buffer_ptr_[offset_ + sizeof(int32_t)] = subtype;
    std::memcpy(&buffer_ptr_[offset_ + sizeof(int32_t) + 1], bytes, byte_count);

    offset_ += value_size;
    return true;
}

void BsonBuilder::endArray() {
    assert(!document_size_offsets_.empty());
    assert(currentDocIsArray());
    int32_t start_offset = document_size_offsets_.back();
    document_size_offsets_.pop_back();
    array_idx_counters_.pop_back();

    // Null-terminate
    buffer_ptr_[offset_] = 0;
    ++offset_;

    int32_t size = offset_ - start_offset;
    std::memcpy(&buffer_ptr_[start_offset], &size, sizeof(int32_t));
    --depth_;
}

std::pair<uint8_t*, int32_t> BsonBuilder::finish() {
    if (depth_ != 0 || error_flag_) {
        return {nullptr, 0};
    }

    // We reserve space for the null-termination during init so we don't need to handle resizing buffer here
    buffer_ptr_[offset_] = 0;
    ++offset_;

    // Write size of the full document to the start of the buffer
    int32_t doc_size = offset_ - start_offset_;
    std::memcpy(&buffer_ptr_[start_offset_], &doc_size, sizeof(doc_size));
    return {&buffer_ptr_[start_offset_], doc_size};
}

bool BsonBuilder::verifyEnoughCapacity(int32_t bytes) {
    if (current_usage_ + bytes <= capacity_) {
        return true;
    }

    if (reallocate_) {
        auto reallocated_buf = reallocate_(container_, offset_, current_usage_ + bytes);
        buffer_ptr_          = reallocated_buf.first;
        capacity_            = reallocated_buf.second;
        if (current_usage_ + bytes <= capacity_) {
            return true;
        }
    }

    error_flag_ = true;
    return false;
}

bool BsonBuilder::verifyCapacityAndInsertArrayKey(uint8_t element_type, int32_t value_size) {
    assert(currentDocIsArray());

    uint8_t key_buffer[k_array_doc_key_buf_size];
    int32_t idx                  = array_idx_counters_.back()++;
    auto [key_str, key_full_len] = fillArrayDocKey(key_buffer, idx);

    if (!verifyEnoughCapacity(value_size + 1 + key_full_len)) {
        return false;
    }

    buffer_ptr_[offset_] = element_type;
    ++offset_;

    std::memcpy(&buffer_ptr_[offset_], key_str, key_full_len);
    offset_ += key_full_len;
    current_usage_ += value_size + 1 + key_full_len;
    return true;
}

bool BsonBuilder::verifyCapacityAndInsertKey(uint8_t element_type, std::string_view name, int32_t value_size) {
    assert(!currentDocIsArray());

    if (!verifyEnoughCapacity(value_size + 2 + (int32_t)name.length())) {
        return false;
    }

    buffer_ptr_[offset_] = element_type;
    ++offset_;

    // remember to copy null-termination
    std::memcpy(&buffer_ptr_[offset_], name.data(), name.length());
    offset_ += (int32_t)name.length();
    buffer_ptr_[offset_] = '\0';
    ++offset_;
    current_usage_ += value_size + 2 + (int32_t)name.length();
    return true;
}

void BsonBuilder::reserveMainDocumentHeader() {
    // document begins with document size as int32_t
    offset_ += 4;

    // Document is always null-terminated
    current_usage_ += 5;
}

}  // namespace sc_api::core::util
