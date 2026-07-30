/**
 * @file
 * @brief
 *
 */

#ifndef SC_API_VARIABLE_REFERENCES_H
#define SC_API_VARIABLE_REFERENCES_H

#include "variables.h"

namespace sc_api::variable {

namespace state {

inline constexpr DeviceVariableReference<bool> active{"state.active"};
inline constexpr DeviceVariableReference<bool> fault{"state.fault"};

}  // namespace state

namespace activepedal {

inline constexpr DeviceVariableReference<bool> using_position_input{"ap.using_position_input"};

inline constexpr DeviceVariableReference<float> primary_input{"ap.primary_input"};
inline constexpr DeviceVariableReference<float> ext_pedal1_input{"ap.ext_pedal1_input"};
inline constexpr DeviceVariableReference<float> ext_pedal2_input{"ap.ext_pedal2_input"};

inline constexpr DeviceVariableReference<float> deadzone_low{"ap.deadzone.low"};
inline constexpr DeviceVariableReference<float> deadzone_high{"ap.deadzone.high"};
inline constexpr DeviceVariableReference<float> pedal_face_pos_mm{"ap.pedal_face_pos_mm"};
inline constexpr DeviceVariableReference<float> abs_pedal_face_pos_mm{"ap.abs_pedal_face_pos_mm"};
inline constexpr DeviceVariableReference<float> pedal_face_travel_mm{"ap.pedal_face_travel_mm"};

inline constexpr DeviceVariableReference<float> force{"ap.force_N"};
inline constexpr DeviceVariableReference<float> force_no_effect_filter{"ap.force_no_efilter_N"};
inline constexpr DeviceVariableReference<float> force_effect_offset{"ap.force_effect_offset_N"};
inline constexpr DeviceVariableReference<float> pos_effect_offset_mm{"ap.pos_effect_offset_mm"};

}  // namespace activepedal

namespace wirelesswheel {

inline constexpr DeviceVariableReference<bool> connected{"ww.connected"};

inline constexpr DeviceVariableReference<float> analog_input[4]{
    {"ww.analog_input0"}, {"ww.analog_input1"}, {"ww.analog_input2"}, {"ww.analog_input3"}};

inline constexpr DeviceVariableReference<uint32_t> digital_inputs[4]{
    {"ww.digital_inputs0"}, {"ww.digital_inputs1"}, {"ww.digital_inputs2"}, {"ww.digital_inputs3"}};

inline constexpr DeviceVariableReference<float> bite_point{"ww.bite_point"};

}  // namespace wirelesswheel

namespace wheel {

inline constexpr DeviceVariableReference<bool> connected{"connected"};

inline constexpr DeviceVariableReference<float> analog_input[4]{
    {"analog_input0"}, {"analog_input1"}, {"analog_input2"}, {"analog_input3"}};

inline constexpr DeviceVariableReference<uint32_t> digital_inputs[4]{
    {"digital_inputs0"}, {"digital_inputs1"}, {"digital_inputs2"}, {"digital_inputs3"}};

inline constexpr DeviceVariableReference<float> bite_point{"bite_point"};

}  // namespace wheel

namespace sc2 {

inline constexpr DeviceVariableReference<float> profile_max_torque{"sc2.profile_max_torque_Nm"};

}

namespace wheelbase {

/** Full wheelbase rotation range in degrees
 *
 * For example 360 means that wheel can be rotated 180 degrees to both directorions.
 */
inline constexpr DeviceVariableReference<float> range{"wb.range"};

/** Effect pipeline torque in Nm. Positive values are torque in clockwise direction */
inline constexpr DeviceVariableReference<float> pipeline_torque{"wb.pipeline_torque_Nm"};

/** Rotation velocity in RPM. Positive values are rotaion in clockwise direction */
inline constexpr DeviceVariableReference<float> velocity{"wb.velocity_rpm"};

/** Wheel rotation in range [-1.0, 1.0] relative to the center position and the range */
inline constexpr DeviceVariableReference<float> input{"wb.wheel_rotation"};

/** Wheel rotation in degrees relative to the center position */
inline constexpr DeviceVariableReference<float> position{"wb.motor_position_deg"};

/** Is device in normal operating mode with full torque range available?
 *
 *  User has to explicitly request full forces before full capabilities of the wheelbase are available.
 *  When wheelbase is in safe mode, the maximum torque is limited to
 */
inline constexpr DeviceVariableReference<bool> high_torque_enabled{"wb.high_torque"};

/** User defined maximum torque limit. Wheelbase won't produce torque effects higher than this limit
 *
 * Direct input effects are also scaled to this value when in normal operation mode. If device is in safe mode,
 * the actual limit is usually lower than this value.
 */
inline constexpr DeviceVariableReference<float> profile_max_torque{"wb.profile_max_torque_Nm"};

}  // namespace wheelbase

namespace sc3 = wheelbase;

}  // namespace sc_api::variable

#endif  // SC_API_VARIABLE_REFERENCES_H
