#pragma once
#include <sc-api/result.h>

void throw_on_error(sc_api::ResultCode rc);
[[noreturn]] void throw_internal_error(const char* msg);
