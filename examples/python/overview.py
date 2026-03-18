"""Overview: telemetry + sim data in a single script."""

import time

import simucube_api

with simucube_api.Api(
    control_flags=(
        simucube_api.ControlFlag.control_telemetry | simucube_api.ControlFlag.control_sim_data
    ),
    id_name="python_overview",
    user_info=simucube_api.ApiUserInformation(
        display_name="Python Overview", type="tool", author="Simucube", version_string="0.1"
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

    # Send sim data
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
    print("Sim data sent")

    # Send telemetry in a loop
    telem_defs = session.telemetries
    group = simucube_api.TelemetryUpdateGroup(telem_defs)

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

        group["engine_rpm"] = rpm
        group["physics_running"] = True
        group.send()

        time.sleep(0.01)  # 100 Hz
