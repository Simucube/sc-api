"""Stream the contents of a window to a Simucube device screen.

Grabs the client area of a window with dxcam, scales it to the screen of the
device and sends it as RGB565 frames with DashStreamer.

    pip install numpy opencv-python "dxcam>=0.3" pywin32
    python dash_stream.py --window "Assetto Corsa" --fps 30

Only the primary monitor is captured. The capture region is clamped to it, so a
window on a second monitor gives a cut or empty image.
"""

from __future__ import annotations

import argparse
import ctypes
import time

import cv2
import dxcam
import win32api
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


def client_region(hwnd: int) -> tuple[int, int, int, int] | None:
    """Return the client area of the window as a dxcam region, or None if it is unusable."""
    try:
        left, top, right, bottom = win32gui.GetClientRect(hwnd)
        left, top = win32gui.ClientToScreen(hwnd, (left, top))
        right, bottom = win32gui.ClientToScreen(hwnd, (right, bottom))
    except win32gui.error:
        return None  # The window is gone.

    # dxcam raises for a region that reaches outside the primary display.
    screen_w = win32api.GetSystemMetrics(0)
    screen_h = win32api.GetSystemMetrics(1)
    left, right = max(0, min(left, screen_w)), max(0, min(right, screen_w))
    top, bottom = max(0, min(top, screen_h)), max(0, min(bottom, screen_h))
    if right - left < 1 or bottom - top < 1:
        return None
    return left, top, right, bottom


def stream(session, device_id, screen, hwnd: int, fps: float) -> None:
    camera = dxcam.create(output_color="BGR")
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

                region = client_region(hwnd)
                if region is None:
                    print("Window closed or has no visible client area")
                    break

                frame = None if win32gui.IsIconic(hwnd) else camera.grab(region)
                if frame is not None:
                    resized = cv2.resize(frame, (screen.width, screen.height), interpolation=cv2.INTER_AREA)
                    # C-contiguous uint8 of shape (height, width, 2): packed RGB565.
                    last_frame = cv2.cvtColor(resized, cv2.COLOR_BGR2BGR565)
                elif last_frame is None:
                    time.sleep(0.001)  # Nothing captured yet.
                    continue
                # grab() returns None while the screen is unchanged, and a minimized
                # window is not grabbed at all. Re-send the last frame in both cases:
                # the device leaves streaming after about 3 s without a frame.

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
            del camera
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

    # Wait for a device that has a screen
    device_with_screen = None
    for event in api.events(timeout=5.0):
        if event is None:
            break
        match event:
            case simucube_api.DeviceInfoChanged(session=s):
                device_with_screen = s.device_info.find_first(lambda d: len(d.screens) > 0)
                if device_with_screen:
                    break

    if not device_with_screen:
        print("No device with a screen found")
        raise SystemExit(1)

    screen = device_with_screen.screens[0]
    print(f"Streaming to device {device_with_screen.uid} at {screen.width}x{screen.height}")
    stream(session, device_with_screen.session_id, screen, hwnd, args.fps)

print("Done")
