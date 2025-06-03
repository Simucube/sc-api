#ifndef SC_API_UTIL_BSON_READER_H
#define SC_API_UTIL_BSON_READER_H
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace sc_api::core::util {
/** Helper class for validating and parsing BSON data */
class BsonReader {
public:
    /** Refers to an element within a document
     *
     * Returned by elementOffset() and must not be modified.
     */
    struct ElementOffset {
        int32_t offset;
        int32_t arr_index;
    };

    enum ElementType {
        ELEMENT_DOUBLE             = 0x01,
        ELEMENT_STR                = 0x02,
        ELEMENT_DOC                = 0x03,
        ELEMENT_ARRAY              = 0x04,
        ELEMENT_BINARY             = 0x05,
        ELEMENT_BOOL               = 0x08,
        ELEMENT_NULL               = 0x0A,
        ELEMENT_I32                = 0x10,
        ELEMENT_I64                = 0x12,

        /** Type that is returned when there isn't element available. Not actual value */
        ELEMENT_END                = 0x00,

        ELEMENT_UNSUPPORTED        = -1,
        ELEMENT_FORMAT_ERROR       = -2,

        ELEMENT_INTERNAL_DOC_BEGIN = -3,
    };

    /** Expect that the buffer contains valid BSON document and use that to determine valid size
     *
     * Valid BSON document always starts with int32_t size of the document
     *
     * Unsafe as buffer size isn't validated to be at least size of the document.
     * Prefer BsonReader(const uint8_t* buffer, std::size_t size) when dealing with unvalidated data.
     */
    explicit BsonReader(const uint8_t* buffer) noexcept;
    explicit BsonReader(const uint8_t* buffer, std::size_t size) noexcept;
    ~BsonReader() noexcept = default;

    /** Return BsonReader referring to valid but completely empty BSON */
    BsonReader() noexcept;

    BsonReader(const BsonReader& r) noexcept            = default;
    BsonReader& operator=(const BsonReader& r) noexcept = default;

    /** Is current element ELEMENT_END or error value? */
    bool                  atEnd() const noexcept { return cur_type_ == 0 || error(); }
    bool                  error() const noexcept { return cur_type_ != ELEMENT_INTERNAL_DOC_BEGIN && cur_type_ < 0; }
    static constexpr bool isError(ElementType e) { return e != ELEMENT_INTERNAL_DOC_BEGIN && e < 0; }
    static constexpr bool isEndOrError(ElementType e) { return e != ELEMENT_INTERNAL_DOC_BEGIN && e <= 0; }

    /** Read next element
     *
     * \return ELEMENT_END, if there isn't more elements within current document
     *         ELEMENT_UNSUPPORTED, if element has unsupported type
     *         ELEMENT_FORMAT_ERROR, if element seems to have unsupported format
     *         Element type otherwise
     */
    ElementType next() noexcept;

    /** Tries to find element within the current document by given key
     *
     * \param key Key that is being searched
     * \return ELEMENT_END, if key isn't found. The current offset is not changed
     *         ELEMENT_UNSUPPORTED, if element has unsupported type
     *         ELEMENT_FORMAT_ERROR, if element seems to have unsupported format
     *         Element type otherwise. key() will match the key argument
     */
    ElementType seekKey(std::string_view key) noexcept;

    /** Find the next key but only search forward from the current position
     *
     * Will not find key if it has been passed by previous seek or next call.
     *
     * \param key Key that is being searched
     * \return ELEMENT_END, if key isn't found. The current offset is not changed
     *         ELEMENT_UNSUPPORTED, if element has unsupported type
     *         ELEMENT_FORMAT_ERROR, if element seems to have unsupported format
     *         Element type otherwise. key() will match the key argument
     *
     */
    ElementType seekNextKey(std::string_view key) noexcept;

    /** Seek to array index. Current document must be array type
     *
     * \return ELEMENT_END, index is outside bounds of the array
     *         ELEMENT_UNSUPPORTED, if element has unsupported type
     *         ELEMENT_FORMAT_ERROR, if element seems to have unsupported format
     *         Element type otherwise.
     */
    ElementType seekIndex(int32_t index) noexcept;

    /** Get offset of the current element */
    ElementOffset elementOffset() const noexcept { return {element_start_offset_, arr_index_}; }

    /** Seek to element in the offset previously found by elementOffset. Offset must be within the current document
     *
     * \return ELEMENT_END, if key isn't found
     *         ELEMENT_UNSUPPORTED, if element has unsupported type
     *         ELEMENT_FORMAT_ERROR, if element seems to have unsupported format
     *         Element type otherwise. key() will match the key argument
     */
    ElementType seek(ElementOffset offset) noexcept;

    /** Seek to beginning of the current document
     *
     * Call to next() will return the first element within the (sub) document.
     */
    void seekBegin() noexcept;

    /** Returns true, if currently iterated element is an array instead of document. */
    bool currentlyIteratingArray() const { return arr_index_ > -2; }

    /** Array index, only valid when interating array elements (after beginSub with ELEMENT_ARRAY) */
    int32_t index() const noexcept { return arr_index_; }

    /** Document key value. If iterating array, this is base-10 numeric string that matches index()
     *
     * Guaranteed to be null-terminated (which isn't normally required for std::string_view)
     */
    std::string_view key() const noexcept { return key_; }

    /** Get document or array element value start pointer and size with in the read buffer
     *
     * This can be used to create new BsonReader and only iterate data with in the subdocument.
     */
    std::pair<const uint8_t*, std::size_t> subdocument() const noexcept;
    int32_t                                subdocumentOffset() const noexcept { return buf_offset_; }

    /** Start to iterate elements in document or array */
    bool beginSub();

    /** Stop iterating elements in document or array
     *
     * Reading will continue in the outer document or array at the next element after the current array or document
     */
    bool endSub();

    /** Get double value. Current element type must be ELEMENT_DOUBLE */
    double doubleValue() const noexcept;

    /** Get value of ELEMENT_STR
     *
     * Guaranteed to be null-terminated (which isn't normally required for std::string_view), but
     * may also contain nulls
     */
    std::string_view stringValue() const noexcept;
    bool             boolValue() const noexcept;
    int64_t          int64Value() const noexcept;
    int32_t          int32Value() const noexcept;

    /** Converts ELEMENT_DOUBLE, ELEMENT_BOOL, ELEMENT_I32, ELEMENT_I64 to double
     *
     * Will lose precision when the value isn't representable perfectly as double
     * Other data types return 0.0
     *
     */
    double asDouble() const noexcept;

    /** Converts ELEMENT_STR, ELEMENT_BINARY, ELEMENT_DOUBLE, ELEMENT_BOOL, ELEMENT_I32, ELEMENT_I64 to string
     *
     * Other types will return empty string.
     */
    std::string asString() const;

    int32_t asInt32() const noexcept;

    uint8_t                                binaryType() const noexcept;
    std::pair<const uint8_t*, std::size_t> binaryValue() const noexcept;

    /** Get span that represents the current element
     *
     *  First byte will be element type, followed by the key and value
     */
    std::pair<const uint8_t*, std::size_t> rawElement() const noexcept;

    bool tryGetValue(double& v) const noexcept;
    bool tryGetValue(int32_t& v) const noexcept;
    bool tryGetValue(int64_t& v) const noexcept;
    bool tryGetValue(bool& v) const noexcept;
    bool tryGetValue(std::string_view& v) const noexcept;
    bool tryGetValue(std::string& v) const;
    bool tryGetValue(std::pair<const uint8_t*, std::size_t>& v) const noexcept;
    bool tryGetValue(std::vector<uint8_t>& v) const;

    template <typename T>
    bool tryFindAndGet(std::string_view key, T& value_out) {
        BsonReader::ElementType t = seekKey(key);
        return !BsonReader::isEndOrError(t) && tryGetValue(value_out);
    }

    /** Read document size from the given buffer */
    static int32_t getTotalDocumentSize(const uint8_t* buf);

    /** Validate that given buffer contains valid BSON document */
    static bool validate(const uint8_t* buf, std::size_t s);

    /** Validates current subdocument
     *
     * Reads through data recursivelly until ELEMENT_END for the current level is reached
     */
    bool validate();

private:
    struct DocState {
        int32_t begin_offset;
        int32_t end_offset;
        int32_t arr_index_;
    };

    bool readKey();

    static constexpr std::size_t k_max_subdocs = 16;

    DocState doc_layers_[k_max_subdocs];
    int      last_doc_layer_ = -1;

    const uint8_t*   buffer_;
    std::size_t      size_;
    int32_t          element_start_offset_ = 0;
    int32_t          buf_offset_           = 0;
    int32_t          arr_index_            = -2;
    int32_t          cur_doc_begin_        = 0;
    int32_t          cur_doc_end_          = 0;
    ElementType      cur_type_             = ELEMENT_DOC;
    std::string_view key_;
};
}  // namespace sc_api::core::util

#endif  // SC_API_UTIL_BSON_READER_H
