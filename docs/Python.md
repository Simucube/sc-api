# Python Bindings {#Python}

The `simucube_api` package gives the features of the C++ API to Python. This guide shows how to
install it and how to use each part.

Every class and method carries a docstring. Use `help(simucube_api.Api)` to read it in the
interpreter.

## Install

```
pip install .
```

Requirements:

- Windows. There is no support for other operating systems.
- Python 3.11 or newer.
- NumPy. `pip` installs it as a dependency.

The build needs a C++ compiler, CMake and nanobind. `pip` gets nanobind through
scikit-build-core.

@note Effect pipelines need NumPy arrays of `float32`. Dashboard frames take NumPy arrays or
`bytes`. The other parts of the package do not need NumPy.

## Open a session

`Api` is a context manager. It starts the background thread that opens the session and keeps its
state up to date. The `with` block closes the API and releases every resource.

```python
import simucube_api

with simucube_api.Api() as api:
    session = api.wait_for_session(timeout=10.0)
    print(f"state: {session.state}")
```

`wait_for_session` raises `TimeoutError` if no session arrives in time.

To receive control access, give the flags and the user information to the constructor. `Api` then
registers again automatically after a reconnect.

```python
with simucube_api.Api(
    control_flags=(
        simucube_api.ControlFlag.control_telemetry | simucube_api.ControlFlag.control_sim_data
    ),
    id_name="my_simulator",
    user_info=simucube_api.ApiUserInformation(
        display_name="My Simulator", type="game", author="Example Ltd", version_string="1.0"
    ),
) as api:
    session = api.wait_for_session(timeout=10.0)
```

The `id_name` identifies the application to Tuner. It takes a maximum of 16 characters. Keep it the
same between runs, because Tuner reuses the state that belongs to it.

## Receive events

`api.events()` returns an iterator of events. With a timeout, each step waits that many seconds. It
then yields `None` and continues.

Events support structural pattern matching:

```python
for event in api.events(timeout=1.0):
    if event is None:
        continue  # Nothing arrived in the last second.

    match event:
        case simucube_api.SessionStateChanged(state=state, session=session):
            if state == simucube_api.SessionState.connected_control:
                print("Control granted")
        case simucube_api.DeviceInfoChanged(session=session):
            print("Devices changed")
```

`api.create_event_queue()` gives a queue instead. Its `try_pop` returns `None` when the queue is
empty, and `pop` waits. Use a queue when the loop must not block on the events.

These event types are available: `SessionStateChanged`, `DeviceInfoChanged`,
`VariableDefinitionsChanged`, `TelemetryDefinitionsChanged` and `SimDataChanged`.

### Stop an event consumer

A blocking call releases the interpreter lock. A thread that waits in one when Python shuts down
never returns. The thread then keeps its Python objects alive, and nanobind prints
`leaked N instances` at exit.

Close the queue or the iterator, then join the thread. Do this before your program ends.

```python
import threading

with simucube_api.Api() as api:
    events = api.events()

    def consume():
        for event in events:
            print(event)

    thread = threading.Thread(target=consume)
    thread.start()

    # ... do work here ...

    events.close()  # Ends the iteration.
    thread.join()
```

`events.close()` and `queue.close()` release a thread that waits for an event. The iterator is also
a context manager, so a `with` block closes it.

Do not make the consumer a daemon thread. Python does not join daemon threads at exit.

## Read device info

`session.device_info` returns the current snapshot, or `None` when no data has arrived yet.

```python
full_info = session.device_info
if full_info is not None:
    for device in full_info:
        print(f"{device.uid}  {device.session_id}  {device.role}")
        for feedback in device.feedbacks:
            print(f"    {feedback.id} type={feedback.type}")
```

Search by capability, not by device model:

```python
active_pedals = full_info.find_all(
    lambda d: any(
        fb.type == simucube_api.FeedbackType.active_pedal for fb in d.feedbacks
    )
)

device_with_leds = full_info.find_first(
    lambda d: any(fb.type == simucube_api.FeedbackType.rgb_light for fb in d.feedbacks)
)
```

`full_info.get_by_session_id(device.parent_session_id)` resolves the connection hierarchy.

## Read variables

`VariableObject` maps friendly attribute names to variable names. It reads the live value from
shared memory on every attribute access.

```python
pedal = simucube_api.VariableObject(
    session.variables,
    {"force": "ap.force_N", "position": "ap.pedal_face_pos_mm"},
    device_id=device.session_id,
)

print(f"{pedal.force} N, {pedal.position} mm")
```

The constructor raises `KeyError` when a variable is absent. A variable exists only when a device
that supplies it is connected. Therefore catch `KeyError` when the device set can change.

`telemetry_variables` builds a `VariableObject` for every telemetry value. It also gives a
`has_<name>` attribute that tells whether any source supplies that value.

```python
tv = simucube_api.telemetry_variables(session.variables, session.telemetries)
if tv.has_engine_rpm:
    print(tv.engine_rpm)
```

To list every variable, iterate `session.variables`.

## Send telemetry

`TelemetryUpdateGroup` takes the telemetry definitions of the session. Set the values by name, then
send the group.

```python
group = simucube_api.TelemetryUpdateGroup(session.telemetries)

group["engine_rpm"] = 5400.0
group["physics_running"] = True
group.send()
```

