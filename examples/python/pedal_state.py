"""Read and print ActivePedal force/position at 1Hz."""

import simucube_api

with simucube_api.Api() as api:
    pedals = []  # list of (uid, role, VariableObject)

    for event in api.events(timeout=1.0):
        if event is None:
            # Timeout -- print current pedal state
            if pedals:
                print("ActivePedals:")
                for uid, role, ap in pedals:
                    print(f"  {role}, uid={uid}, position: {ap.position} mm, force: {ap.force} N")
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
                    try:
                        ap_vars = simucube_api.VariableObject(variables, {
                            "force": "ap.force_N",
                            "position": "ap.pedal_face_pos_mm",
                        }, device_id=ap.session_id)
                    except KeyError:
                        continue
                    pedals.append((ap.uid, ap.role, ap_vars))
                print(f"Devices changed -- found {len(pedals)} ActivePedal(s)")
