/**
 * @file
 * @brief
 *
 */

#ifndef SC_API_CORE_SIM_DATA_TRACK_PROPERTIES_GENERATED_H_
#define SC_API_CORE_SIM_DATA_TRACK_PROPERTIES_GENERATED_H_
#include <cstdint>

#include "sc-api/core/property_reference.h"

namespace sc_api::core::sim_data::track {

extern const TrackPropertyRef<int32_t> sector_count;

extern const TrackPropertyRef<double> track_length;

extern const TrackPropertyRef<double> pitlane_speed_limit;

extern const TrackPropertyRef<bool> has_joker;

extern const TrackPropertyRef<std::string_view> name;

extern const TrackPropertyRef<std::string_view> base_name;

extern const TrackPropertyRef<std::string_view> variant;

extern const TrackPropertyRef<std::string_view> country;

extern const TrackPropertyRef<std::string_view> track_style;



}  // namespace sc_api::core::sim_data::track

#endif  // SC_API_CORE_SIM_DATA_TRACK_PROPERTIES_GENERATED_H_
