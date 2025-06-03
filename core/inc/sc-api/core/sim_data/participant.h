/**
 * @file
 * @brief
 *
 */

#ifndef SC_API_CORE_SIM_DATA_PARTICIPANT_PROPERTIES_GENERATED_H_
#define SC_API_CORE_SIM_DATA_PARTICIPANT_PROPERTIES_GENERATED_H_
#include <cstdint>

#include "sc-api/core/property_reference.h"

namespace sc_api::core::sim_data::participant {

extern const ParticipantPropertyRef<int32_t> vehicle_number;

extern const ParticipantPropertyRef<int32_t> tire_id_lr;

extern const ParticipantPropertyRef<int32_t> tire_id_rr;

extern const ParticipantPropertyRef<int32_t> tire_id_lf;

extern const ParticipantPropertyRef<int32_t> tire_id_rf;

extern const ParticipantPropertyRef<int32_t> tire_id;

extern const ParticipantPropertyRef<bool> in_current_session;

extern const ParticipantPropertyRef<bool> on_track;

extern const ParticipantPropertyRef<std::string_view> name;

extern const ParticipantPropertyRef<std::string_view> abbrev_name;

extern const ParticipantPropertyRef<std::string_view> team_name;

extern const ParticipantPropertyRef<std::string_view> vehicle_id;



}  // namespace sc_api::core::sim_data::participant

#endif  // SC_API_CORE_SIM_DATA_PARTICIPANT_PROPERTIES_GENERATED_H_
