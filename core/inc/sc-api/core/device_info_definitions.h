/**
 * @file
 * @brief
 *
 */

#ifndef SC_API_DEVICE_INFO_DEFINITIONS_H_
#define SC_API_DEVICE_INFO_DEFINITIONS_H_

#include <string_view>

namespace sc_api::core {
namespace device_info {

/** Device roles define intended use for the whole device and the type of the device. */
enum class DeviceRole {
    wheel,
    wheelbase,
    throttle_pedal,
    brake_pedal,
    handbrake,
    clutch_pedal,
    gear_stick,
    button_box,

    /** Device exists to allow connecting other devices */
    hub,

    /** Device's current role isn't known. For example ActivePedal before initial configuration. */
    unknown,

    /** Used when the device role is some other than what is supported by this API version */
    other,
};
inline constexpr const char* device_role_ids[] = {
    "wheel",
    "wheelbase",
    "throttle_pedal",
    "brake_pedal",
    "handbrake",
    "clutch_pedal",
    "gear_stick",
    "button_box",
    "hub",
    "unknown",
    "other"
,};

constexpr std::string_view toString(DeviceRole r) { return device_role_ids[(int)r]; }
constexpr DeviceRole deviceRoleFromString(std::string_view s) {
    for (int i = 0; i < (int)DeviceRole::other; ++i) {
        if (s == device_role_ids[i]) return (DeviceRole)i;
    }
    return DeviceRole::other;
}



/** Control types define the physical controls of the devices.
 *
 * Controls are physical features of the device.
 *
 * This enum and its numeric ids will change between SC-API versions.
 * Use textual representation to refer these instead of enumerator numeric values.
 */
enum class ControlType {

    /** This control is a wheelbase. */
    wheelbase,

    /** Wheel that is connected to the wheelbase */
    wheel,

    /** Pedal that is intended for pressing with a foot */
    pedal,

    /** Paddle switch */
    paddle,

    /** Hat switch */
    hat_switch,

    /** Button */
    button,

    /** Toggle switch */
    toggle_switch,

    /** Two way directional switch (2 digital inputs) */
    dir_2way,

    /** Four way directional switch (4 digital inputs) */
    dir_4way,

    /** Rotary encoder */
    rot_enc,

    /** Combination of button, 4-way directional and rotary encoder */
    funky_switch,

    /** Controllable light. Only for feedback and doesn't function as input. */
    light,

    /** Used as placeholder for unavailable data. */
    unknown,

    /** Used when the control type is some other than what is supported by this API version */
    other,
};
inline constexpr const char* control_type_ids[] = {
    "wheelbase",
    "wheel",
    "pedal",
    "paddle",
    "hat_switch",
    "button",
    "toggle_switch",
    "dir_2way",
    "dir_4way",
    "rot_enc",
    "funky_switch",
    "light",
    "unknown",
    "other"
,};

constexpr std::string_view toString(ControlType r) { return control_type_ids[(int)r]; }
constexpr ControlType controlTypeFromString(std::string_view s) {
    for (int i = 0; i < (int)ControlType::other; ++i) {
        if (s == control_type_ids[i]) return (ControlType)i;
    }
    return ControlType::other;
}



/** Feedback types define how the simulator can interract with the devices.
 *
 *
 * This enum and its numeric ids will change between SC-API versions.
 * Use textual representation to refer these instead of enumerator numeric values.
 */
enum class FeedbackType {

    /** DirectInput HID force feedback support */
    direct_input,

    /** Force feedback effect pipelines */
    wheelbase,

    /** ActivePedal */
    active_pedal,

    /** Fully controllable rgb light */
    rgb_light,

    /** Light that can only be turned on or off */
    light,

    /** Placeholder for unavailable data. */
    unknown,

