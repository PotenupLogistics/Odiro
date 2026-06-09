from __future__ import annotations

from typing import Any

from deliverybot_policy.actions import make_policy_candidate, make_stop_action
from deliverybot_policy.context import get_float_field, get_goal, get_policy_priority, get_robot_state
from deliverybot_policy.pathfinding import build_pathfinding_debug, find_policy_astar_path, world_to_grid_index


POLICY_ID = "reroute_when_blocked"


def evaluate(context: dict[str, Any]) -> dict[str, Any] | None:
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

    path_result = find_policy_astar_path(
        grid_info,
        cell_lookup,
        start_index,
        goal_index,
        context.get("policyEntry", {}),
    )
    if path_result.path:
        return None

    path_debug = build_pathfinding_debug(path_result)
    path_debug.update(
        {
            "robotGridX": start_index[0],
            "robotGridY": start_index[1],
            "goalGridX": goal_index[0],
            "goalGridY": goal_index[1],
        }
    )

    return make_policy_candidate(
        POLICY_ID,
        make_stop_action(),
        "reroute_failed_path_blocked_or_missing",
        get_policy_priority(context, 20),
        path_debug,
    )
