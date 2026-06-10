from __future__ import annotations

from dataclasses import dataclass
from dataclasses import replace
import json
import math
from typing import Any

from deliverybot_policy.context import get_float_field, get_goal, get_robot_state, normalize_angle_degree
from deliverybot_policy.hybrid_astar import HybridAStarResult, build_hybrid_astar_options, find_hybrid_astar_path
from deliverybot_policy.pathfinding import (
    AStarResult,
    GridIndex,
    build_astar_options,
    build_path_points_debug,
    find_astar_path_result,
    grid_index_to_world_location,
    is_blocked_cell,
    world_to_grid_index,
)


PLANNER_ASTAR = "astar"
PLANNER_HYBRID_ASTAR = "hybrid_astar"
PLANNER_DWA = "dwa"
PLANNER_AUTO = "auto"
DEFAULT_MIN_LOOKAHEAD_DISTANCE_M = 1.5


@dataclass(frozen=True)
class PlannedPathResult:
    planner_id: str
    status: str
    grid_path: list[GridIndex]
    world_path: list[dict[str, float]]
    pose_path: list[dict[str, float | str]]
    expanded_nodes: int = 0
    path_cost: float = 0.0
    raw_pose_count: int = 0
    post_processed: bool = False
    path_cache_hit: bool = False


@dataclass(frozen=True)
class LookaheadTargetResult:
    target_world: dict[str, float]
    direction: str
    nearest_index: int
    target_index: int
    distance_to_path_cm: float


def normalize_planner_id(value: Any, default: str = PLANNER_AUTO) -> str:
    planner_id = str(value or "").strip().lower().replace("-", "_")
    if planner_id in {"grid_astar", "a_star", "a*"}:
        return PLANNER_ASTAR
    if planner_id in {"hybrid", "hybrid_a_star", "hybridastar"}:
        return PLANNER_HYBRID_ASTAR
    if planner_id in {PLANNER_ASTAR, PLANNER_HYBRID_ASTAR, PLANNER_DWA, PLANNER_AUTO}:
        return planner_id
    return default


def resolve_policy_planner_id(context: dict[str, Any], policy_entry: dict[str, Any]) -> str:
    policy_planner = get_nested_value(
        policy_entry,
        (
            ("planner",),
            ("plannerMode",),
            ("pathPlanner",),
            ("pathfinding", "planner"),
            ("pathfinding", "plannerMode"),
            ("parameters", "planner"),
            ("parameters", "plannerMode"),
        ),
    )
    if policy_planner is not None:
        return normalize_planner_id(policy_planner, PLANNER_AUTO)

    return normalize_planner_id(context.get("plannerMode", PLANNER_AUTO), PLANNER_AUTO)


def resolve_global_planner_id(context: dict[str, Any], policy_entry: dict[str, Any]) -> str:
    planner_id = resolve_policy_planner_id(context, policy_entry)
    if planner_id == PLANNER_AUTO:
        return PLANNER_ASTAR
    if planner_id == PLANNER_DWA:
        return normalize_planner_id(
            get_nested_value(
                policy_entry,
                (
                    ("globalPlanner",),
                    ("parameters", "globalPlanner"),
                    ("dwa", "globalPlanner"),
                    ("parameters", "dwa", "globalPlanner"),
                ),
            ),
            PLANNER_ASTAR,
        )
    return planner_id


def find_path_for_policy(context: dict[str, Any]) -> PlannedPathResult:
    policy_entry = context.get("policyEntry", {})
    safe_policy_entry = policy_entry if isinstance(policy_entry, dict) else {}
    planner_id = resolve_global_planner_id(context, safe_policy_entry)
    cache_key = build_path_cache_key(context, safe_policy_entry, planner_id)
    cached_result = get_cached_path_result(context, cache_key)
    if cached_result is not None:
        return replace(cached_result, path_cache_hit=True)

    if planner_id == PLANNER_HYBRID_ASTAR:
        result = find_hybrid_path_for_policy(context, safe_policy_entry)
        if result.status == "ok" or resolve_policy_planner_id(context, safe_policy_entry) == PLANNER_HYBRID_ASTAR:
            store_path_cache_result(context, cache_key, result)
            return result

    result = find_astar_path_for_policy(context, safe_policy_entry)
    store_path_cache_result(context, cache_key, result)
    return result


