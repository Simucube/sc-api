#include "command_parsing.h"
#include <iostream>

namespace sc_api::core {
using util::BsonReader;

int32_t parseCommandResultHeader(util::BsonReader& reader, std::string_view& command_name_out) {
    int32_t return_code = -1;
    if (!reader.tryFindAndGet("result", return_code)) {
        return -1;
    }

    if (return_code != 0) {
        std::string_view error_msg;
        if (reader.tryFindAndGet("error_message", error_msg)) {
            std::cerr << "Command failed: " << error_msg << "\n";
        }
        
        return return_code;
    }

    if (BsonReader::isEndOrError(reader.seekKey("data")) || !reader.beginSub()) {
        return -1;
    }

    if (reader.next() != BsonReader::ELEMENT_DOC) {
        return -1;
    }

    command_name_out = reader.key();

    if (!reader.beginSub()) {
        return -1;
    }

    return 0;
}

}  // namespace sc_api::core
