# Getting Started {#GettingStarted}

This guide has instructions for installing the Simucube API and small examples to get started on developing with the API.

Overview of API:s features can be found [here](#Features).

## Connecting to API session

[ApiCore](#sc_api::ApiCore) class is lower level implementation of establishing sessions and managing communication to the backend.
It allows full control over establishing new sessions and updating the session state. [Api](#sc_api::Api) class
wraps [ApiCore](#sc_api::ApiCore) class and creates a background thread to handle all communication and management operations.
Usually it is easier to use [Api](#sc_api::Api) to handle the session management unless more manual control is required.

Api [Session](#sc_api::Session) is established when [Api](#sc_api::Api) class successfully reads valid shared memory header
created by the backend process (Simucube Tuner). Session stays connected as long as it isn't closed and the backend process is running.
Create [EventQueue](#sc_api::Api::EventQueue) to receive updates about the session state changes.

For read-only access (device info, sim data, and variables) this is everything needed to be done.
Sending commands, controlling devices and telemetry requires establishing control session by registering.

Most functionality requires that Api Session is established. Api handles this automatically and EventQueue can be used
to wait for a session to be available.
```cpp
sc_api::Api api;
std::unique_ptr<sc_api::Api::EventQueue> eventQueue = api.createEventQueue();

std::shared_ptr<Session> session;
auto                     timeout     = std::chrono::steady_clock::now() + std::chrono::seconds(10);
std::cout << "Wait 10s for Api session to be estabilished" << std::endl;
while (auto event = eventQueue->tryPopUntil(timeout)) {
    handle(*event, [&](const session_event::SessionStateChanged& s) {
        if (s.state == sc_api::SessionState::connected_monitor) {
            session = s.session;
            break;
        }
    });
}

// Do something with the session
```

### Enabling control

Access to controlling force feedback effects, telemetry, and other simulator data needs to be enabled if it is needed.
Enabling control without any encryption can be done using [NoAuthControlEnabler](#sc_api::NoAuthControlEnabler), which takes control flags as parameter which enables the control to all features it is needed for.

```cpp
sc_api::ApiUserInformation user_info;
user_info.display_name   = "example";
user_info.type           = "";
user_info.path           = "";
user_info.author         = "Simucube";
user_info.version_string = "1.0";

sc_api::NoAuthControlEnabler* control_enabler = new sc_api::NoAuthControlEnabler(
                &api,
                sc_api::Session::ControlFlag::control_telemetry | sc_api::Session::ControlFlag::control_ffb_effects,
                "my_simulator_id",
                user_info);
```

## Get device info

Device info is bson formatted data in shared memory that is available after a session is established.

```cpp
std::shared_ptr<sc_api::device_info::FullInfo> device_info = session->getDeviceInfo();
std::cout << device_info->getDeviceCount() << " devices connected\n";
for (const device_info::DeviceInfo& device : *device_info) {
    std::cout << "Device UID: " << device.getUid()
        << " Session id: " << device.getSessionId().id
        << " role: " << toString(device.getRole())
        << "\n";
}
```

Session specific things, such as device session ids may change when session is restarted.
API user can only rely on session specific things staying the same when session is kept connected.
Session specific data should **not** be stored in any configs and instead device UID or feature based mapping should be used to refer devices.

## Variables

Variables allow read-only access to the state of the connected devices and active telemetry values.
Variable definitions list available data and where to access them in shared memory.


```cpp


```

## Control devices

### Effect pipeline configuration

Effect pipelines are created and used through [FfbPipeline](#sc_api::FfbPipeline) class.
It creates pipeline for a device within the session and allows configuring and sending effects to devices.


```cpp

sc_api::FfbPipeline pipeline(session, device_session_id);

sc_api::PipelineConfig config;
config.offset_type = sc_api::OffsetType::force_relative;

pipeline.configurePipeline(config);
```

### Generating effects

Effect offset samples are provided in sets of continuous samples that have fixed time between them and precisely defined start time. When new samples are added to a pipeline they override any previous samples if sample times overlap and transition between them is handled smoothly. Any samples that arrive later than their precise start time are ignored.

Sample set sample frequency can differ with every set, but it is probably easier to generate a fixed number of samples every game physics update step and send them with a starting timestamp a couple milliseconds in future. 

To generate effect use [sc_api::FfbPipeline::generateEffect](#sc_api::FfbPipeline::generateEffect). 

```cpp
constexpr int number_of_samples = 100;
float offsets[number_of_samples] = {};

// Fill offset buffer with simple alternating values for this example
for (int i = 0; i < number_of_samples; i++) { 
    if (i % 2) {
        offsets[i] = 0.1f;
    } else {
        offsets[i] = -0.1f;
    }
}

pipeline.generateEffect(
            sc_api::time::getTimestamp() + sc_api::time::getTimestampFrequencyHz() / 250 /* give 4ms time for these samples to reach the pedal */,
            sc_api::time::microsecondsToTimestampTicks(10000), 
            offsets, 
            number_of_samples);
```

It is recommended to keep track for the previous timestamp sent and increase it at regular rate instead of
using getTimestamp() to generate new timestamp every time effect is sent to avoid PC side scheduling jitter and other
inconsistencies affecting the generated effect. Timestamp updating must be monitored to avoid it getting out sync from
the actual current time as that could lead to samples arriving too late or early when application has been running for
a long time.
