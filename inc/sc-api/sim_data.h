/**
 * @file
 * @brief
 *
 */

#ifndef SC_API_INTERNAL_SIM_DATA_PROPERTIES_H_
#define SC_API_INTERNAL_SIM_DATA_PROPERTIES_H_
#include <sc-api/core/sim_data.h>
#include <sc-api/core/sim_data/participant.h>
#include <sc-api/core/sim_data/session.h>
#include <sc-api/core/sim_data/sim.h>
#include <sc-api/core/sim_data/tire.h>
#include <sc-api/core/sim_data/track.h>
#include <sc-api/core/sim_data/vehicle.h>
#include <sc-api/core/sim_data_builder.h>

namespace sc_api {

namespace sim_data {

using namespace core::sim_data;

using UpdateBuilder       = ::sc_api::core::sim_data_builder::SimDataUpdateBuilder;

using VehiclesBuilder     = ::sc_api::core::sim_data_builder::TypedListBuilder<VehiclePropertyClass>;
using VehicleBuilder      = ::sc_api::core::sim_data_builder::TypedBuilder<VehiclePropertyClass>;
using ParticipantsBuilder = ::sc_api::core::sim_data_builder::TypedNumIdListBuilder<ParticipantPropertyClass>;
using ParticipantBuilder  = ::sc_api::core::sim_data_builder::TypedBuilder<ParticipantPropertyClass>;
using TracksBuilder       = ::sc_api::core::sim_data_builder::TypedListBuilder<TrackPropertyClass>;
using TrackBuilder        = ::sc_api::core::sim_data_builder::TypedBuilder<TrackPropertyClass>;
using TiresBuilder        = ::sc_api::core::sim_data_builder::TypedNumIdListBuilder<TirePropertyClass>;
using TireBuilder         = ::sc_api::core::sim_data_builder::TypedBuilder<TirePropertyClass>;
using SimBuilder          = ::sc_api::core::sim_data_builder::TypedBuilder<SimPropertyClass>;
using SessionsBuilder     = ::sc_api::core::sim_data_builder::TypedListBuilder<SessionPropertyClass>;
using SessionBuilder      = ::sc_api::core::sim_data_builder::TypedBuilder<SessionPropertyClass>;

}  // namespace sim_data

}  // namespace sc_api

#endif  // SC_API_INTERNAL_SIM_DATA_PROPERTIES_H_
