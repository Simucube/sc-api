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


class TelemetryUpdateObject:
    """Attribute-style wrapper around TelemetryUpdateGroup.

    Usage::

        group = TelemetryUpdateObject(telemetry_defs)
        group.engine_rpm = 1000.0
        group.physics_running = True
        group.send()

        # Read back a value
        print(group.engine_rpm)

        # Remove a telemetry entry
        group.engine_rpm = None
    """

    _RESERVED = frozenset({"send", "available_names", "group_id"})

    def __init__(self, telemetry_definitions):
        """Create a new telemetry update object.

        Args:
            telemetry_definitions: A TelemetryDefinitions collection from
                the active session.
        """
        object.__setattr__(self, "_group", TelemetryUpdateGroup(telemetry_definitions))

    def __setattr__(self, name, value):
        """Set a telemetry value by attribute name.

        Assign ``None`` to remove the telemetry entry from the update group.

        Raises:
            AttributeError: If *name* is not a valid telemetry channel.
            TypeError: If *value* does not match the channel's expected type.
        """
        if name.startswith("_"):
            object.__setattr__(self, name, value)
            return
        group = object.__getattribute__(self, "_group")
        try:
            if value is None:
                if name in group:
                    del group[name]
            else:
                group[name] = value
        except KeyError:
            raise AttributeError(name, name=name, obj=self) from None

    def __getattr__(self, name):
        """Get the current value of a telemetry channel.

        Raises:
            AttributeError: If *name* has not been set on this object.
        """
        group = object.__getattribute__(self, "_group")
        if name in self._RESERVED:
            return getattr(group, name)
        try:
            return group[name]
        except KeyError:
            raise AttributeError(name, name=name, obj=self) from None

    def __delattr__(self, name):
        """Remove a telemetry entry from the update group.

        Raises:
            AttributeError: If *name* has not been set on this object.
        """
        group = object.__getattribute__(self, "_group")
        try:
            del group[name]
        except KeyError:
            raise AttributeError(name, name=name, obj=self) from None

    def __repr__(self):
        group = object.__getattribute__(self, "_group")
        return f"<TelemetryUpdateObject {group!r}>"


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
    "TelemetryUpdateObject",
    "duration_ns_from_hz",
]


def duration_ns_from_hz(hz: int) -> int:
    """Convert frequency in Hz to duration in nanoseconds."""
    return 1_000_000_000 // hz
