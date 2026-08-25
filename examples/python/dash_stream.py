"""Stream the contents of a window to a Simucube device screen.

Grabs the client area of a window with dxcam, scales it to the screen of the
device and sends it as RGB565 frames with DashStreamer. The window may be on any
monitor, and it may be moved between monitors while streaming.

    pip install numpy opencv-python "dxcam>=0.3" pywin32
    python dash_stream.py --window "Assetto Corsa" --fps 30
"""

from __future__ import annotations

import argparse
import ctypes
import time

import cv2
import dxcam
import win32api
import win32con
import win32gui

import simucube_api


def set_dpi_awareness() -> None:
    # Must run before any window rect is read and before dxcam is created, or the
    # window coordinates and the capture coordinates use different pixel scales.
    try:
        ctypes.windll.shcore.SetProcessDpiAwareness(2)  # per-monitor aware
    except OSError:
        ctypes.windll.user32.SetProcessDPIAware()


def find_window(title_substring: str) -> tuple[int, str] | None:
    """Return (hwnd, title) of the first visible window whose title contains the substring."""
    needle = title_substring.lower()
    matches: list[tuple[str, int]] = []

    def on_window(hwnd: int, _param: object) -> None:
        if win32gui.IsWindowVisible(hwnd):
            title = win32gui.GetWindowText(hwnd)
            if needle in title.lower():
                matches.append((title, hwnd))

    win32gui.EnumWindows(on_window, None)
    if not matches:
        return None
    # Prefer an exact title match over a substring one, so that a browser tab
    # mentioning the game does not win over the game window itself.
    matches.sort(key=lambda m: m[0].lower() != needle)
    title, hwnd = matches[0]
    return hwnd, title


def find_output(display: str) -> tuple[int, int] | None:
    """Return the (device_idx, output_idx) that dxcam uses for a display device name."""
    # dxcam has no public lookup for this, but the outputs of its module level
    # factory carry the same `\\.\DISPLAYn` names that win32api reports.
    factory = getattr(dxcam, "__factory", None)
    if factory is None:
        return None
    for device_idx, outputs in enumerate(factory.outputs):
        for output_idx, output in enumerate(outputs):
            if output.devicename == display:
                return device_idx, output_idx
    return None


def client_region(hwnd: int) -> tuple[str, tuple[int, int, int, int] | None] | None:
    """Return the display the window is on and its client area in that display.

    Returns None if the window is gone. The region is None while the client area is
    fully off screen, which is not an error.
    """
    try:
        left, top, right, bottom = win32gui.GetClientRect(hwnd)
        left, top = win32gui.ClientToScreen(hwnd, (left, top))
        right, bottom = win32gui.ClientToScreen(hwnd, (right, bottom))
        monitor = win32api.MonitorFromWindow(hwnd, win32con.MONITOR_DEFAULTTONEAREST)
        info = win32api.GetMonitorInfo(monitor)
    except win32gui.error:
        return None  # The window is gone.

    # A dxcam region is local to its output, and dxcam raises if it reaches outside.
    origin_x, origin_y, far_x, far_y = info["Monitor"]
    width, height = far_x - origin_x, far_y - origin_y
    left, right = max(0, min(left - origin_x, width)), max(0, min(right - origin_x, width))
    top, bottom = max(0, min(top - origin_y, height)), max(0, min(bottom - origin_y, height))
    if right - left < 1 or bottom - top < 1:
        return info["Device"], None
    return info["Device"], (left, top, right, bottom)


def find_screen_device(full_info: simucube_api.FullInfo | None) -> simucube_api.DeviceInfo | None:
    """Return the first device that has a streamable screen, or None."""
    if full_info is None:
        return None
    return full_info.find_first(lambda d: len(d.screens) > 0)


