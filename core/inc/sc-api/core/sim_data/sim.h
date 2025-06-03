/**
 * @file
 * @brief
 *
 */

#ifndef SC_API_CORE_SIM_DATA_SIM_PROPERTIES_GENERATED_H_
#define SC_API_CORE_SIM_DATA_SIM_PROPERTIES_GENERATED_H_
#include <cstdint>

#include "sc-api/core/property_reference.h"

namespace sc_api::core::sim_data::sim {

/** Simulator process startup can be detected */
extern const SimPropertyRef<bool> process_detection;

/** Engine max RPM available */
extern const SimPropertyRef<bool> max_rpm_available;

/** Shift light level available */
extern const SimPropertyRef<bool> full_shift_light_data_available;

extern const SimPropertyRef<bool> vehicle_detection_support;

/** Name of the simulator */
extern const SimPropertyRef<std::string_view> name;



}  // namespace sc_api::core::sim_data::sim

#endif  // SC_API_CORE_SIM_DATA_SIM_PROPERTIES_GENERATED_H_