def find_astar_path_for_policy(context: dict[str, Any], policy_entry: dict[str, Any]) -> PlannedPathResult:
    grid_info = context.get("gridInfo", {})
    cell_lookup = context.get("gridCellLookup", {})
    robot_state = get_robot_state(context)
    goal = get_goal(context)

    if not isinstance(grid_info, dict) or not isinstance(cell_lookup, dict) or not robot_state or not goal:
        return PlannedPathResult(PLANNER_ASTAR, "missing_grid_robot_or_goal", [], [], [])

    start_index = world_to_grid_index(
        grid_info,
        get_float_field(robot_state, "x"),
        get_float_field(robot_state, "y"),
    )
    goal_index = world_to_grid_index(
        grid_info,
        get_float_field(goal, "x"),
        get_float_field(goal, "y"),
    )
    if start_index is None or goal_index is None:
        return PlannedPathResult(PLANNER_ASTAR, "outside_grid", [], [], [])

    astar_result: AStarResult = find_astar_path_result(
        grid_info,
        cell_lookup,
        start_index,
        goal_index,
        build_astar_options(get_policy_pathfinding_spec(policy_entry)),
    )
    world_path = [grid_index_to_world_location(grid_info, grid_index) for grid_index in astar_result.path]
    pose_path = [
        {
            "x": point["x"],
            "y": point["y"],
            "z": point["z"],
            "yawDegree": 0.0,
            "direction": "Forward",
        }
        for point in world_path
    ]
    return PlannedPathResult(
        PLANNER_ASTAR,
        astar_result.status,
        astar_result.path,
        world_path,
        pose_path,
        astar_result.expanded_nodes,
        astar_result.path_cost,
    )


def find_hybrid_path_for_policy(context: dict[str, Any], policy_entry: dict[str, Any]) -> PlannedPathResult:
    grid_info = context.get("gridInfo", {})
    cell_lookup = context.get("gridCellLookup", {})
    robot_state = get_robot_state(context)
    goal = get_goal(context)

    if not isinstance(grid_info, dict) or not isinstance(cell_lookup, dict) or not robot_state or not goal:
        return PlannedPathResult(PLANNER_HYBRID_ASTAR, "missing_grid_robot_or_goal", [], [], [])

    hybrid_spec = merge_hybrid_astar_runtime_spec(get_policy_hybrid_astar_spec(policy_entry), context)
    result: HybridAStarResult = find_hybrid_astar_path(
        grid_info,
        cell_lookup,
        {
            "x": get_float_field(robot_state, "x"),
            "y": get_float_field(robot_state, "y"),
            "yawDegree": get_float_field(robot_state, "yawDegree"),
        },
        goal,
        build_hybrid_astar_options(hybrid_spec),
    )
    world_path = [
        {
            "x": pose.x_cm,
            "y": pose.y_cm,
            "z": get_float_field(goal, "z"),
        }
        for pose in result.poses
    ]
    pose_path: list[dict[str, float | str]] = [
        {
            "x": pose.x_cm,
            "y": pose.y_cm,
            "z": get_float_field(goal, "z"),
            "yawDegree": pose.yaw_degree,
            "direction": pose.direction,
        }
        for pose in result.poses
    ]
    return PlannedPathResult(
        PLANNER_HYBRID_ASTAR,
        result.status,
        result.grid_path,
        world_path,
        pose_path,
        result.expanded_nodes,
        result.path_cost,
        result.raw_pose_count,
        result.post_processed,
    )


def build_planned_path_debug(
    grid_info: dict[str, Any],
    result: PlannedPathResult,
    max_points: int = 200,
) -> dict[str, Any]:
    debug: dict[str, Any] = {
        "planner": result.planner_id,
        "pathStatus": result.status,
        "pathLength": len(result.world_path),
        "pathCost": result.path_cost,
        "expandedNodeCount": result.expanded_nodes,
        "pathCacheHit": result.path_cache_hit,
    }
    if result.planner_id == PLANNER_HYBRID_ASTAR:
        debug["rawPathLength"] = result.raw_pose_count
        debug["postProcessedPath"] = result.post_processed

    if result.grid_path:
        debug.update(build_path_points_debug(grid_info, result.grid_path, max_points=max_points))
        return debug

    sampled_world_path = sample_world_path(result.world_path, max_points=max_points)
    debug["pathWorldPoints"] = sampled_world_path
    return debug