    /** Used when the feedback type is some other than what is supported by this API version */
    other,
};
inline constexpr const char* feedback_type_ids[] = {
    "direct_input",
    "wheelbase",
    "active_pedal",
    "rgb_light",
    "light",
    "unknown",
    "other"
,};

constexpr std::string_view toString(FeedbackType r) { return feedback_type_ids[(int)r]; }
constexpr FeedbackType feedbackTypeFromString(std::string_view s) {
    for (int i = 0; i < (int)FeedbackType::other; ++i) {
        if (s == feedback_type_ids[i]) return (FeedbackType)i;
    }
    return FeedbackType::other;
}



/** Input roles define intended usage of the input source
 *
 * This should be used to map device inputs to game controls.
 *
 * This enum and its numeric ids will change between SC-API versions.
 * Use textual representation to refer these instead of enumerator numeric values.
 */
enum class InputRole {
    steering,
    throttle,
    brake,
    clutch,
    clutch_secondary,
    clutch_primary,
    gear_shift,
    gear_shift_up,
    gear_shift_down,
    handbrake,
    ignition,
    starter,
    headlight_toggle,
    high_beam_toggle,
    headlight_flash,
    pit_limiter,
    overtake,
    drs,
    traction_control_toggle,
    traction_control_increase,
    traction_control_decrease,
    tear_off_visor,
    trigger_windshield_wipers,
    toggle_windshield_wipers,
    brake_bias_increase,
    brake_bias_decrease,
    front_antiroll_bar_increase,
    front_antiroll_bar_decrease,
    rear_antiroll_bar_increase,
    rear_antiroll_bar_decrease,
    fuel_map_increase,
    fuel_map_decrease,
    turbo_pressure_increase,
    turbo_pressure_decrease,
    abs_adjust,
    abs_increase,
    abs_decrease,
    front_wing_increase,
    front_wing_decrease,
    rear_wing_increase,
    rear_wing_decrease,
    diff_preload_increase,
    diff_preload_decrease,
    diff_entry_increase,
    diff_entry_decrease,
    diff_middle_increase,
    diff_middle_decrease,
    diff_exit_increase,
    diff_exit_decrease,
    throttle_shaping_increase,
    throttle_shaping_decrease,
    mgu_k_re_gen_gain_increase,
    mgu_k_re_gen_gain_decrease,
    mgu_k_deploy_mode_increase,
    mgu_k_deploy_mode_decrease,
    mgu_k_fixed_deploy_increase,
    mgu_k_fixed_deploy_decrease,
    low_fuel_accept,
    fcy_mode_toggle,
    next_dash_page,
    previous_dash_page,
    left_indicator,
    right_indicator,
    kers,
    horn,
    reset_vehicle,
    enter_vehicle,
    exit_vehicle,
    launch_control,
    abs_value_0,
    abs_value_1,
    abs_value_2,
    abs_value_3,
    abs_value_4,
    abs_value_5,
    abs_value_6,
    abs_value_7,
    abs_value_8,
    abs_value_9,
    abs_value_10,
    abs_value_11,
    tc_adjust,
    tc_value_0,
    tc_value_1,
    tc_value_2,
    tc_value_3,
    tc_value_4,
    tc_value_5,
    tc_value_6,
    tc_value_7,
    tc_value_8,
    tc_value_9,
    tc_value_10,
    tc_value_11,
    tc2_value_0,
    tc2_value_1,
    tc2_value_2,
    tc2_value_3,
    tc2_value_4,
    tc2_value_5,
    tc2_value_6,
    tc2_value_7,
    tc2_value_8,
    tc2_value_9,
    tc2_value_10,
    tc2_value_11,
    enter,
    back,
    up,
    down,
    left,
    right,
    pause,
    next_overlay_page,
    previous_overlay_page,
    lap_timing_overlay_page,
    standings_overlay_page,
    relative_overlay_page,
    fuel_overlay_page,
    tires_overlay_page,
    tire_info_overlay_page,
    pit_stop_adjustments_overlay_page,
    in_car_adjustments_overlay_page,
    mirror_adjustments_overlay_page,
    radio_adjustments_overlay_page,
    weather_overlay_page,
    select_next_control,
    select_previous_control,
    increment_selected_control,
    decrement_selected_control,
    toggle_selected_control,
    toggle_dash_box,
    prev_splits_delta_display,
    next_splits_delta_display,
    increase_field_of_view,
    decrease_field_of_view,
    shift_horizon_up,
    shift_horizon_down,
    increase_driver_height,
    decrease_driver_height,
    recenter_tilt_axis,
    glance_left,
    glance_right,
    glance_back,
    change_camera,
    move_forward,
    move_backward,
    move_left,
    move_right,
    mark_event_in_telemetry,
    active_reset_save_start_point,
    active_reset_run,
    spotter_silence,
    damage_report,
    ffb_gain_increase,
    ffb_gain_decrease,
    in_game_push_to_talk,
    external_push_to_talk,
    chat_sorry,
    chat_thanks,
    chat_enter_pit,
    chat_exit_pit,
    mute,
    volume_increase,
    volume_decrease,

    /** Used for inputs that are mapped to some type of input, but it is unknown */
    unknown,

    /** Used for inputs that are missing dynamic mapping to a control */
    unmapped,