def stream(session, device_id, screen, hwnd: int, fps: float) -> None:
    camera = None
    camera_display = None
    frame_period_s = 1.0 / fps
    t0_ns = time.perf_counter_ns()
    frames = 0
    dropped = 0
    last_frame = None
    first_failed_s = None
    warned = False

    with simucube_api.DashStreamer(session, device_id) as dash:
        try:
            while True:
                # Pace against an ideal timeline so that a late grab does not add drift.
                target_s = frames * frame_period_s
                now_s = (time.perf_counter_ns() - t0_ns) / 1e9
                if now_s < target_s:
                    time.sleep(min(target_s - now_s, frame_period_s))
                    continue

                located = client_region(hwnd)
                if located is None:
                    print("Window closed")
                    break
                display, region = located

                if display != camera_display:
                    output = find_output(display)
                    if output is None:
                        print(f"dxcam has no capture output for display {display}")
                        break
                    # dxcam misbehaves with two cameras on one output, so release first.
                    if camera is not None:
                        camera.release()
                    camera = dxcam.create(*output, output_color="BGR")
                    camera_display = display

                grabbed = region is not None and not win32gui.IsIconic(hwnd)
                frame = camera.grab(region) if grabbed else None
                if frame is not None:
                    resized = cv2.resize(frame, (screen.width, screen.height), interpolation=cv2.INTER_AREA)
                    # C-contiguous uint8 of shape (height, width, 2): packed RGB565.
                    last_frame = cv2.cvtColor(resized, cv2.COLOR_BGR2BGR565)
                elif last_frame is None:
                    time.sleep(0.001)  # Nothing captured yet.
                    continue
                # grab() returns None while the screen is unchanged, and a minimized or
                # fully off screen window is not grabbed at all. Re-send the last frame
                # in all cases: the device leaves streaming after about 3 s without one.

                result = dash.stream_frame(last_frame)
                frames += 1
                if result == simucube_api.FrameResult.dropped:
                    dropped += 1
                elif result == simucube_api.FrameResult.failed:
                    # Also returned while the streamer is still connecting.
                    if first_failed_s is None:
                        first_failed_s = now_s
                    elif not warned and now_s - first_failed_s > 5.0:
                        print("No frame delivered for 5 s. Is another sender streaming to the device?")
                        warned = True
                else:
                    first_failed_s = None
        except KeyboardInterrupt:
            print("\nStopping")
        finally:
            if camera is not None:
                camera.release()
        feedback = dash.get_stream_feedback()

    print(f"Sent {frames} frames, {dropped} dropped by the backend")
    print(f"Device showed {feedback.device_frame_counter} frames and dropped {feedback.dropped_count}")


parser = argparse.ArgumentParser(description="Stream a window to a Simucube device screen.")
parser.add_argument("--window", required=True, help="part of the title of the window to capture")
parser.add_argument("--fps", type=float, default=30.0, help="target frame rate (default: 30)")
args = parser.parse_args()

set_dpi_awareness()

found = find_window(args.window)
if found is None:
    print(f"No visible window with a title containing {args.window!r}")
    raise SystemExit(1)
hwnd, window_title = found
print(f"Capturing window {window_title!r}")

with simucube_api.Api(
    control_flags=simucube_api.ControlFlag.control_telemetry,
    id_name="python_dash_example",
    user_info=simucube_api.ApiUserInformation(
        display_name="Python Dash Example", author="Simucube"
    ),
) as api:
    session = api.wait_for_session(timeout=10.0)

    # The device list is usually ready as soon as the session opens, and its
    # DeviceInfoChanged event is often delivered before this point. Read the current list
    # first and use the events only to wait for a device that is not there yet. The queue
    # exists before the list is read, so no update is lost in between.
    with api.events(timeout=5.0) as events:
        device_with_screen = find_screen_device(session.device_info)
        while device_with_screen is None:
            event = next(events, None)
            if event is None:
                break  # No update within the timeout, or the queue closed.
            match event:
                case simucube_api.DeviceInfoChanged(session=s):
                    device_with_screen = find_screen_device(s.device_info)

    if device_with_screen is None:
        print("No device with a screen found")
        raise SystemExit(1)

    screen = device_with_screen.screens[0]
    print(f"Streaming to device {device_with_screen.uid} at {screen.width}x{screen.height}")
    stream(session, device_with_screen.session_id, screen, hwnd, args.fps)

print("Done")
