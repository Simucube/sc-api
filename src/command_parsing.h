#ifndef SC_API_COMMAND_PARSING_H
#define SC_API_COMMAND_PARSING_H
#include "sc-api/util/bson_reader.h"

namespace sc_api {

int32_t parseCommandResultHeader(util::BsonReader& reader, std::string_view& command_name_out);

}  // namespace sc_api

#endif  // SC_API_COMMAND_PARSING_H
