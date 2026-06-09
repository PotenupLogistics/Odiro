from __future__ import annotations

from dataclasses import dataclass
import math
from typing import Any

from deliverybot_policy.actions import clamp, make_action, make_policy_candidate
from deliverybot_policy.context import (
    get_drive_spec,
    get_float_field,
    get_goal,
    get_motion_control_spec,
    get_policy_priority,
    get_robot_state,
    normalize_angle_degree,
)
from deliverybot_policy.pathfinding import (
    build_pathfinding_debug,
    build_path_points_debug,
    find_policy_astar_path,
    grid_index_to_world_location,
    world_to_grid_index,
)
from deliverybot_policy.policies.normal_path_follow import choose_lookahead_index


POLICY_ID = "dwa_local_avoidance"


@dataclass(frozen=True)
class DwaConfig:
    activation_distance_m: float = 4.0
    safety_distance_m: float = 0.45
    no_safe_stop_distance_m: float = 0.55
    stop_on_no_safe_trajectory: bool = False
    prediction_time_s: float = 1.4
    step_time_s: float = 0.2
    min_speed_kmh: float = 0.5
    max_speed_kmh: float = 3.0
    speed_sample_count: int = 4
    steering_sample_count: int = 11
    max_turn_rate_degree_s: float = 90.0
    target_weight: float = 1.2
    path_weight: float = 0.8
    clearance_weight: float = 0.8
    speed_weight: float = 0.2
    heading_weight: float = 0.6


def evaluate(context: dict[str, Any]) -> dict[str, Any] | None:
    grid_info = context.get("gridInfo", {})
    cell_lookup = context.get("gridCellLookup", {})
    robot_state = get_robot_state(context)
    goal = get_goal(context)

    if not isinstance(grid_info, dict) or not isinstance(cell_lookup, dict) or not robot_state or not goal:
        return None

    config = build_dwa_config(context)
    obstacles = build_obstacle_points_from_lidar(context, config.activation_distance_m)
    if not obstacles:
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

    path_result = find_policy_astar_path(
        grid_info,
        cell_lookup,
        start_index,
        goal_index,
        context.get("policyEntry", {}),
    )
    if not path_result.path:
        return None

    lookahead_index = choose_lookahead_index(grid_info, path_result.path, get_motion_control_spec(context))
    target_world = grid_index_to_world_location(grid_info, lookahead_index)
    path_world = [grid_index_to_world_location(grid_info, grid_index) for grid_index in path_result.path]

    candidates = sample_dwa_commands(config)
    best_command = select_best_command(robot_state, goal, target_world, path_world, obstacles, candidates, config)
    debug = build_pathfinding_debug(path_result)
    debug.update(build_path_points_debug(grid_info, path_result.path))
    debug.update(
        {
            "lookAheadGridX": lookahead_index[0],
            "lookAheadGridY": lookahead_index[1],
            "lookAheadWorldX": target_world["x"],
            "lookAheadWorldY": target_world["y"],
            "lookAheadWorldZ": target_world["z"],
            "dwaObstacleCount": len(obstacles),
            "dwaActivationDistanceM": config.activation_distance_m,
            "dwaSafetyDistanceM": config.safety_distance_m,
        }
    )

    if best_command is None:
        debug["dwaStatus"] = "no_safe_trajectory"
        debug.update(build_obstacle_proximity_debug(robot_state, obstacles, config))
        return None

    steering, speed_kmh, score, clearance_cm = best_command
    debug.update(
        {
            "dwaStatus": "ok",
            "dwaScore": score,
            "dwaClearanceCm": clearance_cm,
        }
    )

    return make_policy_candidate(
        POLICY_ID,
        make_action(steering, speed_kmh, direction="Forward"),
        "dwa_local_avoidance_selected",
        get_policy_priority(context, 25),
        debug,
    )


