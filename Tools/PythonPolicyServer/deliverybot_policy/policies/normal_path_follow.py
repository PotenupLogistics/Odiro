from __future__ import annotations

import math
from typing import Any

from deliverybot_policy.actions import clamp, make_action, make_policy_candidate, make_stop_action
from deliverybot_policy.context import (
    distance_2d_cm,
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
    build_steering_to_target,
    choose_lookahead_target_info,
    find_path_for_policy,
    world_to_grid_index,
)


POLICY_ID = "normal_path_follow"


def evaluate(context: dict[str, Any]) -> dict[str, Any] | None:
    return build_path_follow_candidate(context, POLICY_ID, "path_follow_action_selected")


def build_path_follow_candidate(
    context: dict[str, Any],
    policy_id: str,
    reason: str,
    speed_limit_kmh: float | None = None,
) -> dict[str, Any]:
    priority = get_policy_priority(context, 100)
    grid_info = context.get("gridInfo", {})
    cell_lookup = context.get("gridCellLookup", {})
    robot_state = get_robot_state(context)
    goal = get_goal(context)

    if not isinstance(grid_info, dict) or not isinstance(cell_lookup, dict) or not grid_info:
        return make_policy_candidate(
            policy_id,
            make_stop_action(),
            "grid_not_ready_for_path_follow",
            priority,
            {"pathStatus": "grid_not_ready"},
        )

    if not robot_state or not goal:
        return make_policy_candidate(
            policy_id,
            make_stop_action(),
            "goal_or_robot_state_missing",
            priority,
            {"pathStatus": "missing_robot_or_goal"},
        )

    motion_spec = get_motion_control_spec(context)
    drive_spec = get_drive_spec(context)
    goal_acceptance_cm = get_float_field(motion_spec, "goalAcceptanceDistanceM", 0.8) * 100.0

    distance_to_goal_cm = distance_2d_cm(robot_state, goal)
    if distance_to_goal_cm <= goal_acceptance_cm:
        return make_policy_candidate(
            policy_id,
            make_stop_action(),
            "goal_reached",
            priority,
            {
                "pathStatus": "goal_reached",
                "distanceToGoalCm": distance_to_goal_cm,
            },
        )

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
        return make_policy_candidate(
            policy_id,
            make_stop_action(),
            "robot_or_goal_outside_grid",
            priority,
            {
                "pathStatus": "outside_grid",
                "robotGridIndex": start_index,
                "goalGridIndex": goal_index,
            },
        )

    path_result = find_path_for_policy(context)
    path = path_result.world_path
    if not path:
        path_debug = build_planned_path_debug(grid_info, path_result)
        path_debug.update(
            {
                "robotGridX": start_index[0],
                "robotGridY": start_index[1],
                "goalGridX": goal_index[0],
                "goalGridY": goal_index[1],
            }
        )
        return make_policy_candidate(
            policy_id,
            make_stop_action(),
            "path_not_found",
            priority,
            path_debug,
        )

    lookahead_result = choose_lookahead_target_info(path_result, robot_state, motion_spec)
    lookahead_world = lookahead_result.target_world
    direction = lookahead_result.direction
    steering, yaw_error_degree = build_steering_to_target(robot_state, lookahead_world, motion_spec, direction)
    target_speed_kmh = build_target_speed_kmh(drive_spec, motion_spec, steering, speed_limit_kmh, direction)
    path_debug = build_planned_path_debug(grid_info, path_result)
    lookahead_grid_index = world_to_grid_index(
        grid_info,
        lookahead_world["x"],
        lookahead_world["y"],
    )
    path_debug.update(
        {
            "lookAheadGridX": lookahead_grid_index[0] if lookahead_grid_index is not None else None,
            "lookAheadGridY": lookahead_grid_index[1] if lookahead_grid_index is not None else None,
            "lookAheadWorldX": lookahead_world["x"],
            "lookAheadWorldY": lookahead_world["y"],
            "lookAheadWorldZ": lookahead_world["z"],
            "nearestPathIndex": lookahead_result.nearest_index,
            "lookAheadPathIndex": lookahead_result.target_index,
            "distanceToPathCm": lookahead_result.distance_to_path_cm,
            "yawErrorDegree": yaw_error_degree,
            "distanceToGoalCm": distance_to_goal_cm,
            "pathDirection": direction,
        }
    )

    return make_policy_candidate(
        policy_id,
        make_action(steering, target_speed_kmh, direction=direction),
        reason,
        priority,
        path_debug,
    )


def choose_lookahead_index(
    grid_info: dict[str, Any],
    path: list[tuple[int, int]],
    motion_spec: dict[str, Any],
) -> tuple[int, int]:
    if not path:
        return (0, 0)

    cell_size_cm = get_float_field(grid_info, "cellSizeCm", 100.0)
    lookahead_distance_cm = get_float_field(motion_spec, "lookAheadDistanceM", 1.0) * 100.0
    min_path_offset = max(1, math.ceil(lookahead_distance_cm / max(cell_size_cm, 1.0)))
    lookahead_offset = min(min_path_offset, len(path) - 1)
    return path[lookahead_offset]


def build_steering(
    robot_state: dict[str, Any],
    target_world: dict[str, float],
    motion_spec: dict[str, Any],
) -> tuple[float, float]:
    delta_x = target_world["x"] - get_float_field(robot_state, "x")
    delta_y = target_world["y"] - get_float_field(robot_state, "y")
    desired_yaw_degree = math.degrees(math.atan2(delta_y, delta_x))
    yaw_error_degree = normalize_angle_degree(desired_yaw_degree - get_float_field(robot_state, "yawDegree"))
    steering_sensitivity = get_float_field(motion_spec, "steeringSensitivity", 0.8)
    steering = clamp((yaw_error_degree / 90.0) * steering_sensitivity, -1.0, 1.0)
    return steering, yaw_error_degree


def build_target_speed_kmh(
    drive_spec: dict[str, Any],
    motion_spec: dict[str, Any],
    steering: float,
    speed_limit_kmh: float | None,
    direction: str = "Forward",
) -> float:
    max_speed_field = "maxReverseSpeedKmh" if direction == "Reverse" else "maxSpeedKmh"
    max_speed_kmh = get_float_field(drive_spec, max_speed_field, 0.0)
    requested_speed_kmh = get_float_field(motion_spec, "targetSpeedKmh", 3.0)
    if requested_speed_kmh <= 0.0:
        requested_speed_kmh = min(3.0, max_speed_kmh) if max_speed_kmh > 0.0 else 0.0

    target_speed_kmh = min(requested_speed_kmh, max_speed_kmh) if max_speed_kmh > 0.0 else requested_speed_kmh

    if speed_limit_kmh is not None:
        target_speed_kmh = min(target_speed_kmh, max(float(speed_limit_kmh), 0.0))

    if abs(steering) > 0.6 and target_speed_kmh > 0.0:
        min_turn_speed_kmh = get_float_field(motion_spec, "minTurnSpeedKmh", 1.0)
        turn_speed_kmh = max(min_turn_speed_kmh, target_speed_kmh * 0.5)
        target_speed_kmh = min(target_speed_kmh, turn_speed_kmh)

    return max(target_speed_kmh, 0.0)
