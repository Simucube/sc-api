"""SimData section property names for documentation and IDE use."""

VEHICLE_KEYS = [
    "engine_idle_rpm",
    "engine_redline_rpm",
    "shift_light_first_rpm",
    "shift_light_last_rpm",
    "shift_light_blink_rpm",
    "steering_wheel_rotation_deg",
    "gearbox_forward_gears",
    "gearbox_backward_gears",
    "non_unique",
    "has_drs",
    "has_abs",
    "has_tc",
    "name",
    "short_name",
    "model",
    "brand",
    "class_name",
]

TRACK_KEYS = [
    "sector_count",
    "track_length",
    "pitlane_speed_limit",
    "has_joker",
    "name",
    "base_name",
    "variant",
    "country",
    "track_style",
]

PARTICIPANT_KEYS = [
    "vehicle_number",
    "tire_id_lr",
    "tire_id_rr",
    "tire_id_lf",
    "tire_id_rf",
    "tire_id",
    "in_current_session",
    "on_track",
    "name",
    "abbrev_name",
    "team_name",
    "vehicle_id",
]

SIM_SESSION_KEYS = [
    "player_participant_id",
    "number_of_laps",
    "player_vehicle_id",
    "track_id",
    "session_type",
    "session_name",
]

TIRE_KEYS = [
    "hardness_order",
    "name",
    "short_name",
    "weather",
]

SIM_KEYS = [
    "process_detection",
    "max_rpm_available",
    "full_shift_light_data_available",
    "vehicle_detection_support",
    "name",
]