def build_path_cache_key(
    context: dict[str, Any],
    policy_entry: dict[str, Any],
    planner_id: str,
) -> tuple[Any, ...] | None:
    if not is_path_cache_enabled(policy_entry):
        return None

    grid_info = context.get("gridInfo", {})
    cell_lookup = context.get("gridCellLookup", {})
    robot_state = get_robot_state(context)
    goal = get_goal(context)
    if not isinstance(grid_info, dict) or not isinstance(cell_lookup, dict) or not robot_state or not goal:
        return None

    start_index = world_to_grid_index(
        grid_info,
        get_float_field(robot_state, "x"),
        get_float_field(robot_state, "y"),
    )
    goal_index = world_to_grid_index(
        grid_info,
        get_float_field(goal, "x"),
        get_float_field(goal, "y"),
    )
    if start_index is None or goal_index is None:
        return None

    yaw_bin_degree = get_path_cache_yaw_bin_degree(policy_entry)
    yaw_bin = round(normalize_angle_degree(get_float_field(robot_state, "yawDegree")) / yaw_bin_degree)
    return (
        str(policy_entry.get("policyId", "")),
        planner_id,
        start_index,
        goal_index,
        yaw_bin,
        get_blocked_cell_signature(cell_lookup),
        get_policy_settings_signature(policy_entry),
    )


def is_path_cache_enabled(policy_entry: dict[str, Any]) -> bool:
    value = get_nested_value(
        policy_entry,
        (
            ("pathCacheEnabled",),
            ("pathfinding", "pathCacheEnabled"),
            ("parameters", "pathCacheEnabled"),
            ("parameters", "pathfinding", "pathCacheEnabled"),
        ),
    )
    if value is None:
        return True
    if isinstance(value, bool):
        return value
    if isinstance(value, str):
        return value.strip().lower() in {"1", "true", "yes", "y", "on"}
    return bool(value)


def get_path_cache_yaw_bin_degree(policy_entry: dict[str, Any]) -> float:
    value = get_nested_value(
        policy_entry,
        (
            ("pathCacheYawBinDegree",),
            ("pathfinding", "pathCacheYawBinDegree"),
            ("parameters", "pathCacheYawBinDegree"),
            ("parameters", "pathfinding", "pathCacheYawBinDegree"),
        ),
    )
    try:
        return max(float(value), 1.0)
    except (TypeError, ValueError):
        return 15.0


def get_blocked_cell_signature(cell_lookup: dict[Any, Any]) -> tuple[int, int]:
    blocked_count = 0
    checksum = 2166136261
    for grid_index, cell in cell_lookup.items():
        if not isinstance(grid_index, tuple) or len(grid_index) != 2:
            continue
        if not isinstance(cell, dict) or not is_blocked_cell(cell):
            continue

        blocked_count += 1
        grid_x = int(grid_index[0])
        grid_y = int(grid_index[1])
        checksum ^= (grid_x * 73856093) ^ (grid_y * 19349663)
        checksum = (checksum * 16777619) & 0xFFFFFFFF

    return blocked_count, checksum


def get_policy_settings_signature(policy_entry: dict[str, Any]) -> str:
    settings = {
        "planner": resolve_policy_planner_id({}, policy_entry),
        "globalPlanner": get_nested_value(
            policy_entry,
            (
                ("globalPlanner",),
                ("parameters", "globalPlanner"),
                ("dwa", "globalPlanner"),
                ("parameters", "dwa", "globalPlanner"),
            ),
        ),
        "pathfinding": get_policy_pathfinding_spec(policy_entry),
        "hybridAStar": get_policy_hybrid_astar_spec(policy_entry),
    }
    return json.dumps(settings, sort_keys=True, separators=(",", ":"), default=str)


def get_cached_path_result(context: dict[str, Any], cache_key: tuple[Any, ...] | None) -> PlannedPathResult | None:
    if cache_key is None:
        return None

    cache = get_path_cache(context)
    cached_result = cache.get(cache_key)
    return cached_result if isinstance(cached_result, PlannedPathResult) else None