    /** Used when the input role is some other than what is supported by this API version */
    other,
};
inline constexpr const char* input_role_ids[] = {
    "steering",
    "throttle",
    "brake",
    "clutch",
    "clutch_secondary",
    "clutch_primary",
    "gear_shift",
    "gear_shift_up",
    "gear_shift_down",
    "handbrake",
    "ignition",
    "starter",
    "headlight_toggle",
    "high_beam_toggle",
    "headlight_flash",
    "pit_limiter",
    "overtake",
    "drs",
    "traction_control_toggle",
    "traction_control_increase",
    "traction_control_decrease",
    "tear_off_visor",
    "trigger_windshield_wipers",
    "toggle_windshield_wipers",
    "brake_bias_increase",
    "brake_bias_decrease",
    "front_antiroll_bar_increase",
    "front_antiroll_bar_decrease",
    "rear_antiroll_bar_increase",
    "rear_antiroll_bar_decrease",
    "fuel_map_increase",
    "fuel_map_decrease",
    "turbo_pressure_increase",
    "turbo_pressure_decrease",
    "abs_adjust",
    "abs_increase",
    "abs_decrease",
    "front_wing_increase",
    "front_wing_decrease",
    "rear_wing_increase",
    "rear_wing_decrease",
    "diff_preload_increase",
    "diff_preload_decrease",
    "diff_entry_increase",
    "diff_entry_decrease",
    "diff_middle_increase",
    "diff_middle_decrease",
    "diff_exit_increase",
    "diff_exit_decrease",
    "throttle_shaping_increase",
    "throttle_shaping_decrease",
    "mgu_k_re_gen_gain_increase",
    "mgu_k_re_gen_gain_decrease",
    "mgu_k_deploy_mode_increase",
    "mgu_k_deploy_mode_decrease",
    "mgu_k_fixed_deploy_increase",
    "mgu_k_fixed_deploy_decrease",
    "low_fuel_accept",
    "fcy_mode_toggle",
    "next_dash_page",
    "previous_dash_page",
    "left_indicator",
    "right_indicator",
    "kers",
    "horn",
    "reset_vehicle",
    "enter_vehicle",
    "exit_vehicle",
    "launch_control",
    "abs_value_0",
    "abs_value_1",
    "abs_value_2",
    "abs_value_3",
    "abs_value_4",
    "abs_value_5",
    "abs_value_6",
    "abs_value_7",
    "abs_value_8",
    "abs_value_9",
    "abs_value_10",
    "abs_value_11",
    "tc_adjust",
    "tc_value_0",
    "tc_value_1",
    "tc_value_2",
    "tc_value_3",
    "tc_value_4",
    "tc_value_5",
    "tc_value_6",
    "tc_value_7",
    "tc_value_8",
    "tc_value_9",
    "tc_value_10",
    "tc_value_11",
    "tc2_value_0",
    "tc2_value_1",
    "tc2_value_2",
    "tc2_value_3",
    "tc2_value_4",
    "tc2_value_5",
    "tc2_value_6",
    "tc2_value_7",
    "tc2_value_8",
    "tc2_value_9",
    "tc2_value_10",
    "tc2_value_11",
    "enter",
    "back",
    "up",
    "down",
    "left",
    "right",
    "pause",
    "next_overlay_page",
    "previous_overlay_page",
    "lap_timing_overlay_page",
    "standings_overlay_page",
    "relative_overlay_page",
    "fuel_overlay_page",
    "tires_overlay_page",
    "tire_info_overlay_page",
    "pit_stop_adjustments_overlay_page",
    "in_car_adjustments_overlay_page",
    "mirror_adjustments_overlay_page",
    "radio_adjustments_overlay_page",
    "weather_overlay_page",
    "select_next_control",
    "select_previous_control",
    "increment_selected_control",
    "decrement_selected_control",
    "toggle_selected_control",
    "toggle_dash_box",
    "prev_splits_delta_display",
    "next_splits_delta_display",
    "increase_field_of_view",
    "decrease_field_of_view",
    "shift_horizon_up",
    "shift_horizon_down",
    "increase_driver_height",
    "decrease_driver_height",
    "recenter_tilt_axis",
    "glance_left",
    "glance_right",
    "glance_back",
    "change_camera",
    "move_forward",
    "move_backward",
    "move_left",
    "move_right",
    "mark_event_in_telemetry",
    "active_reset_save_start_point",
    "active_reset_run",
    "spotter_silence",
    "damage_report",
    "ffb_gain_increase",
    "ffb_gain_decrease",
    "in_game_push_to_talk",
    "external_push_to_talk",
    "chat_sorry",
    "chat_thanks",
    "chat_enter_pit",
    "chat_exit_pit",
    "mute",
    "volume_increase",
    "volume_decrease",
    "unknown",
    "unmapped",
    "other"
,};

constexpr std::string_view toString(InputRole r) { return input_role_ids[(int)r]; }
constexpr InputRole inputRoleFromString(std::string_view s) {
    for (int i = 0; i < (int)InputRole::other; ++i) {
        if (s == input_role_ids[i]) return (InputRole)i;
    }
    return InputRole::other;
}



/** Input types define how the input is represented in this API
 *
 * This should be used determine how the input values should be read from this API.
 *
 * This enum and its numeric ids will change between SC-API versions.
 * Use textual representation to refer these instead of enumerator numeric values.
 */
enum class InputType {

    /** Analog axis */
    axis,

    /** Digital input that is briefly activated when user interracts with it */
    button,

    /** Incremental rotary encoder */
    inc_rot_enc,

    /** Absolute rotary encoder */
    abs_rot_enc,

    /** Used as placeholder for unavailable data. */
    unknown,

    /** Used when the input type is some other than what is supported by this API version */
    other,
};
inline constexpr const char* input_type_ids[] = {
    "axis",
    "button",
    "inc_rot_enc",
    "abs_rot_enc",
    "unknown",
    "other"
,};

constexpr std::string_view toString(InputType r) { return input_type_ids[(int)r]; }
constexpr InputType inputTypeFromString(std::string_view s) {
    for (int i = 0; i < (int)InputType::other; ++i) {
        if (s == input_type_ids[i]) return (InputType)i;
    }
    return InputType::other;
}



}  // namespace device_info
}  // namespace sc_api::core

#endif  // SC_API_DEVICE_INFO_DEFINITIONS_H_
