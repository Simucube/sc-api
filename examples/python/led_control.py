"""Blink all LEDs on the first device with RGB lights."""

import time

import simucube_api


def find_led_device(full_info):
    """Return the first device that has RGB lights, or None."""
    if full_info is None:
        return None
    return full_info.find_first(
        lambda d: any(fb.type == simucube_api.FeedbackType.rgb_light for fb in d.feedbacks)
    )


with simucube_api.Api(
    control_flags=simucube_api.ControlFlag.control_ffb_effects,
    id_name="python_led_example",
    user_info=simucube_api.ApiUserInformation(
        display_name="Python LED Example", author="Simucube"
    ),
) as api:
    session = api.wait_for_session(timeout=10.0)

    # The device list is usually ready when the session opens, so its DeviceInfoChanged
    # event may already be delivered. A new queue gets no replay: create it first, then
    # read the list, and use the events only to wait for a device that is not there yet.
    with api.events(timeout=5.0) as events:
        device_with_leds = find_led_device(session.device_info)
        while device_with_leds is None:
            event = next(events, None)
            if event is None:
                break  # No update within the timeout, or the queue closed.
            match event:
                case simucube_api.DeviceInfoChanged(session=s):
                    device_with_leds = find_led_device(s.device_info)

    if device_with_leds is None:
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