def store_path_cache_result(
    context: dict[str, Any],
    cache_key: tuple[Any, ...] | None,
    result: PlannedPathResult,
) -> None:
    if cache_key is None or result.status != "ok" or not result.world_path:
        return

    cache = get_path_cache(context)
    cache[cache_key] = replace(result, path_cache_hit=False)
    trim_path_cache(context, cache)


def get_path_cache(context: dict[str, Any]) -> dict[tuple[Any, ...], PlannedPathResult]:
    runtime_state = context.get("policyRuntimeState", {})
    if not isinstance(runtime_state, dict):
        return {}

    cache = runtime_state.setdefault("_pathCache", {})
    if not isinstance(cache, dict):
        cache = {}
        runtime_state["_pathCache"] = cache

    return cache


def trim_path_cache(context: dict[str, Any], cache: dict[tuple[Any, ...], PlannedPathResult]) -> None:
    max_entries = get_path_cache_max_entries(context)
    while len(cache) > max_entries:
        oldest_key = next(iter(cache))
        cache.pop(oldest_key, None)


def get_path_cache_max_entries(context: dict[str, Any]) -> int:
    policy_entry = context.get("policyEntry", {})
    safe_policy_entry = policy_entry if isinstance(policy_entry, dict) else {}
    value = get_nested_value(
        safe_policy_entry,
        (
            ("pathCacheMaxEntries",),
            ("pathfinding", "pathCacheMaxEntries"),
            ("parameters", "pathCacheMaxEntries"),
            ("parameters", "pathfinding", "pathCacheMaxEntries"),
        ),
    )
    try:
        return max(int(value), 1)
    except (TypeError, ValueError):
        return 32


def choose_lookahead_target(
    result: PlannedPathResult,
    robot_state: dict[str, Any],
    motion_spec: dict[str, Any],
) -> tuple[dict[str, float], str]:
    target_result = choose_lookahead_target_info(result, robot_state, motion_spec)
    return target_result.target_world, target_result.direction


def choose_lookahead_target_info(
    result: PlannedPathResult,
    robot_state: dict[str, Any],
    motion_spec: dict[str, Any],
) -> LookaheadTargetResult:
    if not result.world_path:
        return LookaheadTargetResult({
            "x": get_float_field(robot_state, "x"),
            "y": get_float_field(robot_state, "y"),
            "z": get_float_field(robot_state, "z"),
        }, "Forward", -1, -1, 0.0)

    lookahead_distance_cm = get_effective_lookahead_distance_cm(motion_spec)
    robot_x = get_float_field(robot_state, "x")
    robot_y = get_float_field(robot_state, "y")
    nearest_index = find_nearest_path_index(result.world_path, robot_x, robot_y)
    nearest_world = result.world_path[nearest_index]
    distance_to_path_cm = math.hypot(nearest_world["x"] - robot_x, nearest_world["y"] - robot_y)

    direction = get_path_motion_direction(result, nearest_index)
    if distance_to_path_cm >= lookahead_distance_cm:
        return LookaheadTargetResult(nearest_world, direction, nearest_index, nearest_index, distance_to_path_cm)

    cumulative_distance_cm = 0.0
    previous = nearest_world
    best_index = nearest_index

    for index in range(nearest_index + 1, len(result.world_path)):
        if get_pose_direction(result, index) != direction:
            break

        point = result.world_path[index]
        cumulative_distance_cm += math.hypot(point["x"] - previous["x"], point["y"] - previous["y"])
        best_index = index
        previous = point
        if cumulative_distance_cm >= lookahead_distance_cm:
            break

    target = result.world_path[min(best_index, len(result.world_path) - 1)]
    return LookaheadTargetResult(target, direction, nearest_index, best_index, distance_to_path_cm)


def find_nearest_path_index(path: list[dict[str, float]], robot_x: float, robot_y: float) -> int:
    if not path:
        return -1

    nearest_index = 0
    nearest_distance_sq = math.inf
    for index, point in enumerate(path):
        distance_sq = (point["x"] - robot_x) ** 2 + (point["y"] - robot_y) ** 2
        if distance_sq < nearest_distance_sq:
            nearest_index = index
            nearest_distance_sq = distance_sq

    return nearest_index


