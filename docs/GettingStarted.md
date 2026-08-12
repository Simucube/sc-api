# Getting Started {#GettingStarted}

This guide shows how to open a session to Simucube Tuner and how to use each part of the API.

For the feature overview, see [Features](#Features). For build and link instructions, see
[INTEGRATION.md](INTEGRATION.md).

## Before you start

- Windows. There is no support for other operating systems.
- C++17 or newer.
- Simucube Tuner must run on the same PC. Tuner is the backend of this API.
- A Simucube device must be connected for the device parts of this guide.

## Open a session

A session is one connection to Tuner. Almost every other function needs a session.

Two entry points are available. [Api](#sc_api::Api) starts a background thread that opens the
session and keeps its state up to date. [ApiCore](#sc_api::ApiCore) does the same work without a
thread, and then the application must call [Session::poll](#sc_api::Session::poll) or
[Session::runUntilStateChanges](#sc_api::Session::runUntilStateChanges) itself. Use `Api` unless
the application needs that control.

A new session starts in `SessionState::connected_monitor`. This state is read-only. It gives
device info, variables and sim data.

Create an [EventQueue](#sc_api::util::EventQueue) to learn when the state changes.

```cpp
#include <sc-api/api.h>
#include <sc-api/events.h>

#include <chrono>
#include <iostream>

sc_api::Api api;
auto        event_queue = api.createEventQueue();

std::shared_ptr<sc_api::Session> session;

auto timeout = std::chrono::steady_clock::now() + std::chrono::seconds(10);
while (auto event = event_queue->tryPopUntil(timeout)) {
    if (auto* changed = sc_api::event::getIfSessionStateChanged(&event)) {
        if (changed->state == sc_api::SessionState::connected_monitor) {
            session = changed->session;
            break;
        }
    }
}

if (!session) {
    std::cerr << "No session in 10 s. Make sure that Simucube Tuner runs.\n";
    return 1;
}
```

A session never returns from `SessionState::session_lost` to a connected state. If the session is
lost, discard every handle to it. `Api` then opens a new session, and a new
`SessionStateChanged` event carries it.

@note Many things are valid only inside one session. Do not store a device session id in a
configuration file. Store the device UID instead.

## Enable control

Force feedback, telemetry and sim data need control access. Register the API user to get it.

[NoAuthControlEnabler](#sc_api::NoAuthControlEnabler) registers the user without encryption. It
registers again automatically after a reconnect. Keep the object alive for as long as control is
needed.

```cpp
sc_api::ApiUserInformation user_info;
user_info.display_name   = "My Simulator";
user_info.author         = "Example Ltd";
user_info.version_string = "1.0";
user_info.type           = "game";
user_info.path           = "";

// The id name identifies this application to Tuner. Max 16 characters.
// Keep it the same between runs, because Tuner reuses the state that belongs to it.
sc_api::NoAuthControlEnabler control_enabler(
    &api,
    sc_api::Session::control_telemetry | sc_api::Session::control_ffb_effects,
    "my_simulator",
    user_info);
```

Request only the flags that the application uses:

| Flag | What it permits |
| ---- | --------------- |
| `control_ffb_effects` | Effect pipelines and LED control |
| `control_telemetry` | Telemetry update groups |
| `control_sim_data` | Sim data updates |

The session reaches `SessionState::connected_control` when the backend accepts the registration.
Wait for that state before you send commands, telemetry or effects. The `control_flags` field of
the `SessionStateChanged` event tells which flags the backend granted.

@warning [SecureControlEnabler](#sc_api::SecureControlEnabler) is the encrypted variant. It needs
a key pair that Simucube issues. It is not complete yet, so do not use it. Use
`NoAuthControlEnabler` until the secure variant is ready.

## Read device info

Device info lists the connected devices, their controls and their feedbacks. It holds no fast
changing values.

```cpp
#include <sc-api/device_info.h>

std::shared_ptr<sc_api::device_info::FullInfo> device_info = session->getDeviceInfo();

std::cout << device_info->getDeviceCount() << " devices connected\n";
for (const sc_api::device_info::DeviceInfo& device : *device_info) {
    std::cout << "uid: " << device.getUid()
              << " session id: " << device.getSessionId().id
              << " role: " << toString(device.getRole()) << '\n';
}
```

Search by capability, not by device model. Then the code also works with devices that arrive
later.

```cpp
// Find every ActivePedal. A search for the brake role would also find passive pedals.
auto pedals = device_info->findAllByFilter([](const sc_api::device_info::DeviceInfo& device) {
    return device.hasFeedbackType(sc_api::device_info::FeedbackType::active_pedal);
});
```

Tuner replaces the whole device info when a device connects or disconnects, or when a role, an
input mapping or a configuration changes. A `DeviceInfoChanged` event signals this. Call
`getDeviceInfo` again to get the new data.

See `examples/device_info.cpp` for a complete program.

## Read variables

Variables are the values that change often: pedal force, pedal position and the active telemetry
values. They are read-only.

[Session::getVariables](#sc_api::Session::getVariables) returns a snapshot of the definitions.
Each definition holds a direct pointer into shared memory, so a read costs one dereference.

```cpp
#include <sc-api/variable_references.h>
#include <sc-api/variables.h>

sc_api::VariableDefinitions variables = session->getVariables();

sc_api::DeviceSessionId pedal_id = pedals.front()->getSessionId();

const float* force    = variables.findValuePointer(sc_api::variable::activepedal::force, pedal_id);
const float* position = variables.findValuePointer(sc_api::variable::activepedal::pedal_face_pos_mm, pedal_id);

if (force && position) {
    std::cout << "force: " << *force << " N, position: " << *position << " mm\n";
}
```

`findValuePointer` returns `nullptr` when no variable matches. A variable exists only when a
device that supplies it is connected. Therefore always test the pointer.

The pointers stay valid as long as the `sc_api::Session` object exists. `VariableDefinitions`
holds a handle to the session, so the pointers above stay valid at least as long as `variables`.
Pass the session shared pointer together with the value pointers in more complex code.

To list every variable without knowledge of its type, use
[invokeWithValueType](#sc_api::invokeWithValueType):

```cpp
for (const sc_api::VariableDefinition& def : variables) {
    std::cout << def.name << " (" << def.type.toString() << "): ";
    sc_api::invokeWithValueType(def.type, def.value_ptr, [](auto value) {
        if constexpr (sc_api::isRevisionCountedArrayRef(value)) {
            std::cout << "<array>";
        } else {
            std::cout << value;
        }
    });
    std::cout << '\n';
}
```

The backend writes each variable atomically. It does not sample different variables at the same
instant. Two values can therefore be up to 2 ms apart. Average latency is less than 1 ms.

During one session the backend only adds definitions. It never changes or removes one. A
`VariableDefinitionsChanged` event signals that new variables are available.

See `examples/variable_definitions.cpp` and `examples/pedal_state.cpp` for complete programs.

## Send telemetry

Telemetry carries simulator values to Tuner and to the devices. The devices use these values for
their built-in effects.

Group the values that change together into one
[TelemetryUpdateGroup](#sc_api::TelemetryUpdateGroup). Configure the group once, then send it
repeatedly. Put rarely changing values in a separate group, because a group always sends every
value in it.

```cpp
#include <sc-api/telemetry.h>
#include <sc-api/telemetry_references.h>

// The group id must be unique within the application.
sc_api::TelemetryUpdateGroup group{0};

sc_api::Telemetry physics_running(sc_api::telemetry::physics_running, true);
sc_api::Telemetry engine_rpm(sc_api::telemetry::engine_rpm, 0.0f);

group.add({&physics_running, &engine_rpm});

// Resolve the names against the session. Repeat this after every reconnect.
group.configure(session->getTelemetries());

// Then, on every physics step:
engine_rpm.setValue(5400.0f);
group.send();
```

Call `configure` again on every new session, because telemetry definitions are session specific.

@note A telemetry is identified by its name and its value type. Both must match the definition,
or `configure` cannot resolve the telemetry.

## Generate force feedback effects

An effect pipeline sends timestamped offset samples to one device. The device interpolates
between the samples. It adds the result to the offsets from the built-in effects and from the
other pipelines.

Each device has four pipeline slots. Every slot can use its own offset type, gain and filter.

```cpp
#include <sc-api/ffb.h>
#include <sc-api/time.h>

sc_api::PipelineConfig config;
config.offset_type = sc_api::OffsetType::force_N;

sc_api::FfbPipeline pipeline(session, pedal_id);
pipeline.configure(config);
```

These offset types are available for an ActivePedal:

| Offset type | Meaning |
| ----------- | ------- |
| `force_N` | Force offset in newtons. `50.0` needs 50 N more force to hold the pedal in place. |
| `force_relative` | Force offset relative to the measured pedal face force. `-0.5` makes the pedal 50% softer. |
| `position_mm` | Millimeter offset of the whole force curve. `1.0` moves the rest position 1 mm towards the driver. |

A wheelbase uses `torque_Nm` or `torque_relative` instead.

Send the samples as sets. A set has a start time and a fixed time between its samples. A later
set overwrites an earlier one where the sample times overlap, and the device makes the transition
smooth. The device ignores a sample that arrives after its own start time.

```cpp
constexpr unsigned k_sample_count = 2;
float              samples[k_sample_count] = {2.0f, 2.0f};

// Give the samples 5 ms to reach the device.
auto start = sc_api::Clock::now() + std::chrono::milliseconds(5);

// Each sample plays for 10 ms if no later set arrives.
if (!pipeline.generateEffect(start, std::chrono::milliseconds(10), samples, k_sample_count)) {
    std::cerr << "Pipeline is not active\n";
}
```

Use two or more samples per set. One sample per set produces a sawtooth shape when the next set
arrives after the start time.

Do not call `Clock::now()` for every set in production code. Keep the previous end time and
increase it at a fixed rate instead. PC side scheduling jitter then cannot reach the effect.
Compare that time against the real time from time to time, because a long run makes the two
drift apart. Samples then arrive too early or too late.

See `examples/effect_pipelines.cpp` for a complete program.

## Give sim data

Sim data describes the running game and the current play session. Tuner uses it to recognize the
game and the vehicle.

Build an update with [SimDataUpdateBuilder](#sc_api::sim_data::SimDataUpdateBuilder). One builder
type exists for each section. A builder only accepts the properties of its own section.

```cpp
#include <sc-api/sim_data_builder.h>

using namespace sc_api::sim_data;

SimDataUpdateBuilder update("example-sim", true);

SimBuilder sim;
sim.set(sc_api::sim_data::sim::name, "Example Sim");
update.buildAndSet(sim);

VehiclesBuilder vehicles;
VehicleBuilder  vehicle;
vehicle.set(sc_api::sim_data::vehicle::name, "Example Vehicle 5000");
vehicle.set(sc_api::sim_data::vehicle::engine_idle_rpm, 1000);
vehicle.set(sc_api::sim_data::vehicle::engine_redline_rpm, 9000);
vehicles.buildAndAdd("example-vehicle", vehicle);
update.buildAndSet(vehicles);

session->blockingReplaceSimData(update);
```

`blockingReplaceSimData` replaces the whole document.
[Session::blockingUpdateSimData](#sc_api::Session::blockingUpdateSimData) replaces only the
values that the builder names and keeps the rest. Both have an `async` variant.

To read sim data, call [Session::getSimData](#sc_api::Session::getSimData). Every getter returns
`std::nullopt` or `nullptr` when the simulator does not supply that property.

```cpp
#include <sc-api/sim_data.h>

if (auto sim_data = session->getSimData()) {
    if (const auto* vehicle = sim_data->getPlayerVehicle()) {
        std::cout << "vehicle: " << vehicle->getName() << '\n';

        if (auto redline = vehicle->get(sc_api::sim_data::vehicle::engine_redline_rpm)) {
            std::cout << "redline: " << *redline << '\n';
        }
    }
}
```

See `examples/overview.cpp` for a complete program.

## Control LEDs

[LedControl](#sc_api::LedControl) sets the RGB lights of one device. It needs the
`control_ffb_effects` flag.

First declare which lights this application controls. Then set their colors. The color order
matches the index order.

```cpp
#include <sc-api/led_control.h>

auto device_with_leds = device_info->findFirstByFilter([](const sc_api::device_info::DeviceInfo& d) {
    return d.hasFeedbackType(sc_api::device_info::FeedbackType::rgb_light);
});

if (device_with_leds) {
    sc_api::LedControl led_control(session, device_with_leds->getSessionId());

    std::vector<uint32_t> indices;
    for (const auto& light : device_with_leds->getRgbLights()) {
        indices.push_back(light.index);
    }
    std::sort(indices.begin(), indices.end());

    std::vector<sc_api::RgbColor> colors(indices.size(), sc_api::RgbColor{255, 0, 0});
    led_control.setControlledLedsAndColors(indices.data(), colors.data(),
                                           static_cast<unsigned>(indices.size()));
}
```

`clearLeds` turns the lights off but keeps control. `releaseControl` gives the lights back to
other controllers.

See `examples/led_control.cpp` for a complete program.

## Stream dashboard frames

[DashStreamer](#sc_api::DashStreamer) sends dashboard frames to a wheel display through shared
memory. The pixel format is RGB565.

```cpp
#include <sc-api/dash_stream.h>

sc_api::DashStreamer streamer(session, device_session_id.id);

sc_api::FrameResult result = streamer.streamFrame(800, 480, rgb565_pixels);
if (result == sc_api::FrameResult::dropped) {
    // The backend has not consumed the previous frame yet. Slow down.
}

streamer.stop();  // Before the streamer is destroyed.
```

The first streamer that delivers a frame owns the device. Other senders' frames are dropped until
that owner stops. [DashStreamer::getStreamFeedback](#sc_api::DashStreamer::getStreamFeedback)
reports ownership and how many frames the device showed.

## Where to go next

- [INTEGRATION.md](INTEGRATION.md) — build, link and compiler compatibility.
- [Python.md](Python.md) — the same topics for the Python bindings.
- The `examples/` directory of the source tree — complete C++ and Python programs.
- The class list of this reference — every public type.
