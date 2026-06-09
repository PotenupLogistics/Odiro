from __future__ import annotations

import math
from typing import Any

from deliverybot_policy.context import get_float_field, get_goal, get_robot_state, normalize_angle_degree
from deliverybot_policy.pathfinding import GridIndex, grid_index_to_world_location, world_to_grid_index


def get_nested_spec(source: dict[str, Any], field_names: tuple[str, ...]) -> dict[str, Any]:
    for field_name in field_names:
        value = source.get(field_name, {})
        if isinstance(value, dict):
            return value

    return {}


def get_dynamic_obstacle_spec(policy_entry: dict[str, Any]) -> dict[str, Any]:
    if not isinstance(policy_entry, dict):
        return {}

    direct_spec = get_nested_spec(policy_entry, ("dynamicObstacles", "dynamic_obstacles"))
    if direct_spec:
        return direct_spec

    parameters = policy_entry.get("parameters", {})
    if isinstance(parameters, dict):
        return get_nested_spec(parameters, ("dynamicObstacles", "dynamic_obstacles"))

    return {}


def get_bool_setting(source: dict[str, Any], names: tuple[str, ...], default: bool) -> bool:
    for name in names:
        if name not in source:
            continue

        value = source[name]
        if isinstance(value, bool):
            return value
        if isinstance(value, str):
            return value.strip().lower() in {"1", "true", "yes", "y", "on"}

        return bool(value)

    return default


def get_float_setting(source: dict[str, Any], names: tuple[str, ...], default: float) -> float:
    for name in names:
        if name not in source:
            continue

        try:
            return float(source[name])
        except (TypeError, ValueError):
            return default

    return default


def is_dynamic_obstacle_avoidance_enabled(policy_entry: dict[str, Any]) -> bool:
    spec = get_dynamic_obstacle_spec(policy_entry)
    return get_bool_setting(spec, ("enabled", "bEnabled"), True)


def build_dynamic_obstacle_grid_overlay(
    context: dict[str, Any],
    max_distance_m: float,
    protected_indexes: set[GridIndex] | None = None,
) -> tuple[dict[GridIndex, dict[str, Any]], dict[str, Any]]:
    grid_info = context.get("gridInfo", {})
    cell_lookup = context.get("gridCellLookup", {})
    observation = context.get("observation", {})
    policy_entry = context.get("policyEntry", {})

    if not isinstance(grid_info, dict) or not isinstance(cell_lookup, dict) or not isinstance(observation, dict):
        return {}, {"dynamicObstacleStatus": "grid_or_observation_missing"}

    if not is_dynamic_obstacle_avoidance_enabled(policy_entry):
        return cell_lookup, {"dynamicObstacleStatus": "disabled"}

    robot_state = get_robot_state(context)
    if not robot_state:
        return cell_lookup, {"dynamicObstacleStatus": "robot_state_missing"}

    dynamic_spec = get_dynamic_obstacle_spec(policy_entry)
    lidar_spec = context.get("configInfo", {}).get("lidarSpec", {}) if isinstance(context.get("configInfo", {}), dict) else {}
    safe_lidar_spec = lidar_spec if isinstance(lidar_spec, dict) else {}

    inflation_radius_m = max(
        get_float_setting(dynamic_spec, ("inflationRadiusM", "inflation_radius_m"), 0.9),
        0.0,
    )
    front_only = get_bool_setting(dynamic_spec, ("frontOnly", "front_only"), True)
    front_half_angle_degree = max(get_float_field(safe_lidar_spec, "frontHalfAngleDegree", 45.0), 0.0)
    configured_max_distance_m = get_float_setting(
        dynamic_spec,
        ("maxDistanceM", "max_distance_m"),
        max_distance_m,
    )
    effective_max_distance_m = max(min(configured_max_distance_m, max_distance_m), 0.0)

    lidar_rays = observation.get("lidarRays", [])
    if not isinstance(lidar_rays, list):
        return cell_lookup, {"dynamicObstacleStatus": "lidar_rays_missing"}

    overlay_lookup: dict[GridIndex, dict[str, Any]] = dict(cell_lookup)
    blocked_indexes: set[GridIndex] = set()
    hit_ray_count = 0
    protected = protected_indexes or set()

    for lidar_ray in lidar_rays:
        if not isinstance(lidar_ray, dict) or not bool(lidar_ray.get("hit", False)):
            continue

        distance_m = get_float_field(lidar_ray, "distanceM", 0.0)
        if distance_m <= 0.0 or distance_m > effective_max_distance_m:
            continue

        relative_yaw_degree = get_float_field(lidar_ray, "rayYawDegree", 0.0)
        if front_only and abs(normalize_angle_degree(relative_yaw_degree)) > front_half_angle_degree:
            continue

        hit_index = get_lidar_hit_grid_index(grid_info, robot_state, relative_yaw_degree, distance_m)
        if hit_index is None:
            continue

        hit_ray_count += 1
        blocked_indexes.update(
            iter_inflated_obstacle_indexes(
                grid_info,
                hit_index,
                inflation_radius_m * 100.0,
                protected,
            )
        )

    for blocked_index in blocked_indexes:
        source_cell = overlay_lookup.get(blocked_index)
        if not isinstance(source_cell, dict):
            continue

        blocked_cell = dict(source_cell)
        blocked_cell["blocked"] = True
        blocked_cell["areaType"] = "Blocked"
        blocked_cell["cost"] = 1.0e30
        blocked_cell["sourceCollisionProfile"] = "DynamicObstacle"
        overlay_lookup[blocked_index] = blocked_cell

    return overlay_lookup, {
        "dynamicObstacleStatus": "ok",
        "dynamicObstacleHitRayCount": hit_ray_count,
        "dynamicObstacleBlockedCellCount": len(blocked_indexes),
        "dynamicObstacleInflationRadiusM": inflation_radius_m,
        "dynamicObstacleMaxDistanceM": effective_max_distance_m,
        "dynamicObstacleFrontOnly": front_only,
    }


