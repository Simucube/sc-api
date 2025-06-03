/**
 * @file
 * @brief
 *
 */

#ifndef SC_API_CORE_SIM_DATA_VEHICLE_PROPERTIES_GENERATED_H_
#define SC_API_CORE_SIM_DATA_VEHICLE_PROPERTIES_GENERATED_H_
#include <cstdint>

#include "sc-api/core/property_reference.h"

namespace sc_api::core::sim_data::vehicle {

/** Engine RPM when idle */
extern const VehiclePropertyRef<double> engine_idle_rpm;

/** Engine redline RPM */
extern const VehiclePropertyRef<double> engine_redline_rpm;

/** Engine RPM when first shifting light appears. If between [0 - 1] then this is considered to be relative to max rpm value. */
extern const VehiclePropertyRef<double> shift_light_first_rpm;

/** Engine RPM when last shifting light is lit. If between [0 - 1] then this is considered to be relative to max rpm value. */
extern const VehiclePropertyRef<double> shift_light_last_rpm;

/** Engine RPM when shifting lights should blink or otherwise indicate that RPM is too high. If between [0 - 1] then this is considered to be relative to max rpm value. */
extern const VehiclePropertyRef<double> shift_light_blink_rpm;

/** Number of degrees of rotation that the car steering wheel has from fully left to fully right */
extern const VehiclePropertyRef<double> steering_wheel_rotation_deg;

/** Number of forward gears the gearbox has */
extern const VehiclePropertyRef<int32_t> gearbox_forward_gears;

/** Number of backward/reverse gears the gearbox has */
extern const VehiclePropertyRef<int32_t> gearbox_backward_gears;

/** True, if this vehicle id is not actually uniquelly identifiable.
This should only be used when adapting telemetry from a sim that does not offer good way to identify the vehicle. In that case a single vehicle id should be used with this set as true. */
extern const VehiclePropertyRef<bool> non_unique;

/** Car has drag reduction system */
extern const VehiclePropertyRef<bool> has_drs;

/** Car has anti-lock brakes */
extern const VehiclePropertyRef<bool> has_abs;

/** Car has traction control system */
extern const VehiclePropertyRef<bool> has_tc;

/** Name of the car */
extern const VehiclePropertyRef<std::string_view> name;

/** Shorter name of the car */
extern const VehiclePropertyRef<std::string_view> short_name;

/** Model name of the car */
extern const VehiclePropertyRef<std::string_view> model;

/** Brand name of the car */
extern const VehiclePropertyRef<std::string_view> brand;

/** Name of the car class */
extern const VehiclePropertyRef<std::string_view> class_name;



}  // namespace sc_api::core::sim_data::vehicle

#endif  // SC_API_CORE_SIM_DATA_VEHICLE_PROPERTIES_GENERATED_H_
