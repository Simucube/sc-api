#pragma once
#include <sc-api/result.h>

#include <string_view>

void              throw_on_error(sc_api::ResultCode rc);
/// Like throw_on_error(rc), with `detail` (such as Tuner's error message) appended to the exception message.
void              throw_on_error(sc_api::ResultCode rc, std::string_view detail);
[[noreturn]] void throw_internal_error(const char* msg);