def build_dwa_config(context: dict[str, Any]) -> DwaConfig:
    policy_entry = context.get("policyEntry", {})
    safe_policy_entry = policy_entry if isinstance(policy_entry, dict) else {}
    parameters = safe_policy_entry.get("parameters", {})
    safe_parameters = parameters if isinstance(parameters, dict) else {}
    dwa_spec = safe_policy_entry.get("dwa", safe_parameters.get("dwa", {}))
    safe_dwa_spec = dwa_spec if isinstance(dwa_spec, dict) else {}

    drive_spec = get_drive_spec(context)
    motion_spec = get_motion_control_spec(context)

    default_max_speed_kmh = get_float_field(motion_spec, "targetSpeedKmh", 3.0)
    drive_max_speed_kmh = get_float_field(drive_spec, "maxSpeedKmh", default_max_speed_kmh)
    if drive_max_speed_kmh > 0.0:
        default_max_speed_kmh = min(default_max_speed_kmh, drive_max_speed_kmh)

    return DwaConfig(
        activation_distance_m=max(get_float_setting(safe_dwa_spec, "activationDistanceM", 4.0), 0.0),
        safety_distance_m=max(get_float_setting(safe_dwa_spec, "safetyDistanceM", 0.45), 0.0),
        no_safe_stop_distance_m=max(get_float_setting(safe_dwa_spec, "noSafeStopDistanceM", 0.55), 0.0),
        stop_on_no_safe_trajectory=get_bool_setting(safe_dwa_spec, "stopOnNoSafeTrajectory", False),
        prediction_time_s=max(get_float_setting(safe_dwa_spec, "predictionTimeS", 1.4), 0.1),
        step_time_s=max(get_float_setting(safe_dwa_spec, "stepTimeS", 0.2), 0.05),
        min_speed_kmh=max(get_float_setting(safe_dwa_spec, "minSpeedKmh", 0.5), 0.0),
        max_speed_kmh=max(get_float_setting(safe_dwa_spec, "maxSpeedKmh", default_max_speed_kmh), 0.0),
        speed_sample_count=max(get_int_setting(safe_dwa_spec, "speedSampleCount", 4), 1),
        steering_sample_count=max(get_int_setting(safe_dwa_spec, "steeringSampleCount", 11), 3),
        max_turn_rate_degree_s=max(get_float_setting(safe_dwa_spec, "maxTurnRateDegreeS", 90.0), 0.0),
        target_weight=max(get_float_setting(safe_dwa_spec, "targetWeight", 1.2), 0.0),
        path_weight=max(get_float_setting(safe_dwa_spec, "pathWeight", 0.8), 0.0),
        clearance_weight=max(get_float_setting(safe_dwa_spec, "clearanceWeight", 0.8), 0.0),
        speed_weight=max(get_float_setting(safe_dwa_spec, "speedWeight", 0.2), 0.0),
        heading_weight=max(get_float_setting(safe_dwa_spec, "headingWeight", 0.6), 0.0),
    )


def get_float_setting(source: dict[str, Any], field_name: str, default: float) -> float:
    try:
        return float(source.get(field_name, default))
    except (TypeError, ValueError):
        return default


def get_int_setting(source: dict[str, Any], field_name: str, default: int) -> int:
    try:
        return int(source.get(field_name, default))
    except (TypeError, ValueError):
        return default


def get_bool_setting(source: dict[str, Any], field_name: str, default: bool) -> bool:
    value = source.get(field_name, default)
    if isinstance(value, bool):
        return value
    if isinstance(value, str):
        return value.strip().lower() in {"1", "true", "yes", "y", "on"}

    return bool(value)


def build_obstacle_points_from_lidar(context: dict[str, Any], max_distance_m: float) -> list[tuple[float, float]]:
    observation = context.get("observation", {})
    safe_observation = observation if isinstance(observation, dict) else {}
    robot_state = get_robot_state(context)
    rays = safe_observation.get("lidarRays", [])
    if not isinstance(rays, list) or not robot_state:
        return []

    robot_x_cm = get_float_field(robot_state, "x")
    robot_y_cm = get_float_field(robot_state, "y")
    robot_yaw_degree = get_float_field(robot_state, "yawDegree")
    obstacle_points: list[tuple[float, float]] = []

    for ray in rays:
        if not isinstance(ray, dict) or not bool(ray.get("hit", False)):
            continue

        distance_m = get_float_field(ray, "distanceM", 0.0)
        if distance_m <= 0.0 or distance_m > max_distance_m:
            continue

        yaw_radian = math.radians(robot_yaw_degree + get_float_field(ray, "rayYawDegree"))
        distance_cm = distance_m * 100.0
        obstacle_points.append(
            (
                robot_x_cm + math.cos(yaw_radian) * distance_cm,
                robot_y_cm + math.sin(yaw_radian) * distance_cm,
            )
        )

    return obstacle_points


def sample_dwa_commands(config: DwaConfig) -> list[tuple[float, float]]:
    steering_count = config.steering_sample_count
    if steering_count % 2 == 0:
        steering_count += 1

    steering_values = [
        -1.0 + (2.0 * index / max(steering_count - 1, 1))
        for index in range(steering_count)
    ]

    if config.speed_sample_count <= 1:
        speed_values = [config.max_speed_kmh]
    else:
        speed_values = [
            config.min_speed_kmh
            + (config.max_speed_kmh - config.min_speed_kmh) * index / (config.speed_sample_count - 1)
            for index in range(config.speed_sample_count)
        ]

    return [(steering, max(speed_kmh, 0.0)) for steering in steering_values for speed_kmh in speed_values]