def build_dynamic_obstacle_reroute_context(context: dict[str, Any], max_distance_m: float) -> dict[str, Any]:
    reroute_context = dict(context)
    protected_indexes = get_protected_navigation_indexes(context)
    overlay_lookup, dynamic_debug = build_dynamic_obstacle_grid_overlay(
        context,
        max_distance_m,
        protected_indexes,
    )

    if overlay_lookup:
        reroute_context["gridCellLookup"] = overlay_lookup

    reroute_context["dynamicObstacleDebug"] = dynamic_debug
    return reroute_context


def get_protected_navigation_indexes(context: dict[str, Any]) -> set[GridIndex]:
    grid_info = context.get("gridInfo", {})
    if not isinstance(grid_info, dict):
        return set()

    protected_indexes: set[GridIndex] = set()
    robot_state = get_robot_state(context)
    goal = get_goal(context)

    if robot_state:
        robot_index = world_to_grid_index(
            grid_info,
            get_float_field(robot_state, "x"),
            get_float_field(robot_state, "y"),
        )
        if robot_index is not None:
            protected_indexes.add(robot_index)

    if goal:
        goal_index = world_to_grid_index(
            grid_info,
            get_float_field(goal, "x"),
            get_float_field(goal, "y"),
        )
        if goal_index is not None:
            protected_indexes.add(goal_index)

    return protected_indexes


def get_lidar_hit_grid_index(
    grid_info: dict[str, Any],
    robot_state: dict[str, Any],
    relative_yaw_degree: float,
    distance_m: float,
) -> GridIndex | None:
    absolute_yaw_radian = math.radians(get_float_field(robot_state, "yawDegree") + relative_yaw_degree)
    hit_x_cm = get_float_field(robot_state, "x") + math.cos(absolute_yaw_radian) * distance_m * 100.0
    hit_y_cm = get_float_field(robot_state, "y") + math.sin(absolute_yaw_radian) * distance_m * 100.0
    return world_to_grid_index(grid_info, hit_x_cm, hit_y_cm)


def iter_inflated_obstacle_indexes(
    grid_info: dict[str, Any],
    center_index: GridIndex,
    inflation_radius_cm: float,
    protected_indexes: set[GridIndex],
) -> set[GridIndex]:
    cell_size_cm = max(get_float_field(grid_info, "cellSizeCm", 100.0), 1.0)
    radius_cell_count = max(math.ceil(inflation_radius_cm / cell_size_cm), 0)
    blocked_indexes: set[GridIndex] = set()
    center_world = grid_index_to_world_location(grid_info, center_index)

    for offset_x in range(-radius_cell_count, radius_cell_count + 1):
        for offset_y in range(-radius_cell_count, radius_cell_count + 1):
            candidate_index = (center_index[0] + offset_x, center_index[1] + offset_y)
            if candidate_index in protected_indexes:
                continue

            candidate_world = grid_index_to_world_location(grid_info, candidate_index)
            distance_cm = math.hypot(
                candidate_world["x"] - center_world["x"],
                candidate_world["y"] - center_world["y"],
            )
            if distance_cm <= inflation_radius_cm + cell_size_cm * 0.5:
                blocked_indexes.add(candidate_index)

    return blocked_indexes
