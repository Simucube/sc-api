"""Generate sine-wave force effects on connected ActivePedals."""

import math

import numpy as np

import simucube_api

FORCE_N = 2.0
FREQ_HZ = 20.0
SAMPLE_TIME_OFFSET_NS = 5_000_000  # 5ms
SAMPLE_LENGTH_NS = 10_000_000  # 10ms

with simucube_api.Api(
    control_flags=simucube_api.ControlFlag.control_ffb_effects,
    id_name="python_ffb_example",
    user_info=simucube_api.ApiUserInformation(
        display_name="Python FFB Example", author="Simucube"
    ),
) as api:
    session = api.wait_for_session(timeout=10.0)
    print("Session connected, waiting for devices...")

    # Wait for devices with active_pedal feedback
    brake_id = None
    throttle_id = None
    for event in api.events(timeout=5.0):
        if event is None:
            break
        match event:
            case simucube_api.DeviceInfoChanged(session=s):
                full_info = s.device_info
                for device in full_info:
                    if any(
                        fb.type == simucube_api.FeedbackType.active_pedal
                        for fb in device.feedbacks
                    ):
                        if device.role == simucube_api.DeviceRole.brake_pedal:
                            brake_id = device.session_id
                        elif device.role == simucube_api.DeviceRole.throttle_pedal:
                            throttle_id = device.session_id

    if not brake_id and not throttle_id:
        print("No ActivePedals found within timeout")
        raise SystemExit(1)

    print(f"Found pedals -- brake={brake_id}, throttle={throttle_id}")

    pipelines = []
    config = simucube_api.PipelineConfig(offset_type=simucube_api.OffsetType.force_N)

    for device_id in [brake_id, throttle_id]:
        if device_id:
            pipeline = simucube_api.FfbPipeline(session, device_id)
            pipeline.configure(config)
            pipelines.append(pipeline)

    queue = api.create_event_queue()
    try:
        start_ns = simucube_api.Clock.now_ns()
        update_ns = simucube_api.duration_ns_from_hz(1000)  # 1kHz

        while True:
            # Drain events
            while (ev := queue.try_pop()) is not None:
                if isinstance(ev, simucube_api.SessionStateChanged):
                    if ev.state != simucube_api.SessionState.connected_control:
                        print("Session disconnected")
                        raise SystemExit(0)

            cur_ns = simucube_api.Clock.now_ns()
            elapsed_s = (cur_ns - start_ns) / 1e9
            v = math.sin(elapsed_s * FREQ_HZ * math.pi * 2)
            samples = np.array([v * FORCE_N, v * FORCE_N], dtype=np.float32)

            for p in pipelines:
                p.generate_effect(cur_ns + SAMPLE_TIME_OFFSET_NS, SAMPLE_LENGTH_NS, samples)

            # Busy-wait for 1ms update rate
            while simucube_api.Clock.now_ns() < cur_ns + update_ns:
                pass
    finally:
        for p in pipelines:
            p.stop()
            p.remove()
