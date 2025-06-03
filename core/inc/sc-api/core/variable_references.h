/**
 * @file
 * @brief
 *
 */

#ifndef SC_API_CORE_VARIABLE_REFERENCES_H
#define SC_API_CORE_VARIABLE_REFERENCES_H

#include "variables.h"

namespace sc_api::core::variable {

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

}  // namespace sc_api::core::variable

#endif  // SC_API_CORE_VARIABLE_REFERENCES_H
