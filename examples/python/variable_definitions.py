"""Print all variable definitions and their current values."""

import simucube_api

with simucube_api.Api() as api:
    session = None
    variables = None

    for event in api.events(timeout=5.0):
        if event is None:
            # Print current values every 5s
            if variables and len(variables) > 0:
                print("Current values:")
                for var in variables:
                    val = variables.read_value(var)
                    print(f"  {var.name}: {val}")
                print()
            continue

        update_defs = False
        match event:
            case simucube_api.SessionStateChanged(state=state, session=s):
                if state == simucube_api.SessionState.connected_monitor:
                    session = s
                    update_defs = True
                else:
                    session = None
                    variables = None
            case simucube_api.VariableDefinitionsChanged(session=s):
                session = s
                update_defs = True

        if update_defs and session:
            variables = session.variables
            print(f"\nVariable definitions changed ({len(variables)} variables):")
            for var in variables:
                val = variables.read_value(var)
                print(f"  {var.name}  type={var.type}  device={var.device_session_id}  value={val}")
            print()
