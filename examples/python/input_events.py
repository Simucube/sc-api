"""Print button and touch actions, with recovery after lost events or device removal.

From the repository root, run: python examples/python/input_events.py
"""

import signal
import time
from dataclasses import dataclass, field

import simucube_api

WORD_COUNT = 4  # The stream covers 128 buttons.

RELEASED_TOUCH = simucube_api.TouchEventPayload()  # A panel without a previous event starts released.

running = True


def on_signal(_signum, _frame):
    global running
    running = False


@dataclass
class DeviceState:
    """One device that reports input events."""

    name: str
    inputs: dict[int, str] = field(default_factory=dict)  # input name of each event id that this device uses
    # The digital input words of the device in shared memory. A word can be absent.
    baseline_words: list[simucube_api.VariableDefinition | None] = field(default_factory=lambda: [None] * WORD_COUNT)
    armed: set[int] = field(default_factory=set)  # event ids whose press action this program started
    last_touch: dict[int, simucube_api.TouchEventPayload] = field(default_factory=dict)  # by panel (input_id)


def hid_button_text(event):
    return "no HID button" if event.hid_index is None else f"HID button {event.hid_index}"


def print_action(device_session_id, device, event_id, action):
    print(f"{device.name} ({device_session_id}): {device.inputs[event_id]} {action}")


def has_baseline(device):
    """The words can arrive after the device info; until then the baseline reads 0."""
    return device.baseline_words[0] is not None


def read_baseline(device):
    baseline = 0
    for i, word in enumerate(device.baseline_words):
        if word is not None:
            baseline |= word.value << (32 * i)
    return baseline


def resolve_baseline_words(variables, input_variable, device_session_id):
    """A wireless wheel has its own device session id, but the SC-link Hub holds its digital input words."""
    is_wireless_wheel_input = input_variable.device_session_id.id != device_session_id
    prefix = "ww." if is_wireless_wheel_input else ""
    return [
        variables.find(f"{prefix}digital_inputs{i}", device=input_variable.device_session_id) for i in range(WORD_COUNT)
    ]


def release_armed_buttons(device_session_id, device, reason):
    for event_id in sorted(device.armed):
        print_action(device_session_id, device, event_id, reason)
    device.armed.clear()


def print_touch(device_session_id, device, panel, point, state):
    print(f"{device.name} ({device_session_id}): panel {panel} point {point} {state}")


def position_text(touch, point):
    return f"{touch.x[point]},{touch.y[point]}"


def is_point_down(touch, point):
    return bool((touch.points_down >> point) & 1)


def is_point_lifted(touch, point):
    return bool((touch.points_up >> point) & 1)


def is_point_tapped(touch, point):
    return bool((touch.points_up >> (point + simucube_api.TOUCH_EVENT_TAP_SHIFT)) & 1)


def print_cancelled_points(device_session_id, device, panel, touch, reason):
    for point in range(simucube_api.TOUCH_EVENT_MAX_POINTS):
        if is_point_down(touch, point):
            print_touch(device_session_id, device, panel, point, reason)


def cancel_touch(device_session_id, device, reason):
    for panel, touch in device.last_touch.items():
        print_cancelled_points(device_session_id, device, panel, touch, reason)
    device.last_touch.clear()


def end_all_actions(device_session_id, device, cause):
    release_armed_buttons(device_session_id, device, f"released ({cause})")
    cancel_touch(device_session_id, device, f"cancelled ({cause})")


def apply_baseline(device_session_id, device):
    """A baseline never starts a press action, because a later event can repeat a transition that the baseline holds."""
    baseline = read_baseline(device)
    for event_id in sorted(device.armed):
        if not (baseline >> event_id) & 1:
            device.armed.remove(event_id)
            print_action(device_session_id, device, event_id, "released (missed)")


def make_device_state(info_entry, variables):
    device = DeviceState(name=info_entry.uid)
    for device_input in info_entry.inputs:
        if device_input.event_id is None:
            continue
        if not device.inputs:
            device.baseline_words = resolve_baseline_words(variables, device_input.variable, info_entry.session_id.id)
        device.inputs[device_input.event_id] = device_input.id
    return device


def release_removed_inputs(device_session_id, previous, current):
    """Releases an armed button whose input left or gave its event id to another input, with the old name."""
    for event_id in sorted(current.armed):
        if current.inputs.get(event_id) != previous.inputs[event_id]:
            current.armed.remove(event_id)
            print_action(device_session_id, previous, event_id, "released (input removed)")


def refresh_devices(session, devices):
    """Rebuilds the table; a device that left ends its actions, a device that stays keeps them."""
    info = session.device_info
    if info is None:
        return devices
    # An older snapshot holds no variables of a device that arrived after it, so each call reads it again.
    variables = session.variables

    # Every listed device stays in the table: a device without button inputs can still report touch.
    next_devices = {}
    for info_entry in info:
        device_session_id = info_entry.session_id.id
        device = make_device_state(info_entry, variables)

        previous = devices.get(device_session_id)
        if previous is not None:
            device.armed = previous.armed
            device.last_touch = previous.last_touch
            release_removed_inputs(device_session_id, previous, device)
            if not has_baseline(previous) and has_baseline(device):
                apply_baseline(device_session_id, device)  # arrived before its variables
        next_devices[device_session_id] = device

    for device_session_id, device in devices.items():
        if device_session_id not in next_devices:
            end_all_actions(device_session_id, device, "device gone")
    return next_devices


