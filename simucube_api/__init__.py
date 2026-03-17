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
    # Exceptions
    "SimucubeError",
    "StateError",
    "IncompatibleError",
    "BusyError",
    "InternalError",
    "SimucubeConnectionError",
]
