/**
 * @file
 * @brief
 *
 */

#ifndef SC_API_CORE_COMMAND_H_
#define SC_API_CORE_COMMAND_H_
#include "session_fwd.h"
#include "util/bson_builder.h"

namespace sc_api::core {

/** Class for creating command request
 *
 * Command requests are BSON formatted data and this class is wrapper around
 * util::BsonBuilder to make it easier to create correctly formatted command
 * request.
 *
 * Execute command by calling Session::asyncCommand
 */
class CommandRequest : protected util::BsonBuilder {
    friend class Session;

public:
    /** Default initialize CommandRequest.
     *
     * initialize or initializeFrom must be called before starting to construct request body */
    CommandRequest();

    /** */
    CommandRequest(std::string_view service, std::string_view command);
    ~CommandRequest();

    using BsonBuilder::arrayAddElement;
    using BsonBuilder::arrayBeginSubDoc;
    using BsonBuilder::docAddElement;
    using BsonBuilder::docAddSubDoc;
    using BsonBuilder::docBeginSubArray;
    using BsonBuilder::docBeginSubDoc;
    using BsonBuilder::endArray;
    using BsonBuilder::endDocument;

    /** Clear any previously constructed command and begins new */
    void initialize(std::string_view service, std::string_view command);

    /** Initializes command request with existing command data
     *
     * Leaves the sub document open allowing adding more fields after the given content.
     *
     * @param service Id name of the service that should handle this command
     * @param command Command id
     * @param content_bson Pointer to valid BSON document that is used to initialize the body of the request
     */
    void initializeFrom(std::string_view service, std::string_view command, const uint8_t* content_bson);

private:
    /** Called by Session::asyncCommand to add final transaction specific fields to the request data and to steal
     *  this request buffer to avoid unnecessarily copying it.
     */
    std::vector<uint8_t> finalize(int32_t cmd_id);

    std::vector<uint8_t> buffer_;
};

}  // namespace sc_api::core

#endif  // SC_API_COMMAND_H_
