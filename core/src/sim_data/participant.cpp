#include "sc-api/core/sim_data/participant.h"

namespace sc_api::core::sim_data::participant {

const ParticipantPropertyRef<int32_t> vehicle_number{"vehicle_number"};
const ParticipantPropertyRef<int32_t> tire_id_lr{"tire_id_lr"};
const ParticipantPropertyRef<int32_t> tire_id_rr{"tire_id_rr"};
const ParticipantPropertyRef<int32_t> tire_id_lf{"tire_id_lf"};
const ParticipantPropertyRef<int32_t> tire_id_rf{"tire_id_rf"};
const ParticipantPropertyRef<int32_t> tire_id{"tire_id"};
const ParticipantPropertyRef<bool> in_current_session{"in_current_session"};
const ParticipantPropertyRef<bool> on_track{"on_track"};
const ParticipantPropertyRef<std::string_view> name{"name"};
const ParticipantPropertyRef<std::string_view> abbrev_name{"abbrev_name"};
const ParticipantPropertyRef<std::string_view> team_name{"team_name"};
const ParticipantPropertyRef<std::string_view> vehicle_id{"vehicle_id"};


}  // namespace sc_api::core::sim_data::participant
