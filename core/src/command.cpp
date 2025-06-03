#include "sc-api/core/command.h"

#include <cassert>
#include <cstring>

namespace sc_api::core {

CommandRequest::CommandRequest() {}

CommandRequest::CommandRequest(std::string_view service, std::string_view command) : util::BsonBuilder() {
    initialize(service, command);
}

CommandRequest::~CommandRequest() {}

void CommandRequest::initialize(std::string_view service, std::string_view command) {
    // Reserve some space ahead of time
    buffer_.resize(100);
    BsonBuilder::initialize(&buffer_);

    docAddElement("00type", 1);
    docAddElement("service", service);
    docBeginSubDoc("cmd");
    docBeginSubDoc(command);
}

void CommandRequest::initializeFrom(std::string_view service, std::string_view command, const uint8_t* content_bson) {
    assert(content_bson);
    // Reserve some space ahead of time
    int32_t content_size;
    std::memcpy(&content_size, content_bson, sizeof(content_size));
    assert(content_size >= 5);

    buffer_.resize(100 + content_size);
    BsonBuilder::initialize(&buffer_);

    docAddElement("00type", 1);
    docAddElement("service", service);
    docBeginSubDoc("cmd");
    docBeginSubDoc(command, content_bson);
}

std::vector<uint8_t> CommandRequest::finalize(int32_t cmd_id) {
    assert(getDocumentDepth() == 2 && "Mismatch of begin and end document operations");

    endDocument();
    endDocument();
    docAddElement("user-data", cmd_id);

    auto [ptr, size] = finish();
    buffer_.resize(size);
    return std::move(buffer_);
}

}  // namespace sc_api::core
