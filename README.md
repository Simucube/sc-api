# Simucube API

Simucube API (sc-api) controls Simucube devices and reads information from them. Simucube Tuner
is the backend of the API and must run on the same PC.

With this API you can:

- read the state of the connected devices and the active telemetry values,
- generate custom force feedback effects,
- send telemetry to Tuner and to the devices,
- give and read simulator state data,
- control the RGB lights of a device,
- stream dashboard frames to a wheel display.

The implementation uses C++17 and supports only Windows. Python bindings are available.

New users start with [docs/GettingStarted.md](docs/GettingStarted.md).

# Current state of API

It is mostly focused on the needs of simulator developers and of tools that interact with the
devices directly. It has no way to edit or switch the device profiles of Tuner. That feature
arrives later, possibly with different communication methods that make external tools simpler.

Currently we attempt to keep backwards compatibility with future Tuner versions, but we do not
guarantee it yet. A newer Tuner version usually supports an older API version. An older Tuner
version does not necessarily support a newer API version, and its feature set can be limited.

Version 1.0 will be the first stable version. From that version onwards, the ABI between the API
and Simucube Tuner stays stable. An application that is released against a stable version keeps
working, and it does not have to follow every Tuner and API update. New features arrive, and old
features stay backwards compatible.

Currently supported Tuner version: [Simucube Tuner 3.1.4](https://downloads.simucube.com/SimucubeTunerSetup-3.1.4.exe)

With the [Simucube API tools](https://downloads.simucube.com/sc-api-tools-2025-12-19.7z) you can view the available variable data, the device information and the sim data.
You can also create simple effect pipelines to test the features. The tools read all information through the API and display it.

## Device support

Support is mostly focused on ActivePedal, so that the full capabilities of this new device type are
available. Device information is also given for the connected wireless wheels and for the SC-link
Hub that handles the connection.

Limited support for SC2 arrives later. It will at least give consistent device information. Effect
pipelines for SC2 are unlikely, because its architecture and design are completely different.

# Installing and integrating

```
cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=<install-dir>
cmake --build build --config Release
cmake --install build --config Release
```

The install tree keeps integration simple for all build systems:

- All headers are below one directory, `<install-dir>/include`.
- All object code is in one library, `<install-dir>/lib/sc-api`. Cryptography
  and network code are already inside it.
- Package config files for CMake (`find_package(sc-api)`) and for pkg-config
  are installed too, but they are not necessary.

Configure with `-DSC_API_SHARED=ON` to build a DLL instead of a static library.

See [docs/INTEGRATION.md](docs/INTEGRATION.md) for command line examples and for
the compiler compatibility rules.

## Python bindings

The `simucube_api` package gives the features of the C++ API to Python 3.11 and newer. Install it from the source tree:

```
pip install .
```

```python
import simucube_api

with simucube_api.Api() as api:
    session = api.wait_for_session(timeout=10.0)

    device_info = session.device_info
    if device_info is not None:
        for device in device_info:
            print(f"{device.uid}  {device.session_id}  {device.role}")
```

See [docs/Python.md](docs/Python.md) for the full Python guide. The `examples/python/` directory
holds complete programs that match the C++ examples. Every class and method carries a docstring, so
`help(simucube_api.Api)` works.

# Contributing

This project uses [Github issues](https://github.com/Simucube/sc-api/issues) to manage bug reports. During this phase, the API is only guaranteed to work
with the "Currently supported Tuner version" above.
Search for an existing issue about the problem first. Report only problems that occur with matching Tuner and API versions.

You can also send feature requests and questions through issues. Remember to tag the issue correctly.

Pull requests are welcome.

## Common terms

- **Session** — the connection between the API user and Simucube Tuner, which is the backend of
  the API.
    - It starts when the API user opens it. It ends when the user program exits or closes the API.
    - If Tuner closes and opens again, open a new session to receive current data.
    - Many things are valid only inside one session. A change of session makes them invalid.
- **Device session id** (`uint16_t`) — identifies a device inside one session. It can change
  between sessions. It does not change inside one session, even if the device disconnects and
  connects again.
- **Device uid** (string) — identifies the physical device. It stays the same across sessions.
- **Action** — a binary message that goes over UDP and gets no reply. Used for low latency
  telemetry and effect pipeline data.
- **Command** — a BSON request and reply protocol over TCP. Used for two-way asynchronous
  communication and for configuration.


# Features {#Features}

## Device info

Device info is BSON data in shared memory. It lists the features and the information of every
connected device. It holds no fast changing data, so it stays mostly the same during a session.
Tuner replaces it when a device connects or disconnects, or when an input mapping, a device role or
a configuration changes.

### Device info data

- **Device UID** identifies a physical device, even after a device role change or a session
  restart.
- **Device session ID**, which the API uses in many places. It changes between sessions.
- The device connection hierarchy.
- The physical controls of the device, and the inputs or feedbacks that each control connects to.
- The intended role of each input, which gives its default use.
- The physical limits of each feedback.
- The HID input that each input connects to.

## Variable data

Variables are the fast changing values from Tuner and from the connected devices. Variable
definitions are session specific.

The variables are in shared memory, and the backend writes each one atomically. It does not sample
different variables at the same instant, because that keeps the latency at a minimum. Two values can
therefore be up to 2 ms apart. Average latency of variable data is less than 1 ms.

A variable definition holds:
- **name** — ASCII string, for example `ap.force_N`
- **type** — enum, for example `f32` or `i32`
- **device session id** — the device that the variable belongs to, or 0 if the variable is global

## Telemetry data

Telemetry sends simulator values to Tuner and to the devices. The devices use these values to
generate their built-in ActivePedal effects. A telemetry is identified by its name and its value
type. Collect the values into telemetry update groups, and send each group as one set.

A telemetry definition holds:
- **name** — ASCII string, for example `engine_rpm`
- **type** — enum, for example `f32` or `i32`
- **numeric id** — identifies the telemetry when an update group is configured

## Effect pipelines

Effect pipelines generate custom effects for ActivePedal, and for other force feedback devices
later.

- A pipeline buffers timestamped sample data. The device plays the effect at high frequency and
  keeps it synchronized.
- Sample data of up to 20 kHz can reach the devices. The devices then transform it into effects.
- Four separate pipelines are available. Each one takes its own configuration and its own data.
- The offsets from every pipeline and from the built-in effects are added together. The sum gives
  the offsets that the control uses.

ActivePedal supports these offset types:

| Offset type | Meaning | Example |
| ----------- | ------- | ------- |
| Force offset | Newton offset to the force under the driver's foot | `50.0` needs 50 N more force to hold the pedal in place. Without it the pedal moves towards the driver. |
| Relative force offset | Force offset relative to the measured pedal face force | `-0.5` makes the pedal 50% lighter. |
| Position offset | Millimeter offset of the whole force curve movement range | `1.0` moves the rest position and the backstop 1 mm towards the driver, measured from the center of the pedal face. |

A wheelbase takes a torque offset in newton meters, or a relative torque offset.

## Sim data

Sim data gives and receives information about the current simulator state and play session.

- Tuner uses it to recognize the running game and the vehicle in use.
- API users can read the same information when another source supplies it.

The data is one BSON document in a shared memory block. Commands update it.

## LED control

LED control sets the RGB lights of an SC-link connected device. An application first declares which
lights it controls, and then sets their colors. A higher priority controller can take a light over.

## Dashboard streaming

Dashboard streaming sends frames to a wheel display through shared memory. A stream-capable
display appears in device info as a `screen` feedback. The feedback gives the frame size and the
pixel format. The only currently supported pixel format is RGB565.

The first sender that delivers a frame owns the device until that sender stops. The backend
reports ownership and display progress back to every sender. Streaming needs control access. No
dedicated streaming flag exists.
