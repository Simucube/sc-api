#include "sc-api/core/time.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

namespace sc_api::core::clock_source {

static const int64_t s_qpc_frequency_ = []() {
    LARGE_INTEGER qpc_frequency;
    QueryPerformanceFrequency(&qpc_frequency);
    return qpc_frequency.QuadPart;
}();

int64_t getTimestamp() {
    LARGE_INTEGER qpc_value;
    QueryPerformanceCounter(&qpc_value);
    return qpc_value.QuadPart;
}

int64_t getTimestampFrequencyHz() { return s_qpc_frequency_; }

}  // namespace sc_api::core::clock_source

#else

namespace sc_api::core::clock_source {

int64_t getTimestamp() { return 0; }

int64_t getTimestampFrequencyHz() { return 1; }

}  // namespace sc_api::core::clock_source

#endif
