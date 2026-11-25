"""Stream a dash with buttons whose touches do not move the PC mouse pointer.

Streams a page with one button to the first wheel that supports mouse suppressed
areas. A tap on the button opens a pop-up, and a tap on the close button of the
pop-up closes it. Each frame carries its visible button as a mouse suppressed
area.

    pip install numpy
    python touch_areas.py
"""

import signal
import time

import numpy as np

import simucube_api

SUBMIT_FPS = 30

running = True


def on_signal(_signum, _frame):
    global running
    running = False


def rgb565(r, g, b):
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)


def find_screen(session):
    """Return (device session id, screen) of the first screen that takes mouse suppressed areas, or None."""
    info = session.device_info
    if info is None:
        return None
    for device in info:
        for screen in device.screens:
            if screen.max_mouse_suppressed_areas >= 1:
                return device.session_id, screen
    return None


def wait_for_screen(api):
    """Return (session, device session id, screen) when Tuner gives control and a screen is found, or None on stop."""
    while running:
        session = api.session
        if session is not None and session.state == simucube_api.SessionState.connected_control:
            found = find_screen(session)
            if found is not None:
                device_id, screen = found
                return session, device_id, screen
        time.sleep(1)
    return None


def area_from_screen_fractions(screen, x, y, w, h):
    return simucube_api.TouchArea(
        int(screen.width * x), int(screen.height * y), int(screen.width * w), int(screen.height * h)
    )


def fill_rect(frame, area, color):
    frame[area.y : area.y + area.h, area.x : area.x + area.w] = color


class Page:
    """A main page with an open button, and a pop-up with a close button on top of it."""

    def __init__(self, screen):
        self.frame = np.empty((screen.height, screen.width), dtype=np.uint16)
        self.open_button = area_from_screen_fractions(screen, 0.1, 0.6, 0.25, 0.25)
        self.popup = area_from_screen_fractions(screen, 0.2, 0.2, 0.6, 0.6)
        self.close_button = area_from_screen_fractions(screen, 0.4, 0.55, 0.2, 0.15)
        self.popup_open = False

    def visible_buttons(self):
        return [self.close_button if self.popup_open else self.open_button]

    def toggle_popup(self):
        self.popup_open = not self.popup_open

    def render(self):
        self.frame.fill(rgb565(0, 0, 32))
        if self.popup_open:
            fill_rect(self.frame, self.popup, rgb565(80, 80, 80))
            fill_rect(self.frame, self.close_button, rgb565(200, 0, 0))
        else:
            fill_rect(self.frame, self.open_button, rgb565(0, 160, 0))
        return self.frame


def is_tap_on_visible_button(event, page):
    tap = event.single_finger_tap
    return tap is not None and any(button.contains(*tap) for button in page.visible_buttons())


def main():
    signal.signal(signal.SIGINT, on_signal)
    signal.signal(signal.SIGTERM, on_signal)

    user_info = simucube_api.ApiUserInformation(
        display_name="Touch area example", type="dash_controller", author="Simucube", version_string="1.0"
    )
    with simucube_api.Api(
        control_flags=simucube_api.ControlFlag.control_display, id_name="touch_areas", user_info=user_info
    ) as api:
        print("Waiting for Tuner and a wheel that supports mouse suppressed areas...")
        found = wait_for_screen(api)
        if found is None:
            return 0
        session, device_id, screen = found

        with simucube_api.DashStreamer(session, device_id) as dash, simucube_api.InputEventReader(session) as reader:
            if not dash.open() or not reader.open():
                print("Cannot open the stream or the input events")
                return 1

            page = Page(screen)
            print("Streaming. Tap the green button on the wheel.")
            next_frame_s = time.perf_counter()

            while running and reader.is_valid:
                events = reader.read().events
                # Taps count only while this streamer owns the device; else another streamer's frame is on the screen
                if dash.is_owner:
                    for event in events:
                        if event.device_session_id.id == device_id.id and is_tap_on_visible_button(event, page):
                            page.toggle_popup()
                            break  # the rest of the batch was on the old page

                dash.stream_frame(page.render(), areas=page.visible_buttons())

                next_frame_s += 1.0 / SUBMIT_FPS
                time.sleep(max(0.0, next_frame_s - time.perf_counter()))

        # The with block has called stop(); this lets the Api thread send it before the Api closes.
        time.sleep(0.1)
    print("Stopped.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
