from __future__ import annotations

from dataclasses import dataclass, replace
import math
from typing import Any

from deliverybot_policy.actions import clamp, make_action, make_policy_candidate, make_stop_action
from deliverybot_policy.context import (
    get_drive_spec,
    get_float_field,
    get_goal,
    get_motion_control_spec,
    get_policy_priority,
    get_robot_state,
    normalize_angle_degree,
)
from deliverybot_policy.planning import (
    build_planned_path_debug,
    choose_lookahead_target_info,
    find_path_for_policy,
    world_to_grid_index,
)
from deliverybot_policy.pathfinding import grid_index_to_world_location, is_blocked_cell


POLICY_ID = "dwa_local_avoidance"


@dataclass(frozen=True)
class DwaConfig:
    activation_distance_m: float = 4.0
    path_lookahead_distance_m: float = 2.0
    safety_distance_m: float = 0.45
    robot_collision_radius_m: float = 0.9
    clearance_margin_m: float = 0.15
    obstacle_inflation_m: float = 0.1
    no_safe_stop_distance_m: float = 0.55
    stop_on_no_safe_trajectory: bool = False
    prediction_time_s: float = 1.4
    step_time_s: float = 0.2
    min_speed_kmh: float = 0.5
    max_speed_kmh: float = 3.0
    allow_reverse: bool = True
    max_reverse_speed_kmh: float = 1.5
    speed_sample_count: int = 4
    reverse_speed_sample_count: int = 3
    steering_sample_count: int = 11
    max_turn_rate_degree_s: float = 90.0
    target_weight: float = 1.2
    path_weight: float = 0.8
    clearance_weight: float = 0.8
    speed_weight: float = 0.2
    heading_weight: float = 0.6
    reverse_score_penalty: float = 0.6
    include_grid_obstacles: bool = True
    grid_obstacle_max_distance_m: float = 3.5
    forward_corridor_half_width_m: float = 0.9
    min_avoidance_steering: float = 0.25
    avoidance_steering_penalty: float = 2.0
    stuck_detection_enabled: bool = True
    stuck_speed_threshold_kmh: float = 0.2
    stuck_progress_distance_m: float = 0.15
    stuck_time_seconds: float = 0.8
    stuck_close_obstacle_distance_m: float = 1.2
    stuck_recovery_reverse_duration_s: float = 0.9
    stuck_recovery_reverse_speed_kmh: float = 1.0
    stuck_recovery_min_clearance_gain_m: float = 0.1


