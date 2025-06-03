# Simucube API

Simucube API is intended for interacting with Simucube devices and Tuner software. 

SC-API provides interface to control and get information about Simucube devices.
API allows generating force feedback effects and reading variable data from devices and Tuner.

Current API implementation supports C++17.

## Common terms

- **Session** - Connection between the API user and Simucube Tuner that acts as the backend of the API
    - Starts when API user establishes the session and ends when the user program exits or otherwise closes the API
        - If Tuner is closed and re-opened the session must be re-established to receive updated data
    - Many things are only valid within a session and are invalidated if the session changes
- **Device session id**, uint16_t - Identifies a device within the session. May change between sessions, but won't change within the session even if the device disconnects and reconnects
- **Device uid**: string - Uniquelly identifies the physical device. Will stay the same even if session changes
- **Action** - Binary formatted command that is sent through UDP and won't be replied. Used for low latency telemetry and effect pipeline data
- **Command** - BSON formatted request response protocol over TCP. Used for bidirectional async communication and configuration with Tuner

# Features {#Features}

## Device info

Device info is BSON formatted data in shared memory that lists features and information about all connected devices.
Device info does not contain any frequently changing data. Device data stays mostly the same during a session.
Data is updated if device connects/disconnects, input mapping changes, device role or configuration changes.

### Device info data

- **Device UID** allows unique indentifying of physical devices even if device roles are changed or API session is restarted.
- **Device session ID**, API refers to this ID in many cases. Changes betweeen sessions.
- Device connection hierarchy
- Information about device's physical controls and which inputs or feedbacks they are connected to.
- Intended roles for the inputs -> what is the default usage for the input
- Physical limits of feedbacks
- Information of which input is connected to which HID input

## Variable data

Frequently updating variables from Tuner and connected devices. Variable definitions are session specific. 

Variables are located in the shared memory and are updated atomically. Values of different variables are not synchronized to guarantee the minimal possible latency so it is possible that when reading more than one variable the values are not sampled at the same instant. Difference between variable samples is less than 2 ms and the average latency of variable data is less than 1 ms.

Variable definition: 
- **name:** ascii string: eg. "ap.pedal_face_force_N"
- **type:** enum: eg. f32, i32
- **device session id:** id of the device the variable belongs to, 0 if the variable is not device specific

## Telemetry data

Provides a way to communicate telemetry data to Tuner and devices and enables generation on built-in ActivePedal effects.
Telemetry data types are identified by their name and value type. API user can form telemetry update groups that can be
sent to Tuner and then to the connected devices.

Telemetry definition: 
- **name:** ascii string: eg. "engine_rpm"
- **type:** enum: eg. f32, i32
- **numeric id:** Used to identify the telemetry when configuring update group.


## Effect pipelines

- A way to generate custom effects for ActivePedals (and other ffb devices later)
- Buffers timestamped sample data and plays effect in high frequency and synchronization
- Allows up-to 20kHz frequency data to be sent to the devices that devices then interpret and transform into effects
- ActivePedal supports following types of offsets:
    - force offset - Newton offset to the force applied to the drivers feet
        - 50.0N offset will cause pedal to require 50N more force to be kept in its current place (otherwise the pedal will start move towards to driver)
    - relative force offset - force offset, but the offset is specified in relation to the measured pedal face force
        - -0.5 offset will make pedal 50% lighter
    - position offset - Millimeter offset that offsets the whole force curve movement range
        - 1mm offset will move the pedal rest position and the backstop 1mm towards the driver (measured from the center of the pedal face)
- Currently there are 4 separate effect pipelines that can be configured differently and fed with different data.
   - Offsets from all effect pipelines and the built-in effects are combined to form the actual offsets used in the control

## Sim data

- A way to give Simucube Tuner information about the current simulator state and play session
  - Allows Simucube Tuner to identify currently running game and used vehicle
  - Allows API users to fetch this information from other sources
- 

# Doxygen html documentation

Generated HTML documentation is available by opening sc-api/docs/html/index.html
It contains more information about the API and its usage.


# Directory structure

```
    SimucubeTuner/
        tuner.exe - Use this to start Simucube Tuner that handles all communication between sc-api and devices
    sc-api/
        docs/ - Documentation
            html/ - Generated Doxygen documentation and some handwritten general documentation
        inc/ - API include directory
        src/ - API source files
        examples/ - Simple example applications demonstrating API features
        view/ - Tools for helping to interact with SC-API
            sc-api-view.exe - Allows viewing and plotting variable data and which should help with debugging effects and seeing what data is available. It can also save the full device information BSON data as a JSON file.
            sc-api-test_tool.exe - Allows viewing device information data by device and configuring pipelines to generate simple ffb effects for ActivePedals.
```
