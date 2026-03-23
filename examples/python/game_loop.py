"""Game-loop pattern with device remapping and relative force effects."""

import math

import numpy as np

import simucube_api


def find_brake_pedal(full_info):
    """Find first ActivePedal brake device."""
    return full_info.find_first(
        lambda d: (
            d.role == simucube_api.DeviceRole.brake_pedal
            and any(
                fb.type == simucube_api.FeedbackType.active_pedal for fb in d.feedbacks
            )
        )
    )


with simucube_api.Api(
    control_flags=simucube_api.ControlFlag.control_ffb_effects,
    id_name="python_game_loop",
    user_info=simucube_api.ApiUserInformation(
        display_name="Python Game Loop", author="Simucube"
    ),
) as api:
    queue = api.create_event_queue()

    session = None
    brake_id = None
    force_var = None
    pipeline = None
    variables = None
    start_ns = simucube_api.Clock.now_ns()
    update_ns = simucube_api.duration_ns_from_hz(1000)
    sample_offset_ns = 4_000_000  # 4ms
    frame_count = 0

    while True:
        # Process all pending events
        session_changed = False
        device_changed = False

        while (event := queue.try_pop()) is not None:
            match event:
                case simucube_api.SessionStateChanged(session=s, control_flags=flags):
                    if s and (flags & simucube_api.ControlFlag.control_ffb_effects):
                        if session is not s:
                            session = s
                            session_changed = True
                case simucube_api.DeviceInfoChanged():
                    device_changed = True

        if session_changed:
            print("Session changed -- resetting state")
            if pipeline:
                pipeline.stop()
                pipeline.remove()
                pipeline = None
            brake_id = None
            force_var = None
            device_changed = True

        # Remap devices if needed
        if device_changed and session:
            full_info = session.device_info
            brake_device = find_brake_pedal(full_info)

            if brake_device and brake_device.session_id != brake_id:
                # Brake pedal changed -- recreate pipeline
                if pipeline:
                    pipeline.stop()
                    pipeline.remove()
                brake_id = brake_device.session_id
                pipeline = simucube_api.FfbPipeline(session, brake_id)
                pipeline.configure(
                    simucube_api.PipelineConfig(
                        offset_type=simucube_api.OffsetType.force_relative
                    )
                )
                variables = session.variables
                force_var = variables.find("ap.pedal_face_force_N", device=brake_id)
                print(f"Brake pedal initialized: session_id={brake_id}")

        # Generate effect
        if pipeline and pipeline.is_active:
            cur_ns = simucube_api.Clock.now_ns()
            elapsed_s = (cur_ns - start_ns) / 1e9
            v = math.sin(elapsed_s * 20.0 * math.pi * 2)

            # 50% softer + 10% sine modulation
            offset = -0.5 + v * 0.1
            samples = np.array([offset, offset], dtype=np.float32)
            pipeline.generate_effect(cur_ns + sample_offset_ns, update_ns * 2, samples)

            frame_count += 1
            if frame_count % 1000 == 0 and force_var and variables:
                force = force_var.value
                print(f"Brake force: {force} N")

        # Wait for next update cycle
        target_ns = simucube_api.Clock.now_ns() + update_ns
        while simucube_api.Clock.now_ns() < target_ns:
            pass
