"""Print connected device info whenever it changes."""

import simucube_api

with simucube_api.Api() as api:
    for event in api.events():
        match event:
            case simucube_api.DeviceInfoChanged(session=session):
                full_info = session.device_info
                if full_info is None:
                    continue
                print("Connected devices:")
                for device in full_info:
                    print(
                        f"  UID: {device.uid}  Session ID: {device.session_id}"
                        f"  Role: {device.role}"
                    )
                    print(f"    Manufacturer: {device.manufacturer_id}")
                    print(f"    Product: {device.product_id}")
                    if device.parent_session_id:
                        parent = full_info.get_by_session_id(device.parent_session_id)
                        if parent:
                            print(f"    Parent: {parent.uid}")
                    for fb in device.feedbacks:
                        print(f"    Feedback: {fb.id} type={fb.type}")
                print()
