/**
 * @file
 * @brief
 *
 */

#ifndef SC_API_CORE_SIM_DATA_SESSION_PROPERTIES_GENERATED_H_
#define SC_API_CORE_SIM_DATA_SESSION_PROPERTIES_GENERATED_H_
#include <cstdint>

#include "sc-api/core/property_reference.h"

namespace sc_api::core::sim_data::session {

extern const SessionPropertyRef<int32_t> player_participant_id;

extern const SessionPropertyRef<std::string_view> player_vehicle_id;

extern const SessionPropertyRef<std::string_view> track_id;

extern const SessionPropertyRef<std::string_view> session_type;

extern const SessionPropertyRef<std::string_view> session_name;



}  // namespace sc_api::core::sim_data::session

#endif  // SC_API_CORE_SIM_DATA_SESSION_PROPERTIES_GENERATED_H_
