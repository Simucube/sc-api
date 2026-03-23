"""Send and read back telemetry using attribute-style helpers."""

import time

import simucube_api

with simucube_api.Api(
    control_flags=simucube_api.ControlFlag.control_telemetry,
    id_name="python_telemetry_object",
    user_info=simucube_api.ApiUserInformation(
        display_name="Telemetry Object Example", type="tool", author="Simucube", version_string="0.1"
    ),
) as api:
    session = api.wait_for_session(timeout=10.0)
    print(f"Connected: state={session.state}")

    # Wait for control
    for event in api.events(timeout=10.0):
        if event is None:
            break
        match event:
            case simucube_api.SessionStateChanged(state=state):
                if state == simucube_api.SessionState.connected_control:
                    break

    # Set up telemetry sending with TelemetryUpdateObject
    telem = simucube_api.TelemetryUpdateObject(session.telemetries)
    telem.physics_running = True
    telem.engine_rpm = 1000.0

    # Set up telemetry reading with telemetry_variables
    tv = simucube_api.telemetry_variables(session.variables, session.telemetries)

    rpm = 1000.0
    rpm_delta = 10.0

    while True:
        rpm += rpm_delta
        if rpm >= 8000.0:
            rpm = 8000.0
            rpm_delta = -10.0
        elif rpm <= 1000.0:
            rpm = 1000.0
            rpm_delta = 10.0

        # Write telemetry values as attributes
        telem.engine_rpm = rpm
        telem.send()

        # Read back telemetry values as variables
        if tv.has_engine_rpm:
            print(f"engine_rpm: sent={rpm:.0f}  read_back={tv.engine_rpm:.0f}")

        time.sleep(0.1)  # 10 Hz
