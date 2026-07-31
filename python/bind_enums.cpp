#include <nanobind/nanobind.h>
#include <sc-api/action.h>
#include <sc-api/device_info_definitions.h>
#include <sc-api/ffb.h>
#include <sc-api/result.h>
#include <sc-api/session.h>
#include <sc-api/session_fwd.h>
#include <sc-api/type.h>

namespace nb = nanobind;

using sc_api::ActionResult;
using sc_api::FilterType;
using sc_api::InterpolationType;
using sc_api::OffsetType;
using sc_api::ResultCode;
using sc_api::Session;
using sc_api::SessionState;
using sc_api::Type;
namespace device_info = sc_api::device_info;

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

    nb::enum_<ResultCode>(
        m, "ResultCode",
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

    // device_info enums generated from device_info_enums.json
#include "device_info_enums_generated.h"

    nb::enum_<OffsetType>(
        m, "OffsetType",
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