def select_best_command(
    robot_state: dict[str, Any],
    goal: dict[str, Any],
    target_world: dict[str, float],
    path_world: list[dict[str, float]],
    obstacles: list[tuple[float, float]],
    candidates: list[tuple[float, float]],
    config: DwaConfig,
) -> tuple[float, float, float, float] | None:
    best_command: tuple[float, float, float, float] | None = None

    for steering, speed_kmh in candidates:
        trajectory = simulate_trajectory(robot_state, steering, speed_kmh, config)
        score, clearance_cm = score_trajectory(trajectory, goal, target_world, path_world, obstacles, speed_kmh, config)
        if clearance_cm < config.safety_distance_m * 100.0:
            continue

        if best_command is None or score > best_command[2]:
            best_command = (steering, speed_kmh, score, clearance_cm)

    return best_command


def should_stop_on_no_safe_trajectory(
    robot_state: dict[str, Any],
    obstacles: list[tuple[float, float]],
    config: DwaConfig,
) -> bool:
    if config.stop_on_no_safe_trajectory:
        return True

    no_safe_stop_distance_cm = config.no_safe_stop_distance_m * 100.0
    safety_distance_cm = config.safety_distance_m * 100.0
    for forward_cm, lateral_cm, _ in iter_obstacles_in_robot_frame(robot_state, obstacles):
        if forward_cm < 0.0:
            continue
        if forward_cm <= no_safe_stop_distance_cm and abs(lateral_cm) <= safety_distance_cm:
            return True

    return False


def build_obstacle_proximity_debug(
    robot_state: dict[str, Any],
    obstacles: list[tuple[float, float]],
    config: DwaConfig,
) -> dict[str, Any]:
    closest_distance_cm = math.inf
    closest_ahead_distance_cm = math.inf
    closest_ahead_forward_cm = math.inf
    closest_ahead_lateral_cm = math.inf

    for forward_cm, lateral_cm, distance_cm in iter_obstacles_in_robot_frame(robot_state, obstacles):
        closest_distance_cm = min(closest_distance_cm, distance_cm)
        if forward_cm >= 0.0 and distance_cm < closest_ahead_distance_cm:
            closest_ahead_distance_cm = distance_cm
            closest_ahead_forward_cm = forward_cm
            closest_ahead_lateral_cm = lateral_cm

    return {
        "dwaClosestObstacleDistanceM": finite_cm_to_m_or_none(closest_distance_cm),
        "dwaClosestAheadObstacleDistanceM": finite_cm_to_m_or_none(closest_ahead_distance_cm),
        "dwaClosestAheadObstacleForwardM": finite_cm_to_m_or_none(closest_ahead_forward_cm),
        "dwaClosestAheadObstacleLateralM": finite_cm_to_m_or_none(closest_ahead_lateral_cm),
        "dwaNoSafeStopDistanceM": config.no_safe_stop_distance_m,
        "dwaStopOnNoSafeTrajectory": config.stop_on_no_safe_trajectory,
    }


def iter_obstacles_in_robot_frame(
    robot_state: dict[str, Any],
    obstacles: list[tuple[float, float]],
) -> list[tuple[float, float, float]]:
    robot_x_cm = get_float_field(robot_state, "x")
    robot_y_cm = get_float_field(robot_state, "y")
    yaw_radian = math.radians(get_float_field(robot_state, "yawDegree"))
    forward_x = math.cos(yaw_radian)
    forward_y = math.sin(yaw_radian)
    right_x = -math.sin(yaw_radian)
    right_y = math.cos(yaw_radian)
    relative_obstacles: list[tuple[float, float, float]] = []

    for obstacle_x_cm, obstacle_y_cm in obstacles:
        delta_x_cm = obstacle_x_cm - robot_x_cm
        delta_y_cm = obstacle_y_cm - robot_y_cm
        forward_cm = delta_x_cm * forward_x + delta_y_cm * forward_y
        lateral_cm = delta_x_cm * right_x + delta_y_cm * right_y
        distance_cm = math.hypot(delta_x_cm, delta_y_cm)
        relative_obstacles.append((forward_cm, lateral_cm, distance_cm))

    return relative_obstacles


def finite_cm_to_m_or_none(value_cm: float) -> float | None:
    if not math.isfinite(value_cm):
        return None

    return value_cm / 100.0