def evaluate(context: dict[str, Any]) -> dict[str, Any] | None:
    grid_info = context.get("gridInfo", {})
    cell_lookup = context.get("gridCellLookup", {})
    robot_state = get_robot_state(context)
    goal = get_goal(context)

    if not isinstance(grid_info, dict) or not isinstance(cell_lookup, dict) or not robot_state or not goal:
        return None

    config = build_dwa_config(context)
    lidar_obstacles = build_obstacle_points_from_lidar(context, config.activation_distance_m)
    grid_obstacles = (
        build_obstacle_points_from_grid(grid_info, cell_lookup, robot_state, config.grid_obstacle_max_distance_m)
        if config.include_grid_obstacles
        else []
    )
    obstacles = dedupe_obstacle_points([*lidar_obstacles, *grid_obstacles])
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

    path_result = find_path_for_policy(context)
    if not path_result.world_path:
        return None

    motion_spec = build_dwa_motion_spec(context, config)
    lookahead_result = choose_lookahead_target_info(path_result, robot_state, motion_spec)
    target_world = lookahead_result.target_world
    path_direction = lookahead_result.direction
    path_world = path_result.world_path

    candidates = sample_dwa_commands(config)
    best_command = select_best_command(
        robot_state,
        goal,
        target_world,
        path_world,
        obstacles,
        candidates,
        config,
        path_direction,
    )
    debug = build_planned_path_debug(grid_info, path_result)
    lookahead_index = world_to_grid_index(grid_info, target_world["x"], target_world["y"])
    debug.update(
        {
            "lookAheadGridX": lookahead_index[0] if lookahead_index is not None else None,
            "lookAheadGridY": lookahead_index[1] if lookahead_index is not None else None,
            "lookAheadWorldX": target_world["x"],
            "lookAheadWorldY": target_world["y"],
            "lookAheadWorldZ": target_world["z"],
            "nearestPathIndex": lookahead_result.nearest_index,
            "lookAheadPathIndex": lookahead_result.target_index,
            "distanceToPathCm": lookahead_result.distance_to_path_cm,
            "pathDirection": path_direction,
            "dwaObstacleCount": len(obstacles),
            "dwaLidarObstacleCount": len(lidar_obstacles),
            "dwaGridObstacleCount": len(grid_obstacles),
            "dwaActivationDistanceM": config.activation_distance_m,
            "dwaPathLookAheadDistanceM": config.path_lookahead_distance_m,
            "dwaGridObstacleMaxDistanceM": config.grid_obstacle_max_distance_m,
            "dwaSafetyDistanceM": config.safety_distance_m,
            "dwaRobotCollisionRadiusM": config.robot_collision_radius_m,
            "dwaClearanceMarginM": config.clearance_margin_m,
            "dwaObstacleInflationM": config.obstacle_inflation_m,
            "dwaRequiredClearanceCm": get_required_clearance_cm(config),
            "dwaForwardCorridorHalfWidthM": config.forward_corridor_half_width_m,
            "dwaMinAvoidanceSteering": config.min_avoidance_steering,
            "globalPlanner": path_result.planner_id,
        }
    )

    recovery_candidate = build_stuck_recovery_candidate(context, robot_state, obstacles, config, debug)
    if recovery_candidate is not None:
        return recovery_candidate

    if best_command is None:
        debug["dwaStatus"] = "no_safe_trajectory"
        debug.update(build_obstacle_proximity_debug(robot_state, obstacles, config))
        if should_stop_on_no_safe_trajectory(robot_state, obstacles, config):
            return make_policy_candidate(
                POLICY_ID,
                make_stop_action(),
                "dwa_no_safe_trajectory_stop",
                get_policy_priority(context, 25),
                debug,
            )

        return None

    steering, speed_kmh, direction, score, clearance_cm = best_command
    debug.update(
        {
            "dwaStatus": "ok",
            "dwaScore": score,
            "dwaClearanceCm": clearance_cm,
            "dwaBodyClearanceCm": clearance_cm - config.robot_collision_radius_m * 100.0,
            "dwaClearanceRecoverySelected": clearance_cm < get_required_clearance_cm(config),
        }
    )

    return make_policy_candidate(
        POLICY_ID,
        make_action(steering, speed_kmh, direction=direction),
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

    drive_max_reverse_speed_kmh = get_float_field(drive_spec, "maxReverseSpeedKmh", 0.0)
    default_max_reverse_speed_kmh = min(1.5, drive_max_reverse_speed_kmh) if drive_max_reverse_speed_kmh > 0.0 else 0.0

    return DwaConfig(
        activation_distance_m=max(get_float_setting(safe_dwa_spec, "activationDistanceM", 4.0), 0.0),
        path_lookahead_distance_m=max(get_float_setting(safe_dwa_spec, "pathLookAheadDistanceM", 2.0), 0.1),
        safety_distance_m=max(get_float_setting(safe_dwa_spec, "safetyDistanceM", 0.45), 0.0),
        robot_collision_radius_m=max(
            get_float_setting(
                safe_dwa_spec,
                "robotCollisionRadiusM",
                get_default_robot_collision_radius_m(context, 0.9),
            ),
            0.0,
        ),
        clearance_margin_m=max(get_float_setting(safe_dwa_spec, "clearanceMarginM", 0.15), 0.0),
        obstacle_inflation_m=max(get_float_setting(safe_dwa_spec, "obstacleInflationM", 0.1), 0.0),
        no_safe_stop_distance_m=max(get_float_setting(safe_dwa_spec, "noSafeStopDistanceM", 0.55), 0.0),
        stop_on_no_safe_trajectory=get_bool_setting(safe_dwa_spec, "stopOnNoSafeTrajectory", False),
        prediction_time_s=max(get_float_setting(safe_dwa_spec, "predictionTimeS", 1.4), 0.1),
        step_time_s=max(get_float_setting(safe_dwa_spec, "stepTimeS", 0.2), 0.05),
        min_speed_kmh=max(get_float_setting(safe_dwa_spec, "minSpeedKmh", 0.5), 0.0),
        max_speed_kmh=max(get_float_setting(safe_dwa_spec, "maxSpeedKmh", default_max_speed_kmh), 0.0),
        allow_reverse=get_bool_setting(
            safe_dwa_spec,
            "allowReverse",
            default_max_reverse_speed_kmh > 0.0,
        ),
        max_reverse_speed_kmh=max(
            get_float_setting(safe_dwa_spec, "maxReverseSpeedKmh", default_max_reverse_speed_kmh),
            0.0,
        ),
        speed_sample_count=max(get_int_setting(safe_dwa_spec, "speedSampleCount", 4), 1),
        reverse_speed_sample_count=max(get_int_setting(safe_dwa_spec, "reverseSpeedSampleCount", 3), 1),
        steering_sample_count=max(get_int_setting(safe_dwa_spec, "steeringSampleCount", 11), 3),
        max_turn_rate_degree_s=max(get_float_setting(safe_dwa_spec, "maxTurnRateDegreeS", 90.0), 0.0),
        target_weight=max(get_float_setting(safe_dwa_spec, "targetWeight", 1.2), 0.0),
        path_weight=max(get_float_setting(safe_dwa_spec, "pathWeight", 0.8), 0.0),
        clearance_weight=max(get_float_setting(safe_dwa_spec, "clearanceWeight", 0.8), 0.0),
        speed_weight=max(get_float_setting(safe_dwa_spec, "speedWeight", 0.2), 0.0),
        heading_weight=max(get_float_setting(safe_dwa_spec, "headingWeight", 0.6), 0.0),
        reverse_score_penalty=max(get_float_setting(safe_dwa_spec, "reverseScorePenalty", 0.6), 0.0),
        include_grid_obstacles=get_bool_setting(safe_dwa_spec, "includeGridObstacles", True),
        grid_obstacle_max_distance_m=max(
            get_float_setting(safe_dwa_spec, "gridObstacleMaxDistanceM", 3.5),
            0.0,
        ),
        forward_corridor_half_width_m=max(get_float_setting(safe_dwa_spec, "forwardCorridorHalfWidthM", 0.9), 0.0),
        min_avoidance_steering=max(get_float_setting(safe_dwa_spec, "minAvoidanceSteering", 0.25), 0.0),
        avoidance_steering_penalty=max(get_float_setting(safe_dwa_spec, "avoidanceSteeringPenalty", 2.0), 0.0),
        stuck_detection_enabled=get_bool_setting(safe_dwa_spec, "stuckDetectionEnabled", True),
        stuck_speed_threshold_kmh=max(get_float_setting(safe_dwa_spec, "stuckSpeedThresholdKmh", 0.2), 0.0),
        stuck_progress_distance_m=max(get_float_setting(safe_dwa_spec, "stuckProgressDistanceM", 0.15), 0.01),
        stuck_time_seconds=max(get_float_setting(safe_dwa_spec, "stuckTimeSeconds", 0.8), 0.1),
        stuck_close_obstacle_distance_m=max(get_float_setting(safe_dwa_spec, "stuckCloseObstacleDistanceM", 1.2), 0.0),
        stuck_recovery_reverse_duration_s=max(
            get_float_setting(safe_dwa_spec, "stuckRecoveryReverseDurationS", 0.9),
            0.1,
        ),
        stuck_recovery_reverse_speed_kmh=max(
            get_float_setting(safe_dwa_spec, "stuckRecoveryReverseSpeedKmh", 1.0),
            0.0,
        ),
        stuck_recovery_min_clearance_gain_m=max(
            get_float_setting(safe_dwa_spec, "stuckRecoveryMinClearanceGainM", 0.1),
            0.0,
        ),
    )


def build_dwa_motion_spec(context: dict[str, Any], config: DwaConfig) -> dict[str, Any]:
    motion_spec = dict(get_motion_control_spec(context))
    configured_lookahead_m = get_float_field(motion_spec, "lookAheadDistanceM", 1.0)
    motion_spec["lookAheadDistanceM"] = max(configured_lookahead_m, config.path_lookahead_distance_m)
    motion_spec["minLookAheadDistanceM"] = max(
        get_float_field(motion_spec, "minLookAheadDistanceM", 0.0),
        config.path_lookahead_distance_m,
    )
    return motion_spec


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


def get_default_robot_collision_radius_m(context: dict[str, Any], default: float) -> float:
    for source_name in ("observation", "configInfo"):
        source = context.get(source_name, {})
        safe_source = source if isinstance(source, dict) else {}
        vehicle_spec = safe_source.get("vehicleSpec", safe_source.get("VehicleSpec", {}))
        if not isinstance(vehicle_spec, dict):
            continue

        radius_m = get_robot_box_extent_radius_m(vehicle_spec)
        if radius_m > 0.0:
            return radius_m

    return default


def get_robot_box_extent_radius_m(vehicle_spec: dict[str, Any]) -> float:
    box_extent = vehicle_spec.get("robotBoxExtentCm", vehicle_spec.get("RobotBoxExtentCm"))
    x_cm = 0.0
    y_cm = 0.0
    if isinstance(box_extent, dict):
        x_cm = get_float_field(box_extent, "x", get_float_field(box_extent, "X", 0.0))
        y_cm = get_float_field(box_extent, "y", get_float_field(box_extent, "Y", 0.0))
    elif isinstance(box_extent, (list, tuple)) and len(box_extent) >= 2:
        try:
            x_cm = float(box_extent[0])
            y_cm = float(box_extent[1])
        except (TypeError, ValueError):
            return 0.0

    return max(abs(x_cm), abs(y_cm)) / 100.0


def get_required_clearance_cm(config: DwaConfig) -> float:
    footprint_clearance_m = (
        config.robot_collision_radius_m
        + config.clearance_margin_m
        + config.obstacle_inflation_m
    )
    return max(config.safety_distance_m, footprint_clearance_m) * 100.0


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


def build_obstacle_points_from_grid(
    grid_info: dict[str, Any],
    cell_lookup: dict[tuple[int, int], dict[str, Any]],
    robot_state: dict[str, Any],
    max_distance_m: float,
) -> list[tuple[float, float]]:
    max_distance_cm = max_distance_m * 100.0
    if max_distance_cm <= 0.0:
        return []

    robot_x_cm = get_float_field(robot_state, "x")
    robot_y_cm = get_float_field(robot_state, "y")
    cell_half_size_cm = max(get_float_field(grid_info, "cellSizeCm", 100.0) * 0.5, 1.0)
    obstacle_points: list[tuple[float, float]] = []

    for grid_index, cell in cell_lookup.items():
        if not isinstance(grid_index, tuple) or len(grid_index) != 2:
            continue
        if not isinstance(cell, dict) or not is_blocked_cell(cell):
            continue

        world_location = grid_index_to_world_location(grid_info, grid_index)
        distance_cm = math.hypot(world_location["x"] - robot_x_cm, world_location["y"] - robot_y_cm)
        if distance_cm > max_distance_cm:
            continue

        obstacle_points.extend(sample_grid_obstacle_points(world_location["x"], world_location["y"], cell_half_size_cm))

    return obstacle_points


def sample_grid_obstacle_points(center_x_cm: float, center_y_cm: float, half_size_cm: float) -> list[tuple[float, float]]:
    return [
        (center_x_cm, center_y_cm),
        (center_x_cm - half_size_cm, center_y_cm),
        (center_x_cm + half_size_cm, center_y_cm),
        (center_x_cm, center_y_cm - half_size_cm),
        (center_x_cm, center_y_cm + half_size_cm),
        (center_x_cm - half_size_cm, center_y_cm - half_size_cm),
        (center_x_cm - half_size_cm, center_y_cm + half_size_cm),
        (center_x_cm + half_size_cm, center_y_cm - half_size_cm),
        (center_x_cm + half_size_cm, center_y_cm + half_size_cm),
    ]


def dedupe_obstacle_points(obstacles: list[tuple[float, float]]) -> list[tuple[float, float]]:
    unique_points: dict[tuple[int, int], tuple[float, float]] = {}
    for x_cm, y_cm in obstacles:
        unique_points[(round(x_cm), round(y_cm))] = (x_cm, y_cm)

    return list(unique_points.values())


def sample_dwa_commands(config: DwaConfig) -> list[tuple[float, float, str]]:
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

    commands = [
        (steering, max(speed_kmh, 0.0), "Forward")
        for steering in steering_values
        for speed_kmh in speed_values
    ]

    if config.allow_reverse and config.max_reverse_speed_kmh > 0.0:
        reverse_min_speed_kmh = min(config.min_speed_kmh, config.max_reverse_speed_kmh)
        if config.reverse_speed_sample_count <= 1:
            reverse_speed_values = [config.max_reverse_speed_kmh]
        else:
            reverse_speed_values = [
                reverse_min_speed_kmh
                + (config.max_reverse_speed_kmh - reverse_min_speed_kmh)
                * index
                / (config.reverse_speed_sample_count - 1)
                for index in range(config.reverse_speed_sample_count)
            ]

        commands.extend(
            (steering, max(speed_kmh, 0.0), "Reverse")
            for steering in steering_values
            for speed_kmh in reverse_speed_values
        )

    return commands


def select_best_command(
    robot_state: dict[str, Any],
    goal: dict[str, Any],
    target_world: dict[str, float],
    path_world: list[dict[str, float]],
    obstacles: list[tuple[float, float]],
    candidates: list[tuple[float, float, str]],
    config: DwaConfig,
    preferred_direction: str,
) -> tuple[float, float, str, float, float] | None:
    best_command: tuple[float, float, str, float, float] | None = None
    b_has_forward_corridor_obstacle = has_forward_corridor_obstacle(robot_state, obstacles, config)
    required_clearance_cm = get_required_clearance_cm(config)
    current_clearance_cm = get_robot_point_clearance_cm(robot_state, obstacles)

    for steering, speed_kmh, direction in candidates:
        trajectory = simulate_trajectory(robot_state, steering, speed_kmh, direction, config)
        score, clearance_cm = score_trajectory(
            trajectory,
            goal,
            target_world,
            path_world,
            obstacles,
            speed_kmh,
            direction,
            config,
            preferred_direction,
        )
        if (
            direction == "Forward"
            and b_has_forward_corridor_obstacle
            and abs(steering) < config.min_avoidance_steering
        ):
            score -= config.avoidance_steering_penalty

        if clearance_cm < required_clearance_cm:
            end_clearance_cm = get_trajectory_end_clearance_cm(trajectory, obstacles)
            if not is_reverse_clearance_recovery_trajectory(
                direction,
                current_clearance_cm,
                clearance_cm,
                end_clearance_cm,
                config,
            ):
                continue

        if best_command is None or score > best_command[3]:
            best_command = (steering, speed_kmh, direction, score, clearance_cm)

    return best_command


def is_reverse_clearance_recovery_trajectory(
    direction: str,
    current_clearance_cm: float,
    min_clearance_cm: float,
    end_clearance_cm: float,
    config: DwaConfig,
) -> bool:
    if direction != "Reverse" or not math.isfinite(current_clearance_cm):
        return False

    required_clearance_cm = get_required_clearance_cm(config)
    if current_clearance_cm >= required_clearance_cm:
        return False

    required_gain_cm = config.stuck_recovery_min_clearance_gain_m * 100.0
    if end_clearance_cm < current_clearance_cm + required_gain_cm:
        return False

    return min_clearance_cm >= current_clearance_cm - 5.0


def build_stuck_recovery_candidate(
    context: dict[str, Any],
    robot_state: dict[str, Any],
    obstacles: list[tuple[float, float]],
    config: DwaConfig,
    debug: dict[str, Any],
) -> dict[str, Any] | None:
    if not config.stuck_detection_enabled:
        return None

    policy_state = get_dwa_policy_state(context)
    current_time_seconds = get_observation_time_seconds(context)
    active_until_seconds = float(policy_state.get("stuckRecoveryUntilSeconds", 0.0) or 0.0)
    if active_until_seconds > current_time_seconds:
        return make_stuck_reverse_candidate(
            context,
            robot_state,
            obstacles,
            config,
            {
                **debug,
                "dwaStatus": "stuck_reverse_recovery",
                "dwaStuckRecoveryRemainingSeconds": active_until_seconds - current_time_seconds,
            },
        )

    if active_until_seconds > 0.0:
        clear_stuck_recovery_state(policy_state)

    if not has_close_obstacle(robot_state, obstacles, config.stuck_close_obstacle_distance_m):
        reset_progress_anchor(policy_state, robot_state, current_time_seconds)
        return None

    speed_kmh = get_float_field(robot_state, "speedKmh", 0.0)
    robot_x_cm = get_float_field(robot_state, "x")
    robot_y_cm = get_float_field(robot_state, "y")
    anchor_x_cm = float(policy_state.get("progressAnchorXcm", robot_x_cm))
    anchor_y_cm = float(policy_state.get("progressAnchorYcm", robot_y_cm))
    distance_from_anchor_cm = math.hypot(robot_x_cm - anchor_x_cm, robot_y_cm - anchor_y_cm)

    if speed_kmh > config.stuck_speed_threshold_kmh or distance_from_anchor_cm >= config.stuck_progress_distance_m * 100.0:
        reset_progress_anchor(policy_state, robot_state, current_time_seconds)
        return None

    stuck_start_seconds = float(policy_state.get("stuckStartSeconds", current_time_seconds))
    policy_state["stuckStartSeconds"] = stuck_start_seconds
    stuck_elapsed_seconds = max(current_time_seconds - stuck_start_seconds, 0.0)
    if stuck_elapsed_seconds < config.stuck_time_seconds:
        return None

    policy_state["stuckRecoveryUntilSeconds"] = current_time_seconds + config.stuck_recovery_reverse_duration_s
    policy_state["stuckRecoveryCount"] = int(policy_state.get("stuckRecoveryCount", 0) or 0) + 1
    return make_stuck_reverse_candidate(
        context,
        robot_state,
        obstacles,
        config,
        {
            **debug,
            "dwaStatus": "stuck_reverse_recovery",
            "dwaStuckElapsedSeconds": stuck_elapsed_seconds,
            "dwaStuckRecoveryCount": policy_state["stuckRecoveryCount"],
        },
    )


def make_stuck_reverse_candidate(
    context: dict[str, Any],
    robot_state: dict[str, Any],
    obstacles: list[tuple[float, float]],
    config: DwaConfig,
    debug: dict[str, Any],
) -> dict[str, Any]:
    steering, min_clearance_cm, end_clearance_cm = choose_reverse_recovery_steering(robot_state, obstacles, config)
    start_clearance_cm = get_robot_point_clearance_cm(robot_state, obstacles)
    required_clearance_cm = get_required_clearance_cm(config)
    required_gain_cm = config.stuck_recovery_min_clearance_gain_m * 100.0
    b_improves_clearance = end_clearance_cm >= start_clearance_cm + required_gain_cm
    b_has_minimum_clearance = min_clearance_cm >= required_clearance_cm
    recovery_debug = {
        **debug,
        **build_obstacle_proximity_debug(robot_state, obstacles, config),
        "dwaRecoveryDirection": "Reverse",
        "dwaRecoverySteering": steering,
        "dwaRecoveryClearanceCm": min_clearance_cm,
        "dwaRecoveryStartClearanceCm": start_clearance_cm,
        "dwaRecoveryMinClearanceCm": min_clearance_cm,
        "dwaRecoveryEndClearanceCm": end_clearance_cm,
        "dwaRecoveryRequiredGainCm": required_gain_cm,
        "dwaRecoveryImprovesClearance": b_improves_clearance,
        "dwaRequiredClearanceCm": required_clearance_cm,
        "dwaStuckSpeedThresholdKmh": config.stuck_speed_threshold_kmh,
        "dwaStuckProgressDistanceM": config.stuck_progress_distance_m,
        "dwaStuckTimeSeconds": config.stuck_time_seconds,
    }

    if (
        config.stuck_recovery_reverse_speed_kmh <= 0.0
        or (not b_has_minimum_clearance and not b_improves_clearance)
    ):
        recovery_debug["dwaStatus"] = "stuck_recovery_no_reverse_clearance"
        return make_policy_candidate(
            POLICY_ID,
            make_stop_action(),
            "dwa_stuck_recovery_no_reverse_clearance",
            get_policy_priority(context, 25),
            recovery_debug,
        )

    return make_policy_candidate(
        POLICY_ID,
        make_action(steering, config.stuck_recovery_reverse_speed_kmh, direction="Reverse"),
        "dwa_stuck_reverse_recovery",
        get_policy_priority(context, 25),
        recovery_debug,
    )


def choose_reverse_recovery_steering(
    robot_state: dict[str, Any],
    obstacles: list[tuple[float, float]],
    config: DwaConfig,
) -> tuple[float, float, float]:
    steering_values = [-1.0, -0.6, -0.3, 0.0, 0.3, 0.6, 1.0]
    recovery_config = replace(
        config,
        prediction_time_s=config.stuck_recovery_reverse_duration_s,
        step_time_s=min(config.step_time_s, config.stuck_recovery_reverse_duration_s),
    )
    best_steering = 0.0
    best_min_clearance_cm = -math.inf
    best_end_clearance_cm = -math.inf
    best_score = -math.inf

    for steering in steering_values:
        trajectory = simulate_trajectory(
            robot_state,
            steering,
            config.stuck_recovery_reverse_speed_kmh,
            "Reverse",
            recovery_config,
        )
        min_clearance_cm = get_trajectory_clearance_cm(trajectory, obstacles)
        end_clearance_cm = get_trajectory_end_clearance_cm(trajectory, obstacles)
        score = end_clearance_cm + min_clearance_cm * 0.25
        if score > best_score:
            best_steering = steering
            best_min_clearance_cm = min_clearance_cm
            best_end_clearance_cm = end_clearance_cm
            best_score = score

    return best_steering, best_min_clearance_cm, best_end_clearance_cm


def has_close_obstacle(
    robot_state: dict[str, Any],
    obstacles: list[tuple[float, float]],
    max_distance_m: float,
) -> bool:
    max_distance_cm = max_distance_m * 100.0
    if max_distance_cm <= 0.0:
        return False

    robot_x_cm = get_float_field(robot_state, "x")
    robot_y_cm = get_float_field(robot_state, "y")
    return any(
        math.hypot(obstacle_x_cm - robot_x_cm, obstacle_y_cm - robot_y_cm) <= max_distance_cm
        for obstacle_x_cm, obstacle_y_cm in obstacles
    )


def get_dwa_policy_state(context: dict[str, Any]) -> dict[str, Any]:
    runtime_state = context.get("policyRuntimeState", {})
    if not isinstance(runtime_state, dict):
        return {}

    policy_state = runtime_state.setdefault(POLICY_ID, {})
    if not isinstance(policy_state, dict):
        policy_state = {}
        runtime_state[POLICY_ID] = policy_state

    return policy_state


def get_observation_time_seconds(context: dict[str, Any]) -> float:
    observation = context.get("observation", {})
    safe_observation = observation if isinstance(observation, dict) else {}
    return get_float_field(safe_observation, "worldTimeSeconds", 0.0)


def reset_progress_anchor(policy_state: dict[str, Any], robot_state: dict[str, Any], current_time_seconds: float) -> None:
    policy_state["progressAnchorXcm"] = get_float_field(robot_state, "x")
    policy_state["progressAnchorYcm"] = get_float_field(robot_state, "y")
    policy_state["stuckStartSeconds"] = current_time_seconds


def clear_stuck_recovery_state(policy_state: dict[str, Any]) -> None:
    policy_state.pop("stuckRecoveryUntilSeconds", None)
    policy_state.pop("stuckStartSeconds", None)


def has_forward_corridor_obstacle(
    robot_state: dict[str, Any],
    obstacles: list[tuple[float, float]],
    config: DwaConfig,
) -> bool:
    activation_distance_cm = config.activation_distance_m * 100.0
    corridor_half_width_cm = max(config.forward_corridor_half_width_m * 100.0, get_required_clearance_cm(config))
    for forward_cm, lateral_cm, _ in iter_obstacles_in_robot_frame(robot_state, obstacles):
        if 0.0 < forward_cm <= activation_distance_cm and abs(lateral_cm) <= corridor_half_width_cm:
            return True

    return False


def should_stop_on_no_safe_trajectory(
    robot_state: dict[str, Any],
    obstacles: list[tuple[float, float]],
    config: DwaConfig,
) -> bool:
    if config.stop_on_no_safe_trajectory:
        return True

    required_clearance_cm = get_required_clearance_cm(config)
    no_safe_stop_distance_cm = max(config.no_safe_stop_distance_m * 100.0, required_clearance_cm)
    for forward_cm, lateral_cm, _ in iter_obstacles_in_robot_frame(robot_state, obstacles):
        if forward_cm < 0.0:
            continue
        if forward_cm <= no_safe_stop_distance_cm and abs(lateral_cm) <= required_clearance_cm:
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
        "dwaRequiredClearanceCm": get_required_clearance_cm(config),
        "dwaRobotCollisionRadiusM": config.robot_collision_radius_m,
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
    direction: str,
    config: DwaConfig,
) -> list[tuple[float, float, float]]:
    x_cm = get_float_field(robot_state, "x")
    y_cm = get_float_field(robot_state, "y")
    yaw_degree = get_float_field(robot_state, "yawDegree")
    direction_sign = -1.0 if direction == "Reverse" else 1.0
    speed_cm_s = speed_kmh * 100000.0 / 3600.0 * direction_sign
    turn_rate_degree_s = steering * config.max_turn_rate_degree_s * direction_sign
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
    direction: str,
    config: DwaConfig,
    preferred_direction: str,
) -> tuple[float, float]:
    if not trajectory:
        return -math.inf, 0.0

    end_x, end_y, end_yaw_degree = trajectory[-1]
    target_distance_cm = math.hypot(target_world["x"] - end_x, target_world["y"] - end_y)
    path_distance_cm = get_distance_to_path_cm(end_x, end_y, path_world)
    clearance_cm = get_trajectory_clearance_cm(trajectory, obstacles)
    goal_heading_score = get_goal_heading_score(end_x, end_y, end_yaw_degree, goal, direction)

    score = (
        -target_distance_cm * 0.01 * config.target_weight
        - path_distance_cm * 0.01 * config.path_weight
        + min(clearance_cm, config.activation_distance_m * 100.0) * 0.01 * config.clearance_weight
        + speed_kmh * config.speed_weight
        + goal_heading_score * config.heading_weight
    )
    if direction == "Reverse" and preferred_direction != "Reverse":
        score -= config.reverse_score_penalty

    return score, clearance_cm


