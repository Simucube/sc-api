"""Blink all LEDs on the first device with RGB lights."""

import time

import simucube_api

with simucube_api.Api(
    control_flags=simucube_api.ControlFlag.control_ffb_effects,
    id_name="python_led_example",
    user_info=simucube_api.ApiUserInformation(
        display_name="Python LED Example", author="Simucube"
    ),
) as api:
    session = api.wait_for_session(timeout=10.0)

    # Wait for a device with RGB lights
    device_with_leds = None
    for event in api.events(timeout=5.0):
        if event is None:
            break
        match event:
            case simucube_api.DeviceInfoChanged(session=s):
                full_info = s.device_info
                device_with_leds = full_info.find_first(
                    lambda d: any(
                        fb.type == simucube_api.FeedbackType.rgb_light for fb in d.feedbacks
                    )
                )
                if device_with_leds:
                    break

    if not device_with_leds:
        print("No device with RGB lights found")
        raise SystemExit(1)

    print(f"Blinking LEDs on device {device_with_leds.uid}")
    indices = sorted(light.index for light in device_with_leds.rgb_lights)
    print(f"Controlling lights: {indices}")

    with simucube_api.LedControl(session, device_with_leds.session_id) as led:
        red = [simucube_api.RgbColor(255, 0, 0)] * len(indices)
        for _ in range(100):
            led.set_leds(indices, red)
            time.sleep(0.02)
            led.clear()
            time.sleep(0.02)

    print("Done")
