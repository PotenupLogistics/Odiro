from __future__ import annotations

from typing import Any


DELIVERY_BOT_SETUP_DEFAULTS: dict[str, dict[str, Any]] = {
    "drive": {
        "max_speed_kmh": 10.0,
        "slowdown_speed_range_kmh": 4.0,
        "speed_limit_tolerance_kmh": 0.5,
        "speed_limit_brake": 0.08,
        "stop_brake_input": 0.2,
        "throttle_input_rate_per_second": 0.35,
        "brake_input_rate_per_second": 0.5,
        "steering_input_rate_per_second": 3.0,
        "acceleration_rate_kmh_per_second": 2.0,
        "deceleration_rate_kmh_per_second": 3.0,
        "use_handbrake_when_brake": False,
        "max_torque": 220.0,
        "max_rpm": 4000.0,
        "engine_idle_rpm": 600.0,
        "engine_brake_effect": 0.04,
        "engine_rev_up_moi": 5.0,
        "engine_rev_down_rate": 600.0,
    },
    "path_follow": {
        "target_speed_kmh": 10.0,
        "look_ahead_distance_m": 1.0,
        "obstacle_slow_speed_kmh": 1.5,
        "draw_debug": True,
        "path_point_acceptance_distance_m": 0.4,
        "goal_acceptance_distance_m": 0.8,
        "steering_sensitivity": 0.8,
        "min_turn_speed_kmh": 1.5,
    },
    "lidar": {
        "scan_range_m": 5.0,
        "angle_step_degree": 2.0,
        "stop_distance_m": 1.2,
        "slow_down_distance_m": 3.5,
        "draw_debug": True,
        "sensor_height_m": 0.07,
        "front_half_angle_degree": 20.0,
        "store_missed_rays": False,
        "trace_channel": "visibility",
        "ignore_tags": ["NoCollision"],
    },
}


DELIVERY_BOT_SETUP_LIMITS: dict[str, dict[str, Any]] = {
    "drive.max_speed_kmh": {"minimum": 0},
    "drive.slowdown_speed_range_kmh": {"minimum": 0.1},
    "drive.speed_limit_brake": {"minimum": 0, "maximum": 1},
    "drive.stop_brake_input": {"minimum": 0, "maximum": 1},
    "path_follow.target_speed_kmh": {"minimum": 0},
    "path_follow.look_ahead_distance_m": {"minimum": 0.1},
    "path_follow.obstacle_slow_speed_kmh": {"minimum": 0},
    "lidar.scan_range_m": {"minimum": 0},
    "lidar.angle_step_degree": {"minimum": 1.0},
    "lidar.stop_distance_m": {"minimum": 0},
    "lidar.slow_down_distance_m": {"minimum": 0, "rule": ">= stop_distance_m + 0.1"},
    "lidar.front_half_angle_degree": {"minimum": 0, "maximum": 180},
}