def get_goal_heading_score(
    end_x: float,
    end_y: float,
    end_yaw_degree: float,
    goal: dict[str, Any],
    direction: str,
) -> float:
    goal_yaw_degree = math.degrees(
        math.atan2(
            get_float_field(goal, "y") - end_y,
            get_float_field(goal, "x") - end_x,
        )
    )
    if direction == "Reverse":
        end_yaw_degree = normalize_angle_degree(end_yaw_degree + 180.0)

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


def get_trajectory_end_clearance_cm(
    trajectory: list[tuple[float, float, float]],
    obstacles: list[tuple[float, float]],
) -> float:
    if not trajectory:
        return math.inf

    end_x_cm, end_y_cm, _ = trajectory[-1]
    return get_point_clearance_cm(end_x_cm, end_y_cm, obstacles)


def get_robot_point_clearance_cm(
    robot_state: dict[str, Any],
    obstacles: list[tuple[float, float]],
) -> float:
    return get_point_clearance_cm(
        get_float_field(robot_state, "x"),
        get_float_field(robot_state, "y"),
        obstacles,
    )


def get_point_clearance_cm(
    x_cm: float,
    y_cm: float,
    obstacles: list[tuple[float, float]],
) -> float:
    if not obstacles:
        return math.inf

    return min(math.hypot(obstacle_x_cm - x_cm, obstacle_y_cm - y_cm) for obstacle_x_cm, obstacle_y_cm in obstacles)


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
