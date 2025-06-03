#include "sc-api/core/sim_data/session.h"

namespace sc_api::core::sim_data::session {

const SessionPropertyRef<int32_t> player_participant_id{"player_participant_id"};
const SessionPropertyRef<std::string_view> player_vehicle_id{"player_vehicle_id"};
const SessionPropertyRef<std::string_view> track_id{"track_id"};
const SessionPropertyRef<std::string_view> session_type{"session_type"};
const SessionPropertyRef<std::string_view> session_name{"session_name"};


}  // namespace sc_api::core::sim_data::session