def get_effective_lookahead_distance_cm(motion_spec: dict[str, Any]) -> float:
    configured_lookahead_m = get_float_field(motion_spec, "lookAheadDistanceM", 1.0)
    minimum_lookahead_m = get_float_field(
        motion_spec,
        "minLookAheadDistanceM",
        DEFAULT_MIN_LOOKAHEAD_DISTANCE_M,
    )
    return max(configured_lookahead_m * 100.0, minimum_lookahead_m * 100.0, 1.0)


def build_steering_to_target(
    robot_state: dict[str, Any],
    target_world: dict[str, float],
    motion_spec: dict[str, Any],
    direction: str,
) -> tuple[float, float]:
    delta_x = target_world["x"] - get_float_field(robot_state, "x")
    delta_y = target_world["y"] - get_float_field(robot_state, "y")
    desired_yaw_degree = math.degrees(math.atan2(delta_y, delta_x))
    if direction == "Reverse":
        desired_yaw_degree = normalize_angle_degree(desired_yaw_degree + 180.0)

    yaw_error_degree = normalize_angle_degree(desired_yaw_degree - get_float_field(robot_state, "yawDegree"))
    steering_sensitivity = get_float_field(motion_spec, "steeringSensitivity", 0.8)
    steering = max(-1.0, min(1.0, (yaw_error_degree / 90.0) * steering_sensitivity))
    return steering, yaw_error_degree


def get_policy_pathfinding_spec(policy_entry: dict[str, Any]) -> dict[str, Any]:
    pathfinding_spec = policy_entry.get("pathfinding", {})
    if isinstance(pathfinding_spec, dict):
        return pathfinding_spec

    parameters = policy_entry.get("parameters", {})
    if isinstance(parameters, dict) and isinstance(parameters.get("pathfinding"), dict):
        return parameters["pathfinding"]

    return {}


def get_policy_hybrid_astar_spec(policy_entry: dict[str, Any]) -> dict[str, Any]:
    for source in (
        policy_entry,
        policy_entry.get("parameters", {}) if isinstance(policy_entry.get("parameters", {}), dict) else {},
    ):
        if not isinstance(source, dict):
            continue

        for field_name in ("hybridAStar", "hybrid_astar", "hybrid"):
            value = source.get(field_name)
            if isinstance(value, dict):
                return value

    return {}


def merge_hybrid_astar_runtime_spec(hybrid_spec: dict[str, Any], context: dict[str, Any]) -> dict[str, Any]:
    merged_spec = dict(hybrid_spec) if isinstance(hybrid_spec, dict) else {}
    vehicle_spec = get_runtime_vehicle_spec(context)
    if not vehicle_spec:
        return merged_spec

    if not has_any_key(merged_spec, ("minTurningRadiusCm", "min_turning_radius_cm")):
        min_turning_radius_cm = get_optional_float(vehicle_spec, ("minTurningRadiusCm", "min_turning_radius_cm"))
        if min_turning_radius_cm is not None and min_turning_radius_cm > 0.0:
            merged_spec["minTurningRadiusCm"] = min_turning_radius_cm

    if not should_use_vehicle_footprint(merged_spec):
        return merged_spec

    half_length_cm, half_width_cm = get_robot_box_half_extents_cm(vehicle_spec)
    if half_length_cm <= 0.0 or half_width_cm <= 0.0:
        return merged_spec

    if not has_any_key(merged_spec, ("footprintCheckEnabled", "footprint_check_enabled")):
        merged_spec["footprintCheckEnabled"] = True
    if not has_any_key(merged_spec, ("footprintHalfLengthCm", "footprint_half_length_cm")):
        merged_spec["footprintHalfLengthCm"] = half_length_cm
    if not has_any_key(merged_spec, ("footprintHalfWidthCm", "footprint_half_width_cm")):
        merged_spec["footprintHalfWidthCm"] = half_width_cm

    return merged_spec


def get_runtime_vehicle_spec(context: dict[str, Any]) -> dict[str, Any]:
    merged_spec: dict[str, Any] = {}
    observation = context.get("observation", {})
    config_info = context.get("configInfo", {})

    for source in (
        observation.get("vehicleSpec", {}) if isinstance(observation, dict) else {},
        config_info.get("vehicleSpec", {}) if isinstance(config_info, dict) else {},
        config_info.get("driveSpec", {}) if isinstance(config_info, dict) else {},
        config_info if isinstance(config_info, dict) else {},
    ):
        if isinstance(source, dict):
            merged_spec.update(source)

    return merged_spec


