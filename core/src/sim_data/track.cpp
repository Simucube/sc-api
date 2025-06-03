#include "sc-api/core/sim_data/track.h"

namespace sc_api::core::sim_data::track {

const TrackPropertyRef<int32_t> sector_count{"sector_count"};
const TrackPropertyRef<double> track_length{"track_length"};
const TrackPropertyRef<double> pitlane_speed_limit{"pitlane_speed_limit"};
const TrackPropertyRef<bool> has_joker{"has_joker"};
const TrackPropertyRef<std::string_view> name{"name"};
const TrackPropertyRef<std::string_view> base_name{"base_name"};
const TrackPropertyRef<std::string_view> variant{"variant"};
const TrackPropertyRef<std::string_view> country{"country"};
const TrackPropertyRef<std::string_view> track_style{"track_style"};


}  // namespace sc_api::core::sim_data::track
