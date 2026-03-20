#include <nanobind/nanobind.h>

#include <sc-api/core/action.h>
#include <sc-api/core/device_info_definitions.h>
#include <sc-api/core/ffb.h>
#include <sc-api/core/result.h>
#include <sc-api/core/session.h>
#include <sc-api/core/session_fwd.h>
#include <sc-api/core/type.h>

namespace nb = nanobind;

using sc_api::ResultCode;
using sc_api::core::ActionResult;
using sc_api::core::FilterType;
using sc_api::core::InterpolationType;
using sc_api::core::OffsetType;
using sc_api::core::Session;
using sc_api::core::SessionState;
using sc_api::core::Type;
namespace device_info = sc_api::core::device_info;

void bind_enums(nb::module_& m) {
    nb::enum_<SessionState>(m, "SessionState",
                            "Connection lifecycle state of a session with Simucube Tuner. "
                            "Returned by session queries and delivered via SessionStateChanged events.")
        .value("invalid", SessionState::invalid)
        .value("connected_monitor", SessionState::connected_monitor)
        .value("connected_control", SessionState::connected_control)
        .value("session_lost", SessionState::session_lost);

    nb::enum_<Session::ControlFlag>(m, "ControlFlag",
                                    "Flags selecting which device capabilities to request when entering control mode. "
                                    "Values can be combined with the | operator.",
                                    nb::is_flag())
        .value("control_ffb_effects", Session::control_ffb_effects)
        .value("control_telemetry", Session::control_telemetry)
        .value("control_sim_data", Session::control_sim_data);

    nb::enum_<ResultCode>(m, "ResultCode",
                          "Low-level operation result codes. "
                          "Most error values are automatically raised as Python exceptions; direct use is rarely needed.")
        .value("ok", ResultCode::ok)
        .value("error_invalid_argument", ResultCode::error_invalid_argument)
        .value("error_invalid_format", ResultCode::error_invalid_format)
        .value("error_not_supported", ResultCode::error_not_supported)
        .value("error_invalid_state", ResultCode::error_invalid_state)
        .value("error_not_registered", ResultCode::error_not_registered)
        .value("error_no_control", ResultCode::error_no_control)
        .value("error_incompatible", ResultCode::error_incompatible)
        .value("error_internal_comm_error", ResultCode::error_internal_comm_error)
        .value("error_internal", ResultCode::error_internal)
        .value("command_error_mask", ResultCode::command_error_mask)
        .value("error_invalid_session_state", ResultCode::error_invalid_session_state)
        .value("error_busy", ResultCode::error_busy)
        .value("error_timeout", ResultCode::error_timeout)
        .value("error_cannot_connect", ResultCode::error_cannot_connect)
        .value("error_protocol", ResultCode::error_protocol);

    nb::enum_<ActionResult>(m, "ActionResult",
                            "Progress state of an asynchronous operation. "
                            "Poll an action's result to determine whether it is still running, succeeded, or failed.")
        .value("inprogress", ActionResult::inprogress)
        .value("complete", ActionResult::complete)
        .value("failed", ActionResult::failed)
        .value("would_block", ActionResult::would_block);

    nb::enum_<Type::BaseType>(m, "BaseType",
                              "Scalar data type of a variable or telemetry channel. "
                              "Used to interpret raw values returned by VariableDefinitions and TelemetryDefinitions.")
        .value("invalid", Type::invalid)
        .value("boolean", Type::boolean)
        .value("i8", Type::i8)
        .value("u8", Type::u8)
        .value("i16", Type::i16)
        .value("u16", Type::u16)
        .value("i32", Type::i32)
        .value("u32", Type::u32)
        .value("i64", Type::i64)
        .value("f32", Type::f32)
        .value("f64", Type::f64)
        .value("cstring", Type::cstring);

    nb::enum_<device_info::DeviceRole>(m, "DeviceRole",
                                       "The functional role of a Simucube device in the hardware setup, "
                                       "such as wheelbase, pedal, or button box.")
        .value("wheel", device_info::DeviceRole::wheel)
        .value("wheelbase", device_info::DeviceRole::wheelbase)
        .value("throttle_pedal", device_info::DeviceRole::throttle_pedal)
        .value("brake_pedal", device_info::DeviceRole::brake_pedal)
        .value("handbrake", device_info::DeviceRole::handbrake)
        .value("clutch_pedal", device_info::DeviceRole::clutch_pedal)
        .value("gear_stick", device_info::DeviceRole::gear_stick)
        .value("button_box", device_info::DeviceRole::button_box)
        .value("hub", device_info::DeviceRole::hub)
        .value("unknown", device_info::DeviceRole::unknown)
        .value("other", device_info::DeviceRole::other);

    nb::enum_<device_info::ControlType>(m, "ControlType",
                                        "Physical control type of a component on a device, "
                                        "such as a pedal, button, hat switch, or rotary encoder.")
        .value("wheelbase", device_info::ControlType::wheelbase)
        .value("wheel", device_info::ControlType::wheel)
        .value("pedal", device_info::ControlType::pedal)
        .value("paddle", device_info::ControlType::paddle)
        .value("hat_switch", device_info::ControlType::hat_switch)
        .value("button", device_info::ControlType::button)
        .value("toggle_switch", device_info::ControlType::toggle_switch)
        .value("dir_2way", device_info::ControlType::dir_2way)
        .value("dir_4way", device_info::ControlType::dir_4way)
        .value("rot_enc", device_info::ControlType::rot_enc)
        .value("funky_switch", device_info::ControlType::funky_switch)
        .value("light", device_info::ControlType::light)
        .value("unknown", device_info::ControlType::unknown)
        .value("other", device_info::ControlType::other);

    nb::enum_<device_info::FeedbackType>(m, "FeedbackType",
                                         "Output/feedback mechanism type of a device component, "
                                         "such as a direct-input wheelbase, active pedal, or RGB light.")
        .value("direct_input", device_info::FeedbackType::direct_input)
        .value("wheelbase", device_info::FeedbackType::wheelbase)
        .value("active_pedal", device_info::FeedbackType::active_pedal)
        .value("rgb_light", device_info::FeedbackType::rgb_light)
        .value("light", device_info::FeedbackType::light)
        .value("unknown", device_info::FeedbackType::unknown)
        .value("other", device_info::FeedbackType::other);

    nb::enum_<device_info::InputType>(m, "InputType",
                                      "Physical input mechanism type of a device component, "
                                      "such as an analog axis, button, or rotary encoder.")
        .value("axis", device_info::InputType::axis)
        .value("button", device_info::InputType::button)
        .value("inc_rot_enc", device_info::InputType::inc_rot_enc)
        .value("abs_rot_enc", device_info::InputType::abs_rot_enc)
        .value("unknown", device_info::InputType::unknown)
        .value("other", device_info::InputType::other);

    nb::enum_<device_info::InputRole>(m, "InputRole",
                                      "Semantic role of a device input, identifying its in-game function "
                                      "such as steering, throttle, brake, or a specific car control action.")
        .value("steering", device_info::InputRole::steering)
        .value("throttle", device_info::InputRole::throttle)
        .value("brake", device_info::InputRole::brake)
        .value("clutch", device_info::InputRole::clutch)
        .value("clutch_secondary", device_info::InputRole::clutch_secondary)
        .value("clutch_primary", device_info::InputRole::clutch_primary)
        .value("gear_shift", device_info::InputRole::gear_shift)
        .value("gear_shift_up", device_info::InputRole::gear_shift_up)
        .value("gear_shift_down", device_info::InputRole::gear_shift_down)
        .value("handbrake", device_info::InputRole::handbrake)
        .value("ignition", device_info::InputRole::ignition)
        .value("starter", device_info::InputRole::starter)
        .value("headlight_toggle", device_info::InputRole::headlight_toggle)
        .value("high_beam_toggle", device_info::InputRole::high_beam_toggle)
        .value("headlight_flash", device_info::InputRole::headlight_flash)
        .value("pit_limiter", device_info::InputRole::pit_limiter)
        .value("overtake", device_info::InputRole::overtake)
        .value("drs", device_info::InputRole::drs)
        .value("traction_control_toggle", device_info::InputRole::traction_control_toggle)
        .value("traction_control_increase", device_info::InputRole::traction_control_increase)
        .value("traction_control_decrease", device_info::InputRole::traction_control_decrease)
        .value("tear_off_visor", device_info::InputRole::tear_off_visor)
        .value("trigger_windshield_wipers", device_info::InputRole::trigger_windshield_wipers)
        .value("toggle_windshield_wipers", device_info::InputRole::toggle_windshield_wipers)
        .value("brake_bias_increase", device_info::InputRole::brake_bias_increase)
        .value("brake_bias_decrease", device_info::InputRole::brake_bias_decrease)
        .value("front_antiroll_bar_increase", device_info::InputRole::front_antiroll_bar_increase)
        .value("front_antiroll_bar_decrease", device_info::InputRole::front_antiroll_bar_decrease)
        .value("rear_antiroll_bar_increase", device_info::InputRole::rear_antiroll_bar_increase)
        .value("rear_antiroll_bar_decrease", device_info::InputRole::rear_antiroll_bar_decrease)
        .value("fuel_map_increase", device_info::InputRole::fuel_map_increase)
        .value("fuel_map_decrease", device_info::InputRole::fuel_map_decrease)
        .value("turbo_pressure_increase", device_info::InputRole::turbo_pressure_increase)
        .value("turbo_pressure_decrease", device_info::InputRole::turbo_pressure_decrease)
        .value("abs_adjust", device_info::InputRole::abs_adjust)
        .value("abs_increase", device_info::InputRole::abs_increase)
        .value("abs_decrease", device_info::InputRole::abs_decrease)
        .value("front_wing_increase", device_info::InputRole::front_wing_increase)
        .value("front_wing_decrease", device_info::InputRole::front_wing_decrease)
        .value("rear_wing_increase", device_info::InputRole::rear_wing_increase)
        .value("rear_wing_decrease", device_info::InputRole::rear_wing_decrease)
        .value("diff_preload_increase", device_info::InputRole::diff_preload_increase)
        .value("diff_preload_decrease", device_info::InputRole::diff_preload_decrease)
        .value("diff_entry_increase", device_info::InputRole::diff_entry_increase)
        .value("diff_entry_decrease", device_info::InputRole::diff_entry_decrease)
        .value("diff_middle_increase", device_info::InputRole::diff_middle_increase)
        .value("diff_middle_decrease", device_info::InputRole::diff_middle_decrease)
        .value("diff_exit_increase", device_info::InputRole::diff_exit_increase)
        .value("diff_exit_decrease", device_info::InputRole::diff_exit_decrease)
        .value("throttle_shaping_increase", device_info::InputRole::throttle_shaping_increase)
        .value("throttle_shaping_decrease", device_info::InputRole::throttle_shaping_decrease)
        .value("mgu_k_re_gen_gain_increase", device_info::InputRole::mgu_k_re_gen_gain_increase)
        .value("mgu_k_re_gen_gain_decrease", device_info::InputRole::mgu_k_re_gen_gain_decrease)
        .value("mgu_k_deploy_mode_increase", device_info::InputRole::mgu_k_deploy_mode_increase)
        .value("mgu_k_deploy_mode_decrease", device_info::InputRole::mgu_k_deploy_mode_decrease)
        .value("mgu_k_fixed_deploy_increase", device_info::InputRole::mgu_k_fixed_deploy_increase)
        .value("mgu_k_fixed_deploy_decrease", device_info::InputRole::mgu_k_fixed_deploy_decrease)
        .value("low_fuel_accept", device_info::InputRole::low_fuel_accept)
        .value("fcy_mode_toggle", device_info::InputRole::fcy_mode_toggle)
        .value("next_dash_page", device_info::InputRole::next_dash_page)
        .value("previous_dash_page", device_info::InputRole::previous_dash_page)
        .value("left_indicator", device_info::InputRole::left_indicator)
        .value("right_indicator", device_info::InputRole::right_indicator)
        .value("kers", device_info::InputRole::kers)
        .value("horn", device_info::InputRole::horn)
        .value("reset_vehicle", device_info::InputRole::reset_vehicle)
        .value("enter_vehicle", device_info::InputRole::enter_vehicle)
        .value("exit_vehicle", device_info::InputRole::exit_vehicle)
        .value("launch_control", device_info::InputRole::launch_control)
        .value("abs_value_0", device_info::InputRole::abs_value_0)
        .value("abs_value_1", device_info::InputRole::abs_value_1)
        .value("abs_value_2", device_info::InputRole::abs_value_2)
        .value("abs_value_3", device_info::InputRole::abs_value_3)
        .value("abs_value_4", device_info::InputRole::abs_value_4)
        .value("abs_value_5", device_info::InputRole::abs_value_5)
        .value("abs_value_6", device_info::InputRole::abs_value_6)
        .value("abs_value_7", device_info::InputRole::abs_value_7)
        .value("abs_value_8", device_info::InputRole::abs_value_8)
        .value("abs_value_9", device_info::InputRole::abs_value_9)
        .value("abs_value_10", device_info::InputRole::abs_value_10)
        .value("abs_value_11", device_info::InputRole::abs_value_11)
        .value("tc_adjust", device_info::InputRole::tc_adjust)
        .value("tc_value_0", device_info::InputRole::tc_value_0)
        .value("tc_value_1", device_info::InputRole::tc_value_1)
        .value("tc_value_2", device_info::InputRole::tc_value_2)
        .value("tc_value_3", device_info::InputRole::tc_value_3)
        .value("tc_value_4", device_info::InputRole::tc_value_4)
        .value("tc_value_5", device_info::InputRole::tc_value_5)
        .value("tc_value_6", device_info::InputRole::tc_value_6)
        .value("tc_value_7", device_info::InputRole::tc_value_7)
        .value("tc_value_8", device_info::InputRole::tc_value_8)
        .value("tc_value_9", device_info::InputRole::tc_value_9)
        .value("tc_value_10", device_info::InputRole::tc_value_10)
        .value("tc_value_11", device_info::InputRole::tc_value_11)
        .value("tc2_value_0", device_info::InputRole::tc2_value_0)
        .value("tc2_value_1", device_info::InputRole::tc2_value_1)
        .value("tc2_value_2", device_info::InputRole::tc2_value_2)
        .value("tc2_value_3", device_info::InputRole::tc2_value_3)
        .value("tc2_value_4", device_info::InputRole::tc2_value_4)
        .value("tc2_value_5", device_info::InputRole::tc2_value_5)
        .value("tc2_value_6", device_info::InputRole::tc2_value_6)
        .value("tc2_value_7", device_info::InputRole::tc2_value_7)
        .value("tc2_value_8", device_info::InputRole::tc2_value_8)
        .value("tc2_value_9", device_info::InputRole::tc2_value_9)
        .value("tc2_value_10", device_info::InputRole::tc2_value_10)
        .value("tc2_value_11", device_info::InputRole::tc2_value_11)
        .value("enter", device_info::InputRole::enter)
        .value("back", device_info::InputRole::back)
        .value("up", device_info::InputRole::up)
        .value("down", device_info::InputRole::down)
        .value("left", device_info::InputRole::left)
        .value("right", device_info::InputRole::right)
        .value("pause", device_info::InputRole::pause)
        .value("next_overlay_page", device_info::InputRole::next_overlay_page)
        .value("previous_overlay_page", device_info::InputRole::previous_overlay_page)
        .value("lap_timing_overlay_page", device_info::InputRole::lap_timing_overlay_page)
        .value("standings_overlay_page", device_info::InputRole::standings_overlay_page)
        .value("relative_overlay_page", device_info::InputRole::relative_overlay_page)
        .value("fuel_overlay_page", device_info::InputRole::fuel_overlay_page)
        .value("tires_overlay_page", device_info::InputRole::tires_overlay_page)
        .value("tire_info_overlay_page", device_info::InputRole::tire_info_overlay_page)
        .value("pit_stop_adjustments_overlay_page", device_info::InputRole::pit_stop_adjustments_overlay_page)
        .value("in_car_adjustments_overlay_page", device_info::InputRole::in_car_adjustments_overlay_page)
        .value("mirror_adjustments_overlay_page", device_info::InputRole::mirror_adjustments_overlay_page)
        .value("radio_adjustments_overlay_page", device_info::InputRole::radio_adjustments_overlay_page)
        .value("weather_overlay_page", device_info::InputRole::weather_overlay_page)
        .value("select_next_control", device_info::InputRole::select_next_control)
        .value("select_previous_control", device_info::InputRole::select_previous_control)
        .value("increment_selected_control", device_info::InputRole::increment_selected_control)
        .value("decrement_selected_control", device_info::InputRole::decrement_selected_control)
        .value("toggle_selected_control", device_info::InputRole::toggle_selected_control)
        .value("toggle_dash_box", device_info::InputRole::toggle_dash_box)
        .value("prev_splits_delta_display", device_info::InputRole::prev_splits_delta_display)
        .value("next_splits_delta_display", device_info::InputRole::next_splits_delta_display)
        .value("increase_field_of_view", device_info::InputRole::increase_field_of_view)
        .value("decrease_field_of_view", device_info::InputRole::decrease_field_of_view)
        .value("shift_horizon_up", device_info::InputRole::shift_horizon_up)
        .value("shift_horizon_down", device_info::InputRole::shift_horizon_down)
        .value("increase_driver_height", device_info::InputRole::increase_driver_height)
        .value("decrease_driver_height", device_info::InputRole::decrease_driver_height)
        .value("recenter_tilt_axis", device_info::InputRole::recenter_tilt_axis)
        .value("glance_left", device_info::InputRole::glance_left)
        .value("glance_right", device_info::InputRole::glance_right)
        .value("glance_back", device_info::InputRole::glance_back)
        .value("change_camera", device_info::InputRole::change_camera)
        .value("move_forward", device_info::InputRole::move_forward)
        .value("move_backward", device_info::InputRole::move_backward)
        .value("move_left", device_info::InputRole::move_left)
        .value("move_right", device_info::InputRole::move_right)
        .value("mark_event_in_telemetry", device_info::InputRole::mark_event_in_telemetry)
        .value("active_reset_save_start_point", device_info::InputRole::active_reset_save_start_point)
        .value("active_reset_run", device_info::InputRole::active_reset_run)
        .value("spotter_silence", device_info::InputRole::spotter_silence)
        .value("damage_report", device_info::InputRole::damage_report)
        .value("ffb_gain_increase", device_info::InputRole::ffb_gain_increase)
        .value("ffb_gain_decrease", device_info::InputRole::ffb_gain_decrease)
        .value("in_game_push_to_talk", device_info::InputRole::in_game_push_to_talk)
        .value("external_push_to_talk", device_info::InputRole::external_push_to_talk)
        .value("chat_sorry", device_info::InputRole::chat_sorry)
        .value("chat_thanks", device_info::InputRole::chat_thanks)
        .value("chat_enter_pit", device_info::InputRole::chat_enter_pit)
        .value("chat_exit_pit", device_info::InputRole::chat_exit_pit)
        .value("mute", device_info::InputRole::mute)
        .value("volume_increase", device_info::InputRole::volume_increase)
        .value("volume_decrease", device_info::InputRole::volume_decrease)
        .value("unknown", device_info::InputRole::unknown)
        .value("unmapped", device_info::InputRole::unmapped)
        .value("other", device_info::InputRole::other);

    nb::enum_<OffsetType>(m, "OffsetType",
                          "Units for an FFB effect offset value. "
                          "Torque-based offsets apply to wheelbases; force- and position-based offsets apply to active pedals.")
        .value("torque_Nm", OffsetType::torque_Nm)
        .value("torque_relative", OffsetType::torque_relative)
        .value("force_N", OffsetType::force_N)
        .value("force_relative", OffsetType::force_relative)
        .value("position_mm", OffsetType::position_mm);

    nb::enum_<InterpolationType>(m, "InterpolationType",
                                 "Interpolation method applied between FFB effect data point samples.")
        .value("none", InterpolationType::none)
        .value("linear", InterpolationType::linear);

    nb::enum_<FilterType>(m, "FilterType",
                          "Filter applied to the FFB output signal, such as a low-pass filter or slew rate limiter.")
        .value("none", FilterType::none)
        .value("low_pass", FilterType::low_pass)
        .value("slew_rate_limit", FilterType::slew_rate_limit);
}
