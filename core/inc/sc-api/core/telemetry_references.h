/**
 * @file
 * @brief
 *
 */

#ifndef SC_API_TELEMETRY_REFS_GENERATED_H_
#define SC_API_TELEMETRY_REFS_GENERATED_H_
#include <cstdint>
#include <string_view>

#include "sc-api/core/telemetry.h"

namespace sc_api::core {

namespace telemetry {

/** Is game physics currently running -> Should we generate telemetry effects */
inline constexpr TelemetryReference<bool> physics_running{{ "physics_running" }};

/** True, when the engine is running */
inline constexpr TelemetryReference<bool> engine_running{{ "engine_running" }};

/** True, when the ignition is on */
inline constexpr TelemetryReference<bool> ignition_on{{ "ignition_on" }};

/** True, if ABS is currently reducing brake pressure */
inline constexpr TelemetryReference<bool> abs_active{{ "abs_active" }};

/** True, when drag reduction system is activated */
inline constexpr TelemetryReference<bool> drs_active{{ "drs_active" }};

/** True, when drag reduction system can be activated after the next activation line. */
inline constexpr TelemetryReference<bool> drs_available{{ "drs_available" }};

/** True, when drag reduction system can currently be activated by pressing DRS button */
inline constexpr TelemetryReference<bool> drs_activatable{{ "drs_activatable" }};

/** True, when wipers are enabled */
inline constexpr TelemetryReference<bool> wipers_enabled{{ "wipers_enabled" }};

/** True, when low beam lights are enabled */
inline constexpr TelemetryReference<bool> low_beam_enabled{{ "low_beam_enabled" }};

/** True, when high beam lights are enabled */
inline constexpr TelemetryReference<bool> high_beam_enabled{{ "high_beam_enabled" }};

/** True when left blinker light is on. Should generally keep toggling while turn signal is active */
inline constexpr TelemetryReference<bool> left_blinker_light_on{{ "left_blinker_light_on" }};

/** True when right blinker light is on. Should generally keep toggling while turn signal is active */
inline constexpr TelemetryReference<bool> right_blinker_light_on{{ "right_blinker_light_on" }};

/** True when left turn signal is active. */
inline constexpr TelemetryReference<bool> left_turn_signal_active{{ "left_turn_signal_active" }};

/** True when right turn signal is active. */
inline constexpr TelemetryReference<bool> right_turn_signal_active{{ "right_turn_signal_active" }};

/** True, when pit limiter is activated */
inline constexpr TelemetryReference<bool> pit_limiter_active{{ "pit_limiter_active" }};

/** Traction control is currently active and braking some wheel. */
inline constexpr TelemetryReference<bool> tc_active{{ "tc_active" }};

/** Spotter detects a car on the left side */
inline constexpr TelemetryReference<bool> spotter_car_on_left{{ "spotter_car_on_left" }};

/** Spotter detects a car on the right side */
inline constexpr TelemetryReference<bool> spotter_car_on_right{{ "spotter_car_on_right" }};

/** Spotter detects a second car on the left side */
inline constexpr TelemetryReference<bool> spotter_second_car_on_left{{ "spotter_second_car_on_left" }};

/** Spotter detects a second car on the right side */
inline constexpr TelemetryReference<bool> spotter_second_car_on_right{{ "spotter_second_car_on_right" }};

/** When shift lights should blink to indicate over rev, explicit flag */
inline constexpr TelemetryReference<bool> shift_light_over_rev{{ "shift_light_over_rev" }};

/** Green flag */
inline constexpr TelemetryReference<bool> flag_green{{ "flag_green" }};

/** Yellow flag is active in the current sector */
inline constexpr TelemetryReference<bool> flag_yellow{{ "flag_yellow" }};

/** Full track yellow flag is active */
inline constexpr TelemetryReference<bool> flag_full_yellow{{ "flag_full_yellow" }};

/** Blue flag is waved for the player */
inline constexpr TelemetryReference<bool> flag_blue{{ "flag_blue" }};

/** Black flag is waved for the player */
inline constexpr TelemetryReference<bool> flag_black{{ "flag_black" }};

/** Red flag is waved for the player */
inline constexpr TelemetryReference<bool> flag_red{{ "flag_red" }};

/** White flag is waved for the player */
inline constexpr TelemetryReference<bool> flag_white{{ "flag_white" }};

/** Checkered flag is waved for the player */
inline constexpr TelemetryReference<bool> flag_checkered{{ "flag_checkered" }};

/** half black, half white flag is waved for the player */
inline constexpr TelemetryReference<bool> flag_black_white{{ "flag_black_white" }};

/** True, when current lap is valid */
inline constexpr TelemetryReference<bool> current_lap_valid{{ "current_lap_valid" }};

/** Engine RPM */
inline constexpr TelemetryReference<float> engine_rpm{{ "engine_rpm" }};

/** Player car forward local velocity in m/s */
inline constexpr TelemetryReference<float> local_velocity_x{{ "local_velocity_x" }};

/** Player car local velocity in m/s */
inline constexpr TelemetryReference<float> local_velocity_y{{ "local_velocity_y" }};

/** Player car upward local velocity in m/s */
inline constexpr TelemetryReference<float> local_velocity_z{{ "local_velocity_z" }};

/** Player car forward local acceleration in m/s^2 */
inline constexpr TelemetryReference<float> local_acceleration_x{{ "local_acceleration_x" }};

/** Player car leftwards local acceleration in m/s^2 */
inline constexpr TelemetryReference<float> local_acceleration_y{{ "local_acceleration_y" }};

/** Player car upwards local acceleration in m/s^2 */
inline constexpr TelemetryReference<float> local_acceleration_z{{ "local_acceleration_z" }};

/** Player car rotation +yaw, clockwise (looking car from up), in radians. Range 0 to 2*PI */
inline constexpr TelemetryReference<float> local_rotation_yaw{{ "local_rotation_yaw" }};

/** Player car rotation +pitch, front upwards, rear down, in radians */
inline constexpr TelemetryReference<float> local_rotation_pitch{{ "local_rotation_pitch" }};

/** Player car rotation +roll, left-side up, right-side down, in radians */
inline constexpr TelemetryReference<float> local_rotation_roll{{ "local_rotation_roll" }};

/** Player car angular velocity +yaw, clockwise (looking car from up), in rad/s */
inline constexpr TelemetryReference<float> local_angular_velocity_yaw{{ "local_angular_velocity_yaw" }};

/** Player car angular velocity +pitch, front upwards, rear down, in rad/s */
inline constexpr TelemetryReference<float> local_angular_velocity_pitch{{ "local_angular_velocity_pitch" }};

/** Player car angular velocity +roll, left-side up, right-side down, in rad/s */
inline constexpr TelemetryReference<float> local_angular_velocity_roll{{ "local_angular_velocity_roll" }};

/** Front-left wheel angular velocity in radians/s */
inline constexpr TelemetryReference<float> wheel_ang_vel_fl{{ "wheel_ang_vel_fl" }};

/** Front-right wheel angular velocity in radians/s */
inline constexpr TelemetryReference<float> wheel_ang_vel_fr{{ "wheel_ang_vel_fr" }};

/** Rear-left wheel angular velocity in radians/s */
inline constexpr TelemetryReference<float> wheel_ang_vel_rl{{ "wheel_ang_vel_rl" }};

/** Rear-right wheel angular velocity in radians/s */
inline constexpr TelemetryReference<float> wheel_ang_vel_rr{{ "wheel_ang_vel_rr" }};

/** Front side brake balance percentage 0 - 100 */
inline constexpr TelemetryReference<float> brake_balance_front{{ "brake_balance_front" }};

/** Brake input that the simulator uses 0 - 1 */
inline constexpr TelemetryReference<float> brake_input{{ "brake_input" }};

/** Player lap time in seconds from the previous completed lap */
inline constexpr TelemetryReference<float> lap_time_previous{{ "lap_time_previous" }};

/** Player lap time in seconds from the best completed lap */
inline constexpr TelemetryReference<float> lap_time_best{{ "lap_time_best" }};

/** Seconds that player has been on the current lap */
inline constexpr TelemetryReference<float> lap_time_current{{ "lap_time_current" }};

/** Predicted final laptime for the current lap */
inline constexpr TelemetryReference<float> lap_time_estimate{{ "lap_time_estimate" }};

/** Car forward velocity as measured by the speedometer (may differ from the "gps" speed local_velocity_x) */
inline constexpr TelemetryReference<float> speed{{ "speed" }};

/** Time in the virtual world in seconds from midnight */
inline constexpr TelemetryReference<float> time{{ "time" }};

/** Engine idle RPM */
inline constexpr TelemetryReference<int16_t> engine_idle_rpm{{ "engine_idle_rpm" }};

/** Max RPM that engine can safely handle */
inline constexpr TelemetryReference<int16_t> engine_max_rpm{{ "engine_max_rpm" }};

/** Engine RPM level where first shift light should light up */
inline constexpr TelemetryReference<int16_t> shift_light_start_rpm{{ "shift_light_start_rpm" }};

/** Engine RPM level where last shift light should light up and up shift should be done */
inline constexpr TelemetryReference<int16_t> shift_light_end_rpm{{ "shift_light_end_rpm" }};

/** When shift lights should blink to indicate over rev */
inline constexpr TelemetryReference<int16_t> shift_light_over_rev_rpm{{ "shift_light_over_rev_rpm" }};

/** Engine map that is currently used */
inline constexpr TelemetryReference<int8_t> engine_map{{ "engine_map" }};

/** Selected transmission gear (0 == neutral, -1 = reverse, 1 = first gear,...) */
inline constexpr TelemetryReference<int8_t> transmission_gear{{ "transmission_gear" }};

/** Current ABS setting. */
inline constexpr TelemetryReference<int8_t> abs_setting{{ "abs_setting" }};

/** Current TC setting. */
inline constexpr TelemetryReference<int8_t> tc_setting{{ "tc_setting" }};

/** Current TC2 setting */
inline constexpr TelemetryReference<int8_t> tc_setting2{{ "tc_setting2" }};

/** Fraction of the the shifting lights lit. 0 == all lights off, 255 == all lights on */
inline constexpr TelemetryReference<uint8_t> shift_light_level{{ "shift_light_level" }};


}  // namespace telemetry
}  // namespace sc_api::core

#endif  // SC_API_TELEMETRY_REFS_GENERATED_H_
