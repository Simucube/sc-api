#ifndef SC_API_COMMAND_PARSING_H
#define SC_API_COMMAND_PARSING_H
#include "sc-api/core/util/bson_reader.h"

namespace sc_api::core {

int32_t parseCommandResultHeader(util::BsonReader& reader, std::string_view& command_name_out);

}  // namespace sc_api::core

#endif  // SC_API_COMMAND_PARSING_H
