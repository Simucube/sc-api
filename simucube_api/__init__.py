"""Simucube API - Python bindings for Simucube device control."""

import sys

if sys.platform != "win32":
    raise ImportError(
        "simucube-api only supports Windows. "
        "The underlying Simucube hardware interface requires Windows IPC mechanisms."
    )

from simucube_api._native import (  # noqa: E402
    __version__,
    # Enums
    SessionState,
    ControlFlag,
    ResultCode,
    ActionResult,
    BaseType,
    DeviceRole,
    ControlType,
    FeedbackType,
    InputType,
    InputRole,
    # Value types
    DeviceSessionId,
    ApiUserInformation,
    RgbColor,
    # Core classes
    Api,
    Session,
    EventQueue,
    # Event types
    SessionStateChanged,
    DeviceInfoChanged,
    VariableDefinitionsChanged,
    TelemetryDefinitionsChanged,
    SimDataChanged,
    # Device info types
    DeviceInfo,
    FullInfo,
    Control,
    Input,
    Feedback,
    RgbLightFeedback,
    UsbDeviceInfo,
    VariableRef,
    InputMapping,
    HidAxisInput,
    HidButtonInput,
    # Variable types
    Type,
    VariableDefinition,
    VariableDefinitions,
    # Telemetry types
    TelemetryDefinition,
    TelemetryDefinitions,
    TelemetryUpdateGroup,
    # FFB types
    PipelineConfig,
    FfbPipeline,
    Clock,
    OffsetType,
    InterpolationType,
    FilterType,
    # LED control
    LedControl,
    # SimData types
    SimData,
    Vehicle,
    Track,
    Participant,
    SimSession,
    Tire,
    Sim,
    # Exceptions
    SimucubeError,
    StateError,
    IncompatibleError,
    BusyError,
    InternalError,
    SimucubeConnectionError,
)

__all__ = [
    "__version__",
    # Enums
    "SessionState",
    "ControlFlag",
    "ResultCode",
    "ActionResult",
    "BaseType",
    "DeviceRole",
    "ControlType",
    "FeedbackType",
    "InputType",
    "InputRole",
    # Value types
    "DeviceSessionId",
    "ApiUserInformation",
    "RgbColor",
    # Core classes
    "Api",
    "Session",
    "EventQueue",
    # Event types
    "SessionStateChanged",
    "DeviceInfoChanged",
    "VariableDefinitionsChanged",
    "TelemetryDefinitionsChanged",
    "SimDataChanged",
    # Device info types
    "DeviceInfo",
    "FullInfo",
    "Control",
    "Input",
    "Feedback",
    "RgbLightFeedback",
    "UsbDeviceInfo",
    "VariableRef",
    "InputMapping",
    "HidAxisInput",
    "HidButtonInput",
    # Variable types
    "Type",
    "VariableDefinition",
    "VariableDefinitions",
    # Telemetry types
    "TelemetryDefinition",
    "TelemetryDefinitions",
    "TelemetryUpdateGroup",
    # FFB types
    "PipelineConfig",
    "FfbPipeline",
    "Clock",
    "OffsetType",
    "InterpolationType",
    "FilterType",
    # LED control
    "LedControl",
    # SimData types
    "SimData",
    "Vehicle",
    "Track",
    "Participant",
    "SimSession",
    "Tire",
    "Sim",
    # Exceptions
    "SimucubeError",
    "StateError",
    "IncompatibleError",
    "BusyError",
    "InternalError",
    "SimucubeConnectionError",
    # Pure-Python helpers
    "duration_ns_from_hz",
]


def duration_ns_from_hz(hz: int) -> int:
    """Convert frequency in Hz to duration in nanoseconds."""
    return 1_000_000_000 // hz