def simulate_trajectory(
    robot_state: dict[str, Any],
    steering: float,
    speed_kmh: float,
    config: DwaConfig,
) -> list[tuple[float, float, float]]:
    x_cm = get_float_field(robot_state, "x")
    y_cm = get_float_field(robot_state, "y")
    yaw_degree = get_float_field(robot_state, "yawDegree")
    speed_cm_s = speed_kmh * 100000.0 / 3600.0
    turn_rate_degree_s = steering * config.max_turn_rate_degree_s
    step_time_s = min(config.step_time_s, config.prediction_time_s)
    step_count = max(math.ceil(config.prediction_time_s / step_time_s), 1)

    trajectory: list[tuple[float, float, float]] = []
    for _ in range(step_count):
        yaw_degree = normalize_angle_degree(yaw_degree + turn_rate_degree_s * step_time_s)
        yaw_radian = math.radians(yaw_degree)
        x_cm += math.cos(yaw_radian) * speed_cm_s * step_time_s
        y_cm += math.sin(yaw_radian) * speed_cm_s * step_time_s
        trajectory.append((x_cm, y_cm, yaw_degree))

    return trajectory


def score_trajectory(
    trajectory: list[tuple[float, float, float]],
    goal: dict[str, Any],
    target_world: dict[str, float],
    path_world: list[dict[str, float]],
    obstacles: list[tuple[float, float]],
    speed_kmh: float,
    config: DwaConfig,
) -> tuple[float, float]:
    if not trajectory:
        return -math.inf, 0.0

    end_x, end_y, end_yaw_degree = trajectory[-1]
    target_distance_cm = math.hypot(target_world["x"] - end_x, target_world["y"] - end_y)
    path_distance_cm = get_distance_to_path_cm(end_x, end_y, path_world)
    clearance_cm = get_trajectory_clearance_cm(trajectory, obstacles)
    goal_heading_score = get_goal_heading_score(end_x, end_y, end_yaw_degree, goal)

    score = (
        -target_distance_cm * 0.01 * config.target_weight
        - path_distance_cm * 0.01 * config.path_weight
        + min(clearance_cm, config.activation_distance_m * 100.0) * 0.01 * config.clearance_weight
        + speed_kmh * config.speed_weight
        + goal_heading_score * config.heading_weight
    )
    return score, clearance_cm


def get_goal_heading_score(end_x: float, end_y: float, end_yaw_degree: float, goal: dict[str, Any]) -> float:
    goal_yaw_degree = math.degrees(
        math.atan2(
            get_float_field(goal, "y") - end_y,
            get_float_field(goal, "x") - end_x,
        )
    )
    yaw_error_degree = abs(normalize_angle_degree(goal_yaw_degree - end_yaw_degree))
    return 1.0 - clamp(yaw_error_degree / 180.0, 0.0, 1.0)


def get_trajectory_clearance_cm(
    trajectory: list[tuple[float, float, float]],
    obstacles: list[tuple[float, float]],
) -> float:
    if not obstacles:
        return math.inf

    closest_distance_cm = math.inf
    for x_cm, y_cm, _ in trajectory:
        for obstacle_x_cm, obstacle_y_cm in obstacles:
            closest_distance_cm = min(
                closest_distance_cm,
                math.hypot(obstacle_x_cm - x_cm, obstacle_y_cm - y_cm),
            )

    return closest_distance_cm


def get_distance_to_path_cm(x_cm: float, y_cm: float, path_world: list[dict[str, float]]) -> float:
    if not path_world:
        return 0.0
    if len(path_world) == 1:
        return math.hypot(path_world[0]["x"] - x_cm, path_world[0]["y"] - y_cm)

    return min(
        get_distance_to_segment_cm(x_cm, y_cm, start, end)
        for start, end in zip(path_world, path_world[1:])
    )


def get_distance_to_segment_cm(
    x_cm: float,
    y_cm: float,
    start: dict[str, float],
    end: dict[str, float],
) -> float:
    start_x = start["x"]
    start_y = start["y"]
    end_x = end["x"]
    end_y = end["y"]
    segment_x = end_x - start_x
    segment_y = end_y - start_y
    segment_length_sq = segment_x * segment_x + segment_y * segment_y

    if segment_length_sq <= 0.0:
        return math.hypot(x_cm - start_x, y_cm - start_y)

    alpha = clamp(((x_cm - start_x) * segment_x + (y_cm - start_y) * segment_y) / segment_length_sq, 0.0, 1.0)
    closest_x = start_x + segment_x * alpha
    closest_y = start_y + segment_y * alpha
    return math.hypot(x_cm - closest_x, y_cm - closest_y)
