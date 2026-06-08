from __future__ import annotations

import math
from typing import Any


def get_nested_object(source: dict[str, Any], field_name: str) -> dict[str, Any]:
    value = source.get(field_name, {})
    return value if isinstance(value, dict) else {}


def get_float_field(source: dict[str, Any], field_name: str, default: float = 0.0) -> float:
    return float(source.get(field_name, default) or default)


def get_int_field(source: dict[str, Any], field_name: str, default: int = 0) -> int:
    return int(source.get(field_name, default) or default)


def get_drive_spec(context: dict[str, Any]) -> dict[str, Any]:
    config_info = context.get("configInfo", {})
    if not isinstance(config_info, dict):
        return {}

    drive_spec = config_info.get("driveSpec", {})
    if isinstance(drive_spec, dict):
        return drive_spec

    vehicle_spec = config_info.get("vehicleSpec", {})
    return vehicle_spec if isinstance(vehicle_spec, dict) else {}


def get_lidar_spec(context: dict[str, Any]) -> dict[str, Any]:
    config_info = context.get("configInfo", {})
    if not isinstance(config_info, dict):
        return {}

    lidar_spec = config_info.get("lidarSpec", {})
    return lidar_spec if isinstance(lidar_spec, dict) else {}


def get_motion_control_spec(context: dict[str, Any]) -> dict[str, Any]:
    config_info = context.get("configInfo", {})
    if not isinstance(config_info, dict):
        return {}

    motion_spec = config_info.get("motionControlSpec", {})
    return motion_spec if isinstance(motion_spec, dict) else {}


def get_robot_state(context: dict[str, Any]) -> dict[str, Any]:
    observation = context.get("observation", {})
    if not isinstance(observation, dict):
        return {}

    return get_nested_object(observation, "robotState")


def get_goal(context: dict[str, Any]) -> dict[str, Any]:
    episode_info = context.get("episodeInfo", {})
    if not isinstance(episode_info, dict) or not bool(episode_info.get("hasGoal", False)):
        return {}

    return get_nested_object(episode_info, "goal")


def normalize_angle_degree(angle_degree: float) -> float:
    return (angle_degree + 180.0) % 360.0 - 180.0


def distance_2d_cm(first: dict[str, Any], second: dict[str, Any]) -> float:
    return math.hypot(
        get_float_field(second, "x") - get_float_field(first, "x"),
        get_float_field(second, "y") - get_float_field(first, "y"),
    )


def get_nearest_observed_object(
    observation: dict[str, Any],
    require_in_front: bool = False,
) -> dict[str, Any] | None:
    observed_objects = observation.get("observedObjects", [])
    if not isinstance(observed_objects, list):
        return None

    nearest_object: dict[str, Any] | None = None
    nearest_distance_m = math.inf

    for observed_object in observed_objects:
        if not isinstance(observed_object, dict):
            continue

        if require_in_front and not bool(observed_object.get("inFront", False)):
            continue

        try:
            distance_m = float(observed_object.get("closestDistanceM", math.inf) or math.inf)
        except (TypeError, ValueError):
            distance_m = math.inf

        if distance_m < nearest_distance_m:
            nearest_object = observed_object
            nearest_distance_m = distance_m

    return nearest_object


def get_policy_priority(context: dict[str, Any], default_priority: int = 100) -> int:
    policy_entry = context.get("policyEntry", {})
    if not isinstance(policy_entry, dict):
        return default_priority

    return int(policy_entry.get("priority", default_priority) or default_priority)
