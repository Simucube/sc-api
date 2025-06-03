/**
 * @file
 * @brief
 *
 */

#ifndef SC_API_CORE_TIME_H_
#define SC_API_CORE_TIME_H_
#include <chrono>
#include <cstdint>

/** API functions for measuring and calculating timestamps for commands requiring them
 *
 * Simucube API has commands that cause effects at specific precise timepoint. All devices are synchronized to same
 * clock which allows specifying timestamps that all connected devices can use.
 */
namespace sc_api::core {

/** Low level API that is used to fetch timestamp from the clock that SC-API uses */
namespace clock_source {

/** Get current timestamp of the monotonic clock that is used as reference time for the API commands
 *
 * The timestamp tick rate can be queried with getTimestampFrequency.
 * This clock source is synchronized to all connected devices.
 *
 * QueryPerformanceCounter is used to implement this and has usually 10MHz tick rate giving
 * 100ns timestamp accuracy.
 */
int64_t getTimestamp();

/** Get how much the timestamp value increases within a second. */
int64_t getTimestampFrequencyHz();

}  // namespace clock_source

/** Clock that is used to represent time in SC-API. Fills C++ TrivialClock requirements.
 *
 * On MSVC compiler currently this is pretty much identical to std::chrono::high_resolution_clock, but we define our
 * own type to make sure that we always use the same clock source as the backend
 */
struct Clock {
    using rep                       = std::int64_t;
    using period                    = std::nano;
    using duration                  = std::chrono::nanoseconds;
    using time_point                = std::chrono::time_point<Clock>;
    static constexpr bool is_steady = true;

    [[nodiscard]] static time_point now() noexcept {
        std::int64_t freq                = clock_source::getTimestampFrequencyHz();
        std::int64_t ticks               = clock_source::getTimestamp();

        // Optimized for common QPC frequencies
        constexpr std::int64_t freq10MHz = 10000000;
        constexpr std::int64_t freq24MHz = 24000000;
        if (freq == freq10MHz) {
            static_assert(period::den % freq10MHz == 0);
            return time_point(duration(ticks * (period::den / freq10MHz)));
        } else if (freq == freq24MHz) {
            const std::int64_t whole = (ticks / freq24MHz) * period::den;
            const std::int64_t part  = (ticks % freq24MHz) * period::den / freq24MHz;
            return time_point(duration(whole + part));
        } else {
            const std::int64_t whole = (ticks / freq) * period::den;
            const std::int64_t part  = (ticks % freq) * period::den / freq;
            return time_point(duration(whole + part));
        }
    }
};

}  // namespace sc_api::core

#endif  // SC_API_CORE_TIME_H_
