"""Read and print ActivePedal force/position at 1Hz."""

import simucube_api

with simucube_api.Api() as api:
    pedals = []  # list of (uid, role, force_var, position_var)
    variables = None

    for event in api.events(timeout=1.0):
        if event is None:
            # Timeout -- print current pedal state
            if pedals:
                print("ActivePedals:")
                for uid, role, force_var, pos_var in pedals:
                    force = variables.read_value(force_var)
                    position = variables.read_value(pos_var)
                    print(f"  {role}, uid={uid}, position: {position} mm, force: {force} N")
                print()
            continue

        match event:
            case simucube_api.DeviceInfoChanged(session=session):
                variables = session.variables
                full_info = session.device_info
                active_pedals = full_info.find_all(
                    lambda d: any(
                        fb.type == simucube_api.FeedbackType.active_pedal for fb in d.feedbacks
                    )
                )
                pedals = []
                for ap in active_pedals:
                    force_var = variables.find("ap.pedal_face_force_N", device=ap.session_id)
                    pos_var = variables.find("ap.pedal_face_pos_mm", device=ap.session_id)
                    if force_var and pos_var:
                        pedals.append((ap.uid, ap.role, force_var, pos_var))
                print(f"Devices changed -- found {len(pedals)} ActivePedal(s)")