def should_use_vehicle_footprint(hybrid_spec: dict[str, Any]) -> bool:
    value = get_nested_value(
        hybrid_spec,
        (
            ("useVehicleFootprint",),
            ("use_vehicle_footprint",),
            ("footprintCheckEnabled",),
            ("footprint_check_enabled",),
        ),
    )
    if isinstance(value, bool):
        return value
    if isinstance(value, str):
        return value.strip().lower() in {"1", "true", "yes", "y", "on"}

    return bool(value)


def get_robot_box_half_extents_cm(vehicle_spec: dict[str, Any]) -> tuple[float, float]:
    half_length_cm = get_optional_float(
        vehicle_spec,
        (
            "footprintHalfLengthCm",
            "footprint_half_length_cm",
            "robotHalfLengthCm",
            "robot_half_length_cm",
            "halfLengthCm",
            "half_length_cm",
        ),
    )
    half_width_cm = get_optional_float(
        vehicle_spec,
        (
            "footprintHalfWidthCm",
            "footprint_half_width_cm",
            "robotHalfWidthCm",
            "robot_half_width_cm",
            "halfWidthCm",
            "half_width_cm",
        ),
    )
    if half_length_cm is not None and half_width_cm is not None:
        return max(half_length_cm, 0.0), max(half_width_cm, 0.0)

    for field_name in ("robotBoxExtentCm", "robot_box_extent_cm", "robotBoxExtent", "boxExtentCm"):
        value = vehicle_spec.get(field_name)
        if isinstance(value, dict):
            return max(get_float_field(value, "x"), 0.0), max(get_float_field(value, "y"), 0.0)
        if isinstance(value, (list, tuple)) and len(value) >= 2:
            try:
                return max(float(value[0]), 0.0), max(float(value[1]), 0.0)
            except (TypeError, ValueError):
                return 0.0, 0.0

    return 0.0, 0.0


def has_any_key(source: dict[str, Any], field_names: tuple[str, ...]) -> bool:
    return any(field_name in source for field_name in field_names)


def get_optional_float(source: dict[str, Any], field_names: tuple[str, ...]) -> float | None:
    for field_name in field_names:
        if field_name not in source:
            continue
        try:
            return float(source[field_name])
        except (TypeError, ValueError):
            return None

    return None


def get_nested_value(source: dict[str, Any], paths: tuple[tuple[str, ...], ...]) -> Any:
    for path in paths:
        current: Any = source
        for field_name in path:
            if not isinstance(current, dict) or field_name not in current:
                current = None
                break
            current = current[field_name]

        if current is not None:
            return current

    return None


def get_pose_direction(result: PlannedPathResult, index: int) -> str:
    if result.pose_path:
        pose_index = min(max(index, 0), len(result.pose_path) - 1)
        direction = str(result.pose_path[pose_index].get("direction", "Forward"))
        if direction in {"Forward", "Reverse"}:
            return direction

    return "Forward"


def get_initial_path_direction(result: PlannedPathResult) -> str:
    if not result.pose_path:
        return "Forward"

    for pose in result.pose_path[1:]:
        direction = str(pose.get("direction", "Forward"))
        if direction in {"Forward", "Reverse"}:
            return direction

    return get_pose_direction(result, 0)


def get_path_motion_direction(result: PlannedPathResult, path_index: int) -> str:
    if not result.world_path:
        return "Forward"

    if path_index < len(result.world_path) - 1:
        return get_pose_direction(result, path_index + 1)

    return get_pose_direction(result, path_index)


def sample_world_path(path: list[dict[str, float]], max_points: int = 200) -> list[dict[str, float]]:
    if len(path) <= max_points:
        return path

    safe_max_points = max(max_points, 2)
    sample_step = max(math.ceil(len(path) / safe_max_points), 1)
    sampled_path = path[::sample_step]
    if sampled_path[-1] != path[-1]:
        sampled_path.append(path[-1])

    return sampled_path