def resolves(devices, event):
    device = devices.get(event.device_session_id.id)
    if device is None:
        return False
    # A touch event names the device and a panel, not an input.
    return event.type == simucube_api.InputEventType.touch or event.input_id in device.inputs


def apply_touch(device_session_id, device, panel, touch):
    previous = device.last_touch.get(panel, RELEASED_TOUCH)
    if touch.flags & simucube_api.TOUCH_EVENT_CANCELLED:
        print_cancelled_points(device_session_id, device, panel, previous, "cancelled (device gone)")
        device.last_touch.pop(panel, None)
        return
    suppressed = bool(touch.flags & simucube_api.TOUCH_EVENT_MOUSE_SUPPRESSED)
    for point in range(simucube_api.TOUCH_EVENT_MAX_POINTS):
        was_down = is_point_down(previous, point)
        is_down = is_point_down(touch, point)
        moved = touch.x[point] != previous.x[point] or touch.y[point] != previous.y[point]
        if is_down and not was_down:
            kept = ", kept from the mouse" if suppressed else ""
            print_touch(device_session_id, device, panel, point, f"down at {position_text(touch, point)}{kept}")
        elif is_down and was_down and moved:
            print_touch(device_session_id, device, panel, point, f"moved to {position_text(touch, point)}")
        elif is_point_tapped(touch, point):
            print_touch(device_session_id, device, panel, point, f"tapped at {position_text(touch, point)}")
        elif is_point_lifted(touch, point) and was_down:
            print_touch(device_session_id, device, panel, point, f"up at {position_text(touch, point)}")
    device.last_touch[panel] = touch


def apply_event(devices, event):
    device_session_id = event.device_session_id.id
    device = devices.get(device_session_id)
    if device is None:
        return

    # The meaning of input_id depends on the type, so the type decides the path first.
    if (touch := event.touch) is not None:
        apply_touch(device_session_id, device, event.input_id, touch)
        if (tap := event.single_finger_tap) is not None:
            print(f"{device.name} ({device_session_id}): panel {event.input_id} single-finger tap at {tap[0]},{tap[1]}")
        return
    if event.type not in (simucube_api.InputEventType.button_pressed, simucube_api.InputEventType.button_released):
        return
    pressed = event.type == simucube_api.InputEventType.button_pressed

    # An event can carry an input_id that no input of this device uses.
    if event.input_id not in device.inputs:
        return

    if pressed:
        # A press while armed means the release was lost; close the first action before the second.
        if event.input_id in device.armed:
            print_action(device_session_id, device, event.input_id, "released (missed)")
        device.armed.add(event.input_id)
        print_action(device_session_id, device, event.input_id, f"pressed, {hid_button_text(event)}")
    elif event.input_id in device.armed:
        device.armed.remove(event.input_id)
        print_action(device_session_id, device, event.input_id, f"released, {hid_button_text(event)}")


def main():
    signal.signal(signal.SIGINT, on_signal)
    signal.signal(signal.SIGTERM, on_signal)

    with simucube_api.Api() as api:
        event_queue = api.create_event_queue()

        reader = None
        reader_session = None  # device info must come from the session of the reader
        devices = {}
        device_info_changed = False

        while running:
            while (event := event_queue.try_pop()) is not None:
                if isinstance(event, (simucube_api.DeviceInfoChanged, simucube_api.VariableDefinitionsChanged)):
                    device_info_changed = True

            if reader is not None and not reader.is_valid:
                # Nothing more arrives, and the last batch is already applied, so every action can end.
                reader.close()
                reader = None
                reader_session = None
                for device_session_id, device in devices.items():
                    end_all_actions(device_session_id, device, "session lost")
                devices = {}
                print("Session lost")

            if reader is None:
                session = api.session
                if session is not None and session.state != simucube_api.SessionState.session_lost:
                    reader = simucube_api.InputEventReader(session)
                    if reader.open():
                        reader_session = session
                        print("Reading input events. Press a wheel button or touch the screen.")
                    else:
                        reader = None
                if reader is None:
                    time.sleep(1)
                    continue
                # The reader delivers no history, so the table starts from the current device info.
                device_info_changed = True

            if device_info_changed:
                device_info_changed = False
                devices = refresh_devices(reader_session, devices)

            result = reader.read()

            # A panel has no baseline, so a loss cancels its touch.
            if result.lost != 0:
                print(f"Lost {result.lost} records")
                devices = refresh_devices(reader_session, devices)
                for device_session_id, device in devices.items():
                    apply_baseline(device_session_id, device)
                    cancel_touch(device_session_id, device, "cancelled (missed)")

            # Device info after the read resolves this batch and drops the old name of an input whose event id moved.
            if result.events:
                devices = refresh_devices(reader_session, devices)

            # An unresolved event gets one more refresh, at most once per batch, before it is ignored.
            refreshed = False
            for event in result.events:
                if not refreshed and not resolves(devices, event):
                    refreshed = True
                    devices = refresh_devices(reader_session, devices)
                apply_event(devices, event)

            time.sleep(0.005)

        for device_session_id, device in devices.items():
            end_all_actions(device_session_id, device, "exit")
        if reader is not None:
            reader.close()


if __name__ == "__main__":
    main()
