# Simucube API

Simucube API is intended for interacting with Simucube devices and Tuner software. 

SC-API provides interface to control and get information about Simucube devices.
API allows generating force feedback effects and reading variable data from devices and Tuner.

Current API implementation uses C++17 and supports only Windows platform.

# Current state of API

Currently API is mostly focused on needs of simulator developers and tools that want to directly interract with the devices.
API does not currently offer way to edit or switch Tuner device profiles.
This functionality will be implemented later and possibly with slightly different communication methods to make it easier to build external tools.

Currently API is in alpha state and does not offer API or ABI compatibility to future API or Tuner versions.
This API version is only designed to work with one specific Tuner version and using any other Tuner version will likely not work.
In this early phase we want to be ably to make significant changes to the implementation if there is need for that and we don't want any limitations.
This means that any API user may have to rebuilt their application with new API version when new Tuner version is released.

API version 1.0 will be the first stable version and from that onwards the ABI between API and the backend (Simucube Tuner) will be considered stable. At that point releasing
application based on this API is safe to do without intention to keep up with updates to Tuner and API as application will continue to function.
New functionality will be added, but old functionality is kept backwards compatible. Target timeline for releasing first stable version is during Fall 2025.

Currently supported Tuner version: [Simucube Tuner 2.6.1](https://downloads.simucube.com/SimucubeTunerSetup-2.6.1.exe)

[Simucube API tools](https://downloads.simucube.com/sc-api-tools-2025-06-19.7z) allow viewing available variable data, device and sim data information.
They also allow creating simple effect pipelines to test functionality. These use the API to fetch all information and just display it.

## Device support

Currently support is mostly focused on ActivePedal to allow using full capabilities of this new type of device.
Device information is provided for connected wireless wheels and SC-link Hub handling the connection.
Limited support for SC2 will arrive later at least to provide consistent device information support, but it is unlikely that effect pipelines will ever be supported by SC2 due to completely different architecture and design.

# Contributing

This project uses [Github issues](https://github.com/Simucube/sc-api/issues) for managing bug reports. Do note that during this phase, API is only guaranteed to work
with the "Currently supported Tuner version" listed above.
Try searching if there is already existing issue regarding the problem and only report issues that occur when using matching Tuner and API versions. 

Feature requests and questions can also be asked through issues. Remember to tag the issue correctly.

Pull requests are welcome, but we likely won't accept any that modify contents of core/ directory, because it is exported from internal sources.
We'll try to move more of the functionality out of "core" over time and limit "core" to just contain low-level protocol.

## Common terms

- **Session** - Connection between the API user and Simucube Tuner that acts as the backend of the API
    - Starts when API user establishes the session and ends when the user program exits or otherwise closes the API
        - If Tuner is closed and re-opened the session must be re-established to receive updated data
    - Many things are only valid within a session and are invalidated if the session changes
- **Device session id**, uint16_t - Identifies a device within the session. May change between sessions, but won't change within the session even if the device disconnects and reconnects
- **Device uid**: string - Uniquelly identifies the physical device. Will stay the same even if session changes
- **Action** - Binary formatted command that is sent through UDP and won't be replied. Used for low latency telemetry and effect pipeline data
- **Command** - BSON formatted request response protocol over TCP. Used for bidirectional async communication and configuration with Tuner


# Features

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

Allows giving and receiving information about the current simulator state and play session:
  - Allows Simucube Tuner to identify currently running game and used vehicle
  - Allows API users to fetch this information from other sources
  
Data is represented by BSON document that is shared through shared memory block and can be updated with commands.