Build the group again on every new session, because telemetry definitions are session specific.

## Generate force feedback effects

An effect pipeline needs the `control_ffb_effects` flag. Samples are a NumPy array of `float32`.
Timestamps are nanoseconds from `Clock.now_ns()`.

```python
import numpy as np

config = simucube_api.PipelineConfig(offset_type=simucube_api.OffsetType.force_N)

pipeline = simucube_api.FfbPipeline(session, device.session_id)
pipeline.configure(config)

samples = np.array([2.0, 2.0], dtype=np.float32)

start_ns = simucube_api.Clock.now_ns() + 5_000_000  # 5 ms ahead
pipeline.generate_effect(start_ns, 10_000_000, samples)  # Each sample lasts 10 ms.
```

Call `pipeline.stop()` and `pipeline.remove()` when the effect ends.

`duration_ns_from_hz` converts an update rate into a nanosecond period:

```python
update_ns = simucube_api.duration_ns_from_hz(1000)  # 1 kHz
```

Give the samples a few milliseconds to reach the device. Use two or more samples per call, because
one sample produces a sawtooth shape when the next call arrives late.

## Give sim data

`session.replace_sim_data` takes a plain dictionary. It needs the `control_sim_data` flag.

```python
session.replace_sim_data(
    {
        "sim": {"name": "Example Sim"},
        "vehicles": [
            {
                "id": "example-vehicle",
                "name": "Example Vehicle 5000",
                "engine_idle_rpm": 1000,
                "engine_redline_rpm": 9000,
            }
        ],
    },
    sim_id="example-sim",
)
```

`session.sim_data` reads the data back. It returns `None` when no sim data is available.

## Control LEDs

`LedControl` is a context manager. It needs the `control_ffb_effects` flag. It releases the lights
when the block ends.

```python
indices = sorted(light.index for light in device_with_leds.rgb_lights)

with simucube_api.LedControl(session, device_with_leds.session_id) as led:
    red = [simucube_api.RgbColor(255, 0, 0)] * len(indices)
    led.set_leds(indices, red)
    ...
    led.clear()
```

## Stream dashboard frames

`DashStreamer` sends dashboard frames to a device screen through shared memory. It needs the
`control_telemetry` flag. The pixel format is RGB565. The streamer is a context manager, and the
`with` block stops the stream.

```python
import time

import numpy as np

device_with_screen = full_info.find_first(lambda d: len(d.screens) > 0)

screen = device_with_screen.screens[0]
width, height = screen.width, screen.height
dropped = 0

with simucube_api.DashStreamer(session, device_with_screen.session_id) as dash:
    for step in range(600):
        # One frame of packed RGB565 pixels: a blue bar that moves to the right.
        frame = np.zeros((height, width), dtype=np.uint16)
        frame[:, step % width] = 0x001F

        result = dash.stream_frame(frame)
        if result == simucube_api.FrameResult.dropped:
            # The backend still holds the previous frame. Skip this one and render the next.
            dropped += 1

        time.sleep(1.0 / 60.0)
```

The array must be `uint16`, of shape `(height, width)` and C-contiguous. `stream_frame` reads it
without a copy. An array of `uint8` and shape `(height, width, 2)` also works, because
`cv2.cvtColor(frame_bgr, cv2.COLOR_BGR2BGR565)` gives that form. `stream_frame(width, height,
data)` takes raw `bytes` of `width * height * 2` little-endian RGB565 pixels. Use it with PIL or
with any other source.

The streamer connects when it sends the first frame, and retries with a backoff. Until it connects,
`stream_frame` returns `FrameResult.failed`. Call `open()` only to detect a connection failure
before the first frame.

The first streamer that delivers a frame owns the device. Frames of other senders are dropped until
that owner stops. `dash.is_owner` reports ownership.

`dash.get_stream_feedback()` gives `is_owner`, `device_frame_counter`, `dropped_count` and
`last_ack_time_ns`. The lag is the number of sent frames minus `device_frame_counter`. Use it to
pace the loop. The counters come at telemetry rate, so they are good for pacing but not for a
per-frame sync.

`stop()` makes the device leave streaming immediately. Without it, the device waits out its
inactivity timeout of approximately 3 seconds. This is only a latency optimization, and correctness
never depends on it. The `with` block calls `stop()` at its end.

A `DashStreamer` is not thread-safe. Call `stream_frame` from one thread only.

`examples/python/dash_stream.py` is a complete example that captures a window and streams it.

## Errors

The bindings raise Python exceptions instead of returning result codes.
`SimucubeError` is the base class. `StateError`, `IncompatibleError`, `BusyError`,
`InternalError` and `SimucubeConnectionError` derive from it.

## Differences from the C++ API

- Dashboard frames are given as NumPy arrays or `bytes`, not as raw pointers.
- Values are read through `VariableObject` attributes, not through raw pointers.
- Sim data is given as dictionaries, not through builder classes.
- Timestamps are integer nanoseconds, not `std::chrono` types.

## Where to go next

- The `examples/python/` directory of the source tree — complete programs.
- [GettingStarted.md](GettingStarted.md) — the same topics for C++, with more detail on the
  concepts.
