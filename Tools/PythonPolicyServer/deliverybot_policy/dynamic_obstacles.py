from __future__ import annotations

import math
from typing import Any

from deliverybot_policy.context import get_float_field, get_goal, get_robot_state, normalize_angle_degree
from deliverybot_policy.pathfinding import GridIndex, grid_index_to_world_location, world_to_grid_index


DYNAMIC_OBSTACLE_STATE_KEY = "dynamicObstacles"
DEFAULT_PERSISTENCE_SECONDS = 1.5


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
    persistence_seconds = max(
        get_float_setting(
            dynamic_spec,
            ("persistenceSeconds", "persistence_seconds", "ttlSeconds", "ttl_seconds"),
            DEFAULT_PERSISTENCE_SECONDS,
        ),
        0.0,
    )

    lidar_rays = observation.get("lidarRays", [])
    if not isinstance(lidar_rays, list):
        return cell_lookup, {"dynamicObstacleStatus": "lidar_rays_missing"}

    overlay_lookup: dict[GridIndex, dict[str, Any]] = dict(cell_lookup)
    blocked_indexes: set[GridIndex] = set()
    observed_hit_indexes: set[GridIndex] = set()
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

        hit_index = get_dynamic_obstacle_center_index(
            grid_info,
            robot_state,
            relative_yaw_degree,
            distance_m,
            protected,
        )
        if hit_index is None:
            continue

        hit_ray_count += 1
        observed_hit_indexes.add(hit_index)

    active_obstacle_indexes = update_dynamic_obstacle_memory(
        context,
        observed_hit_indexes,
        persistence_seconds,
    )

    for obstacle_index in active_obstacle_indexes:
        blocked_indexes.update(
            iter_inflated_obstacle_indexes(
                grid_info,
                obstacle_index,
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
        "dynamicObstacleObservedCellCount": len(observed_hit_indexes),
        "dynamicObstacleMemoryCellCount": len(active_obstacle_indexes),
        "dynamicObstacleBlockedCellCount": len(blocked_indexes),
        "dynamicObstacleInflationRadiusM": inflation_radius_m,
        "dynamicObstacleMaxDistanceM": effective_max_distance_m,
        "dynamicObstacleFrontOnly": front_only,
        "dynamicObstaclePersistenceSeconds": persistence_seconds,
    }


def get_dynamic_obstacle_center_index(
    grid_info: dict[str, Any],
    robot_state: dict[str, Any],
    relative_yaw_degree: float,
    distance_m: float,
    protected_indexes: set[GridIndex],
) -> GridIndex | None:
    hit_index = get_lidar_hit_grid_index(grid_info, robot_state, relative_yaw_degree, distance_m)
    if hit_index is None or hit_index not in protected_indexes:
        return hit_index

    cell_size_m = max(get_float_field(grid_info, "cellSizeCm", 100.0), 1.0) / 100.0
    projected_index = get_lidar_hit_grid_index(
        grid_info,
        robot_state,
        relative_yaw_degree,
        distance_m + cell_size_m,
    )
    if projected_index is None or projected_index in protected_indexes:
        return None

    return projected_index


def update_dynamic_obstacle_memory(
    context: dict[str, Any],
    observed_indexes: set[GridIndex],
    persistence_seconds: float,
) -> set[GridIndex]:
    if persistence_seconds <= 0.0:
        clear_dynamic_obstacle_memory(context)
        return set(observed_indexes)

    obstacle_state = get_dynamic_obstacle_state(context)
    if obstacle_state is None:
        return set(observed_indexes)

    current_time_seconds = get_observation_time_seconds(context)
    cells = obstacle_state.setdefault("cells", {})
    if not isinstance(cells, dict):
        cells = {}
        obstacle_state["cells"] = cells

    for observed_index in observed_indexes:
        cells[serialize_grid_index(observed_index)] = current_time_seconds

    active_indexes: set[GridIndex] = set()
    expired_keys: list[str] = []
    for key, last_seen_value in cells.items():
        try:
            last_seen_seconds = float(last_seen_value)
        except (TypeError, ValueError):
            expired_keys.append(str(key))
            continue

        if current_time_seconds - last_seen_seconds <= persistence_seconds:
            parsed_index = parse_grid_index(str(key))
            if parsed_index is not None:
                active_indexes.add(parsed_index)
        else:
            expired_keys.append(str(key))

    for expired_key in expired_keys:
        cells.pop(expired_key, None)

    obstacle_state["lastUpdateTimeSeconds"] = current_time_seconds
    return active_indexes


def clear_dynamic_obstacle_memory(context: dict[str, Any]) -> None:
    policy_runtime_state = context.get("policyRuntimeState", {})
    if not isinstance(policy_runtime_state, dict):
        return

    policy_runtime_state.pop(DYNAMIC_OBSTACLE_STATE_KEY, None)


def get_dynamic_obstacle_state(context: dict[str, Any]) -> dict[str, Any] | None:
    policy_runtime_state = context.get("policyRuntimeState", {})
    if not isinstance(policy_runtime_state, dict):
        return None

    state = policy_runtime_state.setdefault(DYNAMIC_OBSTACLE_STATE_KEY, {})
    if not isinstance(state, dict):
        state = {}
        policy_runtime_state[DYNAMIC_OBSTACLE_STATE_KEY] = state

    return state


def get_observation_time_seconds(context: dict[str, Any]) -> float:
    observation = context.get("observation", {})
    safe_observation = observation if isinstance(observation, dict) else {}
    return get_float_field(safe_observation, "worldTimeSeconds", 0.0)


def serialize_grid_index(grid_index: GridIndex) -> str:
    return f"{grid_index[0]},{grid_index[1]}"


def parse_grid_index(value: str) -> GridIndex | None:
    parts = value.split(",", 1)
    if len(parts) != 2:
        return None

    try:
        return int(parts[0]), int(parts[1])
    except ValueError:
        return None


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
