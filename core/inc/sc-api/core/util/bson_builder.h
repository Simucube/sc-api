#ifndef SC_API_UTIL_BSON_BUILDER_H
#define SC_API_UTIL_BSON_BUILDER_H
#include <cstdint>
#include <functional>
#include <string_view>
#include <vector>

namespace sc_api::core::util {

/** Helper class for building valid BSON data */
class BsonBuilder {
public:
    BsonBuilder();
    BsonBuilder(uint8_t* buffer, uint32_t max_size);
    BsonBuilder(std::vector<uint8_t>* buffer, uint32_t start_offset = 0, uint32_t reserved_extra_footer = 0);
    BsonBuilder(BsonBuilder&& b) noexcept;

    BsonBuilder& operator=(BsonBuilder&& b) noexcept;

    void initialize(uint8_t* buffer, uint32_t max_size);
    void initialize(std::vector<uint8_t>* buffer, uint32_t start_offset = 0, uint32_t reserved_extra_footer = 0);

    /** Begin inserting document into current array */
    bool arrayBeginSubDoc();
    bool docBeginSubDoc(std::string_view name);
    bool docBeginSubDoc(std::string_view name, const uint8_t* sub_document);
    bool docAddElement(std::string_view name, bool value);
    bool docAddElement(std::string_view name, double value);
    bool docAddElement(std::string_view name, int32_t value);
    bool docAddElement(std::string_view name, int64_t value);
    bool docAddElement(std::string_view name, std::nullptr_t);
    bool docAddElement(std::string_view name, const char* value) {
        return docAddElement(name, std::string_view(value));
    }
    bool docAddElement(std::string_view name, std::string_view value);
    bool docAddElement(std::string_view name, const std::vector<uint8_t>& data, uint8_t subtype = 0x00);
    bool docAddElement(std::string_view name, const uint8_t* bytes, int32_t byte_count, uint8_t subtype = 0x00);
    bool docAddSubDoc(std::string_view name, const uint8_t* sub_document);
    void endDocument();

    /** Start inserting array into current document with the given key
     *
     * addAddElement or arrayBeginDoc must be used to add elements to this array
     * and endArray must be called to close array
     */
    bool docBeginSubArray(const char* name);

    /** Begin inserting array into an array */
    bool arrayBeginSubArray();
    bool arrayAddElement(bool value);
    bool arrayAddElement(double value);
    bool arrayAddElement(int32_t value);
    bool arrayAddElement(int64_t value);
    bool arrayAddElement(const char* value) { return arrayAddElement(std::string_view(value)); }
    bool arrayAddElement(std::string_view value);
    bool arrayAddElement(std::nullptr_t);
    bool arrayAddElement(const std::vector<uint8_t>& data, uint8_t subtype = 0x00);
    bool arrayAddElement(const uint8_t* bytes, int32_t byte_count, uint8_t subtype = 0x00);
    void endArray();

    bool error() const { return error_flag_; }
    bool success() const { return !error_flag_; }

    /** verifies that current document is valid -> endArray and endDocument have been called correctly and adds
     * null-termination
     *
     * \returns On success, returns pointer to start of the valid bson document and total size of the document
     *          {nullptr, 0} on error.
     *
     */
    std::pair<uint8_t*, int32_t> finish();

    /** Get depth of the sub documents stack
     *
     * endArray/endDocument must be called this many times before finish can be called
     */
    int32_t getDocumentDepth() const { return depth_; }

private:
    bool verifyEnoughCapacity(int32_t bytes);
    bool verifyCapacityAndInsertArrayKey(uint8_t element_type, int32_t value_size);
    bool verifyCapacityAndInsertKey(uint8_t element_type, std::string_view name, int32_t value_size);
    void reserveMainDocumentHeader();

    bool currentDocIsArray() const { return (doc_is_array_bits_ & (1ull << depth_)) != 0; }

    std::vector<int32_t> document_size_offsets_;
    std::vector<int32_t> array_idx_counters_;

    uint8_t* buffer_ptr_;
    int32_t  offset_;
    int32_t  current_usage_;
    int32_t  capacity_;
    void*    container_;

    uint64_t doc_is_array_bits_ = 0;
    int32_t  depth_             = 0;

    int32_t start_offset_;
    bool    error_flag_ = false;

    std::function<std::pair<uint8_t*, int32_t>(void*, int32_t, int32_t)> reallocate_;
};

}  // namespace sc_api::core::util

#endif  // SC_API_UTIL_BSON_BUILDER_H
