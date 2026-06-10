from __future__ import annotations

import argparse
import json
import math
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from typing import Any

from deliverybot_policy.catalog import (
    build_default_policy_spec,
    list_policy_catalog_sources,
    load_policy_catalog,
    load_policy_catalog_by_id,
    normalize_policy_spec,
)
from deliverybot_policy.context import get_nearest_observed_object
from deliverybot_policy.registry import build_runtime_policy_response


CLIENT_DISCONNECT_EXCEPTIONS = (BrokenPipeError, ConnectionAbortedError, ConnectionResetError)
POLICY_MODE_CHOICES = (
    "runtime",
    "forward",
    "left",
    "right",
    "reverse",
    "reverse-left",
    "reverse-right",
    "stop",
    "invalid-speed",
    "invalid-steering",
    "invalid-brake",
    "invalid-direction",
    "missing-action",
    "error-status",
    "mismatch-episode-version",
    "mismatch-config-version",
    "mismatch-grid-version",
    "stale-episode-version",
    "stale-config-version",
    "stale-grid-version",
)

VERSION_MISMATCH_POLICY_MODES = {
    "mismatch-episode-version",
    "mismatch-config-version",
    "mismatch-grid-version",
    "stale-episode-version",
    "stale-config-version",
    "stale-grid-version",
}

PLANNER_MODE_CHOICES = (
    "auto",
    "astar",
    "hybrid-astar",
    "hybrid_astar",
)

DWA_MODE_CHOICES = (
    "policy",
    "on",
    "off",
)

RIGHT_OF_WAY_MODE_CHOICES = (
    "policy",
    "pedestrian",
    "robot",
)


def clamp(value: float, minimum: float, maximum: float) -> float:
    return max(minimum, min(maximum, value))


def get_sequence(observation: dict[str, Any]) -> int:
    return int(observation.get("sequence", 0) or 0)


def get_server_grid_version(server: ThreadingHTTPServer) -> int:
    return int(getattr(server, "grid_version", 0) or 0)


def get_server_grid_lock(server: ThreadingHTTPServer) -> threading.RLock:
    grid_lock = getattr(server, "grid_lock", None)
    if grid_lock is None:
        grid_lock = threading.RLock()
        server.grid_lock = grid_lock
    return grid_lock


def get_server_grid_summary(server: ThreadingHTTPServer) -> dict[str, Any]:
    grid_summary = getattr(server, "grid_summary", None)
    return grid_summary if isinstance(grid_summary, dict) else {}


def get_server_episode_info(server: ThreadingHTTPServer) -> dict[str, Any]:
    episode_info = getattr(server, "episode_info", None)
    return episode_info if isinstance(episode_info, dict) else {}


def get_server_config_info(server: ThreadingHTTPServer) -> dict[str, Any]:
    config_info = getattr(server, "config_info", None)
    return config_info if isinstance(config_info, dict) else {}


def get_server_episode_version(server: ThreadingHTTPServer) -> int:
    return int(getattr(server, "episode_version", 0) or 0)


def get_server_config_version(server: ThreadingHTTPServer) -> int:
    return int(getattr(server, "config_version", 0) or 0)


def get_server_grid_cell_lookup(server: ThreadingHTTPServer) -> dict[tuple[int, int], dict[str, Any]]:
    grid_cell_lookup = getattr(server, "grid_cell_lookup", None)
    return grid_cell_lookup if isinstance(grid_cell_lookup, dict) else {}


def get_server_policy_catalog(server: ThreadingHTTPServer) -> dict[str, Any]:
    policy_catalog = getattr(server, "policy_catalog", None)
    return policy_catalog if isinstance(policy_catalog, dict) else {}


def get_server_active_catalog_id(server: ThreadingHTTPServer) -> str:
    active_catalog_id = getattr(server, "active_catalog_id", "")
    return str(active_catalog_id or "")


def get_server_policy_spec(server: ThreadingHTTPServer) -> dict[str, Any]:
    policy_spec = getattr(server, "policy_spec", None)
    return policy_spec if isinstance(policy_spec, dict) else {}


def get_server_policy_runtime_state(server: ThreadingHTTPServer) -> dict[str, Any]:
    policy_runtime_state = getattr(server, "policy_runtime_state", None)
    if not isinstance(policy_runtime_state, dict):
        policy_runtime_state = {}
        server.policy_runtime_state = policy_runtime_state

    return policy_runtime_state


def has_runtime_policy_spec(server: ThreadingHTTPServer) -> bool:
    return bool(getattr(server, "policy_spec_received", False))


def build_grid_status_response(server: ThreadingHTTPServer) -> dict[str, Any]:
    with get_server_grid_lock(server):
        grid_summary = dict(get_server_grid_summary(server))
        grid_version = get_server_grid_version(server)

    return {
        "status": "ok",
        "gridReceived": bool(grid_summary),
        "gridVersion": grid_version,
        "gridSizeX": int(grid_summary.get("gridSizeX", 0) or 0),
        "gridSizeY": int(grid_summary.get("gridSizeY", 0) or 0),
        "cellCount": int(grid_summary.get("cellCount", 0) or 0),
        "walkableCount": int(grid_summary.get("walkableCount", 0) or 0),
        "penaltyCount": int(grid_summary.get("penaltyCount", 0) or 0),
        "blockedCount": int(grid_summary.get("blockedCount", 0) or 0),
    }


def build_episode_status_response(server: ThreadingHTTPServer) -> dict[str, Any]:
    with get_server_grid_lock(server):
        episode_info = dict(get_server_episode_info(server))
        config_info = dict(get_server_config_info(server))
        policy_catalog = dict(get_server_policy_catalog(server))
        policy_spec = dict(get_server_policy_spec(server))
        episode_version = get_server_episode_version(server)
        config_version = get_server_config_version(server)
        grid_status = build_grid_status_response(server)

    return {
        "status": "ok",
        "episodeReceived": bool(episode_info),
        "episodeVersion": episode_version,
        "episodeId": str(episode_info.get("episodeId", "")),
        "robotInstanceId": str(episode_info.get("robotInstanceId", "")),
        "hasStart": isinstance(episode_info.get("start"), dict),
        "hasGoal": bool(episode_info.get("hasGoal", False)),
        "configReceived": bool(config_info),
        "configVersion": config_version,
        "policyCatalogVersion": int(policy_catalog.get("catalogVersion", 0) or 0),
        "policySpecReceived": has_runtime_policy_spec(server),
        "enabledPolicyCount": len(policy_spec.get("enabledPolicies", []))
        if isinstance(policy_spec.get("enabledPolicies", []), list)
        else 0,
        "plannerMode": str(getattr(server, "planner_mode", "auto")),
        "dwaMode": str(getattr(server, "dwa_mode", "policy")),
        "rightOfWayMode": str(getattr(server, "right_of_way_mode", "policy")),
        **grid_status,
    }


def build_policy_catalog_response(server: ThreadingHTTPServer) -> dict[str, Any]:
    with get_server_grid_lock(server):
        policy_catalog = dict(get_server_policy_catalog(server))
        active_catalog_id = get_server_active_catalog_id(server)

    return {
        "status": "ok",
        "activeCatalogId": active_catalog_id,
        **policy_catalog,
    }


def build_policy_catalog_sources_response(server: ThreadingHTTPServer) -> dict[str, Any]:
    with get_server_grid_lock(server):
        active_catalog_id = get_server_active_catalog_id(server)

    return {
        "status": "ok",
        "activeCatalogId": active_catalog_id,
        "sources": list_policy_catalog_sources(),
    }


def build_policy_spec_response(server: ThreadingHTTPServer) -> dict[str, Any]:
    with get_server_grid_lock(server):
        policy_spec = dict(get_server_policy_spec(server))
        policy_catalog = dict(get_server_policy_catalog(server))
        active_catalog_id = get_server_active_catalog_id(server)

    return {
        "status": "ok",
        "activeCatalogId": active_catalog_id,
        "policyCatalogVersion": int(policy_catalog.get("catalogVersion", 0) or 0),
        "policySpecReceived": has_runtime_policy_spec(server),
        "policySpec": policy_spec,
        "enabledPolicyCount": len(policy_spec.get("enabledPolicies", []))
        if isinstance(policy_spec.get("enabledPolicies", []), list)
        else 0,
        "plannerMode": str(getattr(server, "planner_mode", "auto")),
        "dwaMode": str(getattr(server, "dwa_mode", "policy")),
        "rightOfWayMode": str(getattr(server, "right_of_way_mode", "policy")),
    }


def get_int_field(source: dict[str, Any], field_name: str, default: int = 0) -> int:
    return int(source.get(field_name, default) or default)


def get_float_field(source: dict[str, Any], field_name: str, default: float = 0.0) -> float:
    return float(source.get(field_name, default) or default)


def get_nested_object(source: dict[str, Any], field_name: str) -> dict[str, Any]:
    value = source.get(field_name, {})
    return value if isinstance(value, dict) else {}


def merge_dict_recursive(base: dict[str, Any], update: dict[str, Any]) -> dict[str, Any]:
    merged = dict(base)
    for key, value in update.items():
        if isinstance(value, dict) and isinstance(merged.get(key), dict):
            merged[key] = merge_dict_recursive(merged[key], value)
        else:
            merged[key] = value
    return merged


def build_grid_cell_lookup(grid_info: dict[str, Any]) -> dict[tuple[int, int], dict[str, Any]]:
    cells = grid_info.get("cells", [])
    safe_cells = cells if isinstance(cells, list) else []

    cell_lookup: dict[tuple[int, int], dict[str, Any]] = {}
    for cell in safe_cells:
        if not isinstance(cell, dict):
            continue

        try:
            grid_x = get_int_field(cell, "x")
            grid_y = get_int_field(cell, "y")
        except (TypeError, ValueError):
            continue

        cell_lookup[(grid_x, grid_y)] = cell

    return cell_lookup


def build_config_info_from_payload(payload: dict[str, Any]) -> dict[str, Any]:
    config_info: dict[str, Any] = {}

    config_object = payload.get("config", {})
    if isinstance(config_object, dict):
        config_info.update(config_object)

    for field_name in (
        "locationSpec",
        "driveSpec",
        "lidarSpec",
        "motionControlSpec",
        # Legacy compatibility while Unreal still sends/uses these names.
        "vehicleSpec",
        "controlSpec",
    ):
        field_value = payload.get(field_name)
        if isinstance(field_value, dict):
            config_info[field_name] = field_value

    return config_info


def build_episode_info_from_payload(payload: dict[str, Any]) -> dict[str, Any]:
    location_spec = get_nested_object(payload, "locationSpec")

    start = get_nested_object(payload, "start")
    goal = get_nested_object(payload, "goal")

    start_from_location_spec = get_nested_object(location_spec, "startLocationCm")
    goal_from_location_spec = get_nested_object(location_spec, "goalLocationCm")

    if start_from_location_spec:
        start = start_from_location_spec

    if goal_from_location_spec:
        goal = goal_from_location_spec

    has_goal = bool(goal.get("hasGoal", bool(goal)))
    if location_spec:
        has_goal = bool(location_spec.get("autoStartRoute", has_goal))

    return {
        "episodeId": str(payload.get("episodeId", "")),
        "robotInstanceId": str(payload.get("robotInstanceId", "")),
        "start": start,
        "goal": goal,
        "hasGoal": has_goal,
        "locationSpec": location_spec,
    }


def store_grid_info(server: ThreadingHTTPServer, grid_info: dict[str, Any]) -> dict[str, Any]:
    server.grid_version = get_server_grid_version(server) + 1
    server.grid_info = grid_info
    server.grid_cell_lookup = build_grid_cell_lookup(grid_info)
    server.grid_summary = build_grid_summary(grid_info, server.grid_version)
    return build_grid_status_response(server)


def build_grid_summary(grid_info: dict[str, Any], grid_version: int) -> dict[str, Any]:
    cells = grid_info.get("cells", [])
    safe_cells = cells if isinstance(cells, list) else []

    walkable_count = 0
    penalty_count = 0
    blocked_count = 0

    for cell in safe_cells:
        if not isinstance(cell, dict):
            continue

        area_type = str(cell.get("areaType", ""))
        is_blocked = bool(cell.get("blocked", False))

        if is_blocked or area_type == "Blocked":
            blocked_count += 1
        elif area_type == "Penalty":
            penalty_count += 1
        elif area_type == "Walkable":
            walkable_count += 1

    return {
        "gridVersion": grid_version,
        "gridSizeX": int(grid_info.get("gridSizeX", 0) or 0),
        "gridSizeY": int(grid_info.get("gridSizeY", 0) or 0),
        "cellSizeCm": float(grid_info.get("cellSizeCm", 0.0) or 0.0),
        "cellCount": int(grid_info.get("cellCount", len(safe_cells)) or len(safe_cells)),
        "walkableCount": walkable_count,
        "penaltyCount": penalty_count,
        "blockedCount": blocked_count,
    }


def world_to_grid_index(grid_info: dict[str, Any], world_x_cm: float, world_y_cm: float) -> tuple[int, int] | None:
    cell_size_cm = get_float_field(grid_info, "cellSizeCm")
    grid_size_x = get_int_field(grid_info, "gridSizeX")
    grid_size_y = get_int_field(grid_info, "gridSizeY")
    origin_cm = grid_info.get("originCm", {})

    if cell_size_cm <= 0.0 or grid_size_x <= 0 or grid_size_y <= 0:
        return None

    if not isinstance(origin_cm, dict):
        return None

    origin_x_cm = get_float_field(origin_cm, "x")
    origin_y_cm = get_float_field(origin_cm, "y")

    grid_x = math.floor((world_x_cm - origin_x_cm) / cell_size_cm)
    grid_y = math.floor((world_y_cm - origin_y_cm) / cell_size_cm)

    if grid_x < 0 or grid_y < 0 or grid_x >= grid_size_x or grid_y >= grid_size_y:
        return None

    return int(grid_x), int(grid_y)


def get_cell_by_grid_index(
    grid_cell_lookup: dict[tuple[int, int], dict[str, Any]],
    grid_x: int,
    grid_y: int,
) -> dict[str, Any] | None:
    cell = grid_cell_lookup.get((grid_x, grid_y))
    return cell if isinstance(cell, dict) else None


def build_robot_grid_debug(server: ThreadingHTTPServer, observation: dict[str, Any]) -> dict[str, Any]:
    robot_state = observation.get("robotState", {})
    if not isinstance(robot_state, dict):
        return {
            "robotGridStatus": "missing_robot_state",
        }

    try:
        robot_x_cm = get_float_field(robot_state, "x")
        robot_y_cm = get_float_field(robot_state, "y")
    except (TypeError, ValueError):
        return {
            "robotGridStatus": "invalid_robot_location",
        }

    with get_server_grid_lock(server):
        grid_info = getattr(server, "grid_info", None)
        grid_cell_lookup = get_server_grid_cell_lookup(server)
        grid_version = get_server_grid_version(server)

        if not isinstance(grid_info, dict):
            return {
                "robotGridStatus": "grid_not_received",
                "robotGridVersion": grid_version,
            }

        grid_index = world_to_grid_index(grid_info, robot_x_cm, robot_y_cm)
        if grid_index is None:
            return {
                "robotGridStatus": "outside_grid",
                "robotGridVersion": grid_version,
                "robotWorldX": robot_x_cm,
                "robotWorldY": robot_y_cm,
            }

        grid_x, grid_y = grid_index
        cell = get_cell_by_grid_index(grid_cell_lookup, grid_x, grid_y)
        if cell is None:
            return {
                "robotGridStatus": "cell_not_found",
                "robotGridVersion": grid_version,
                "robotGridX": grid_x,
                "robotGridY": grid_y,
            }

        return {
            "robotGridStatus": "ok",
            "robotGridVersion": grid_version,
            "robotGridX": grid_x,
            "robotGridY": grid_y,
            "robotCellAreaType": str(cell.get("areaType", "Unknown")),
            "robotCellCost": get_float_field(cell, "cost"),
            "robotCellBlocked": bool(cell.get("blocked", False)),
            "robotCellSourceCollisionProfile": str(cell.get("sourceCollisionProfile", "")),
        }


def build_point_grid_debug(
    server: ThreadingHTTPServer,
    point: dict[str, Any],
    prefix: str,
) -> dict[str, Any]:
    if not isinstance(point, dict) or not point:
        return {
            f"{prefix}GridStatus": "missing_point",
        }

    try:
        point_x_cm = get_float_field(point, "x")
        point_y_cm = get_float_field(point, "y")
    except (TypeError, ValueError):
        return {
            f"{prefix}GridStatus": "invalid_point",
        }

    with get_server_grid_lock(server):
        grid_info = getattr(server, "grid_info", None)
        grid_cell_lookup = get_server_grid_cell_lookup(server)
        grid_version = get_server_grid_version(server)

        if not isinstance(grid_info, dict):
            return {
                f"{prefix}GridStatus": "grid_not_received",
                f"{prefix}GridVersion": grid_version,
            }

        grid_index = world_to_grid_index(grid_info, point_x_cm, point_y_cm)
        if grid_index is None:
            return {
                f"{prefix}GridStatus": "outside_grid",
                f"{prefix}GridVersion": grid_version,
                f"{prefix}WorldX": point_x_cm,
                f"{prefix}WorldY": point_y_cm,
            }

        grid_x, grid_y = grid_index
        cell = get_cell_by_grid_index(grid_cell_lookup, grid_x, grid_y)
        if cell is None:
            return {
                f"{prefix}GridStatus": "cell_not_found",
                f"{prefix}GridVersion": grid_version,
                f"{prefix}GridX": grid_x,
                f"{prefix}GridY": grid_y,
            }

        return {
            f"{prefix}GridStatus": "ok",
            f"{prefix}GridVersion": grid_version,
            f"{prefix}GridX": grid_x,
            f"{prefix}GridY": grid_y,
            f"{prefix}CellAreaType": str(cell.get("areaType", "Unknown")),
            f"{prefix}CellCost": get_float_field(cell, "cost"),
            f"{prefix}CellBlocked": bool(cell.get("blocked", False)),
            f"{prefix}CellSourceCollisionProfile": str(cell.get("sourceCollisionProfile", "")),
        }


def build_episode_goal_debug(server: ThreadingHTTPServer, observation: dict[str, Any]) -> dict[str, Any]:
    with get_server_grid_lock(server):
        episode_info = dict(get_server_episode_info(server))

    goal = get_nested_object(episode_info, "goal")
    if not goal or not bool(episode_info.get("hasGoal", False)):
        return {
            "goalGridStatus": "goal_not_received",
        }

    debug = build_point_grid_debug(server, goal, "goal")

    robot_state = get_nested_object(observation, "robotState")
    if robot_state:
        try:
            robot_x_cm = get_float_field(robot_state, "x")
            robot_y_cm = get_float_field(robot_state, "y")
            goal_x_cm = get_float_field(goal, "x")
            goal_y_cm = get_float_field(goal, "y")
            debug["distanceToGoalCm"] = math.hypot(goal_x_cm - robot_x_cm, goal_y_cm - robot_y_cm)
        except (TypeError, ValueError):
            debug["distanceToGoalCm"] = 0.0

    return debug


def log_nearest_observed_object(observation: dict[str, Any]) -> None:
    nearest_object = get_nearest_observed_object(observation)
    if nearest_object is None:
        return

    actor_tags = nearest_object.get("actorTags", [])
    safe_actor_tags = actor_tags if isinstance(actor_tags, list) else []

    print(
        "nearest object "
        f"actor={nearest_object.get('actorName', '')} "
        f"tags={[str(tag) for tag in safe_actor_tags]} "
        f"distanceM={float(nearest_object.get('closestDistanceM', 0.0) or 0.0):.2f} "
        f"inFront={bool(nearest_object.get('inFront', False))}"
    )


def log_selected_policy_action(response: dict[str, Any]) -> None:
    debug = response.get("debug", {})
    safe_debug = debug if isinstance(debug, dict) else {}

    action = response.get("action", {})
    safe_action = action if isinstance(action, dict) else {}

    enabled_policies = safe_debug.get("enabledPolicies", [])
    safe_enabled_policies = enabled_policies if isinstance(enabled_policies, list) else []

    print(
        "selected policy "
        f"id={safe_debug.get('selectedPolicyId', safe_debug.get('policyName', 'unknown_policy'))} "
        f"priority={int(safe_debug.get('selectedPolicyPriority', 0) or 0)} "
        f"reason={safe_debug.get('reason', '')} "
        f"pathStatus={safe_debug.get('pathStatus', '-')} "
        f"dynamicBlocked={int(safe_debug.get('dynamicObstacleBlockedCellCount', 0) or 0)} "
        f"stopElapsed={float(safe_debug.get('stopSustainSeconds', 0.0) or 0.0):.2f} "
        f"rerouteAttempts={int(safe_debug.get('rerouteAttemptCount', 0) or 0)} "
        f"recovery={safe_debug.get('recoveryMode', '-')} "
        f"candidates={int(safe_debug.get('candidateCount', 0) or 0)} "
        f"elapsedMs={get_debug_float(safe_debug, 'decisionElapsedMs', 0.0):.1f} "
        f"pathCacheHit={bool(safe_debug.get('pathCacheHit', False))} "
        f"nearestPathIndex={get_debug_int(safe_debug, 'nearestPathIndex', -1)} "
        f"lookAheadPathIndex={get_debug_int(safe_debug, 'lookAheadPathIndex', -1)} "
        f"lookAhead=({float(safe_debug.get('lookAheadWorldX', 0.0) or 0.0):.1f},"
        f"{float(safe_debug.get('lookAheadWorldY', 0.0) or 0.0):.1f}) "
        f"distanceToPathCm={float(safe_debug.get('distanceToPathCm', 0.0) or 0.0):.1f} "
        f"dwaStatus={safe_debug.get('dwaStatus', '-')} "
        f"dwaObstacles={int(safe_debug.get('dwaObstacleCount', 0) or 0)} "
        f"dwaLidar={int(safe_debug.get('dwaLidarObstacleCount', 0) or 0)} "
        f"dwaGrid={int(safe_debug.get('dwaGridObstacleCount', 0) or 0)} "
        f"dwaClearanceCm={get_debug_float(safe_debug, 'dwaClearanceCm', 0.0):.1f} "
        f"dwaClearanceRecovery={bool(safe_debug.get('dwaClearanceRecoverySelected', False))} "
        f"dwaRecovery={safe_debug.get('dwaRecoveryDirection', '-')} "
        f"dwaRecoveryClearanceCm={get_debug_float(safe_debug, 'dwaRecoveryClearanceCm', 0.0):.1f} "
        f"dwaRequiredClearanceCm={get_debug_float(safe_debug, 'dwaRequiredClearanceCm', 0.0):.1f} "
        f"dwaRecoveryStartCm={get_debug_float(safe_debug, 'dwaRecoveryStartClearanceCm', 0.0):.1f} "
        f"dwaRecoveryEndCm={get_debug_float(safe_debug, 'dwaRecoveryEndClearanceCm', 0.0):.1f} "
        f"enabled={[str(policy_id) for policy_id in safe_enabled_policies]} "
        f"direction={safe_action.get('direction', '-')} "
        f"speedKmh={float(safe_action.get('targetSpeedKmh', 0.0) or 0.0):.2f} "
        f"steering={float(safe_action.get('steering', 0.0) or 0.0):.2f} "
        f"throttle={float(safe_action.get('throttle', 0.0) or 0.0):.2f} "
        f"brake={float(safe_action.get('brake', 0.0) or 0.0):.2f}"
    )


def get_debug_int(debug: dict[str, Any], field_name: str, default: int = 0) -> int:
    try:
        return int(debug.get(field_name, default))
    except (TypeError, ValueError):
        return default


def get_debug_float(debug: dict[str, Any], field_name: str, default: float = 0.0) -> float:
    try:
        return float(debug.get(field_name, default))
    except (TypeError, ValueError):
        return default


def should_log_runtime_messages(server: ThreadingHTTPServer) -> bool:
    return bool(getattr(server, "verbose_runtime_log", False))


def build_runtime_policy_context(
    server: ThreadingHTTPServer,
    observation: dict[str, Any],
) -> dict[str, Any]:
    with get_server_grid_lock(server):
        grid_info = getattr(server, "grid_info", None)
        grid_cell_lookup = dict(get_server_grid_cell_lookup(server))
        grid_summary = dict(get_server_grid_summary(server))
        episode_info = dict(get_server_episode_info(server))
        config_info = dict(get_server_config_info(server))
        policy_catalog = dict(get_server_policy_catalog(server))
        policy_spec = dict(get_server_policy_spec(server))
        policy_runtime_state = get_server_policy_runtime_state(server)

    return {
        "observation": observation,
        "gridInfo": grid_info if isinstance(grid_info, dict) else {},
        "gridCellLookup": grid_cell_lookup,
        "gridSummary": grid_summary,
        "episodeInfo": episode_info,
        "configInfo": config_info,
        "policyCatalog": policy_catalog,
        "policySpec": policy_spec,
        "policyRuntimeState": policy_runtime_state,
        "plannerMode": str(getattr(server, "planner_mode", "auto")),
        "dwaMode": str(getattr(server, "dwa_mode", "policy")),
        "rightOfWayMode": str(getattr(server, "right_of_way_mode", "policy")),
    }


def make_response(
    observation: dict[str, Any],
    policy_name: str,
    reason: str,
    action: dict[str, Any] | None,
    status: str = "ok",
) -> dict[str, Any]:
    response: dict[str, Any] = {
        "sequence": get_sequence(observation),
        "status": status,
        "debug": {
            "policyName": policy_name,
            "reason": reason,
        },
    }

    if action is not None:
        response["action"] = action

    return response


def build_forward_test_response(observation: dict[str, Any]) -> dict[str, Any]:
    vehicle_spec = get_vehicle_spec(observation)
    max_speed_kmh = float(vehicle_spec.get("maxSpeedKmh", 0.0) or 0.0)
    target_speed_kmh = clamp(3.0, 0.0, max_speed_kmh)
    should_move = target_speed_kmh > 0.0

    return make_response(
        observation,
        "forward_test_policy",
        "smoke_test_server_returns_low_speed_forward_action",
        {
            "steering": 0.0,
            "throttle": 1.0 if should_move else 0.0,
            "brake": 0.0 if should_move else 1.0,
            "targetSpeedKmh": target_speed_kmh,
            "direction": "Forward",
        },
    )


def get_vehicle_spec(observation: dict[str, Any]) -> dict[str, Any]:
    vehicle_spec = observation.get("vehicleSpec", {})
    if isinstance(vehicle_spec, dict) and vehicle_spec:
        return vehicle_spec

    drive_spec = observation.get("driveSpec", {})
    if isinstance(drive_spec, dict):
        return {
            "maxSpeedKmh": drive_spec.get("maxSpeedKmh", 0.0),
            "maxReverseSpeedKmh": drive_spec.get("maxReverseSpeedKmh", 0.0),
        }

    return {}


def enrich_observation_with_server_config(
    server: ThreadingHTTPServer,
    observation: dict[str, Any],
) -> dict[str, Any]:
    enriched_observation = dict(observation)

    with get_server_grid_lock(server):
        config_info = dict(get_server_config_info(server))
        episode_version = get_server_episode_version(server)
        config_version = get_server_config_version(server)

    if "vehicleSpec" not in enriched_observation:
        if isinstance(config_info.get("vehicleSpec"), dict):
            enriched_observation["vehicleSpec"] = config_info["vehicleSpec"]
        elif isinstance(config_info.get("driveSpec"), dict):
            drive_spec = config_info["driveSpec"]
            enriched_observation["vehicleSpec"] = {
                "maxSpeedKmh": drive_spec.get("maxSpeedKmh", 0.0),
                "maxReverseSpeedKmh": drive_spec.get("maxReverseSpeedKmh", 0.0),
            }

    if "driveSpec" not in enriched_observation and isinstance(config_info.get("driveSpec"), dict):
        enriched_observation["driveSpec"] = config_info["driveSpec"]

    if "lidarSpec" not in enriched_observation and isinstance(config_info.get("lidarSpec"), dict):
        enriched_observation["lidarSpec"] = config_info["lidarSpec"]

    if (
        "motionControlSpec" not in enriched_observation
        and isinstance(config_info.get("motionControlSpec"), dict)
    ):
        enriched_observation["motionControlSpec"] = config_info["motionControlSpec"]

    enriched_observation.setdefault("episodeVersion", episode_version)
    enriched_observation.setdefault("configVersion", config_version)

    return enriched_observation


def get_max_forward_speed_kmh(observation: dict[str, Any]) -> float:
    vehicle_spec = get_vehicle_spec(observation)
    return float(vehicle_spec.get("maxSpeedKmh", 0.0) or 0.0)


def get_max_reverse_speed_kmh(observation: dict[str, Any]) -> float:
    vehicle_spec = get_vehicle_spec(observation)
    return float(vehicle_spec.get("maxReverseSpeedKmh", 0.0) or 0.0)


def build_move_test_response(
    observation: dict[str, Any],
    policy_mode: str,
    steering: float,
    requested_speed_kmh: float,
    direction: str,
) -> dict[str, Any]:
    max_speed_kmh = (
        get_max_reverse_speed_kmh(observation)
        if direction == "Reverse"
        else get_max_forward_speed_kmh(observation)
    )
    target_speed_kmh = clamp(requested_speed_kmh, 0.0, max_speed_kmh)
    should_move = target_speed_kmh > 0.0

    return make_response(
        observation,
        f"{policy_mode}_test_policy",
        f"server_returns_{policy_mode}_action",
        {
            "steering": steering,
            "throttle": 1.0 if should_move else 0.0,
            "brake": 0.0 if should_move else 1.0,
            "targetSpeedKmh": target_speed_kmh,
            "direction": direction,
        },
    )


def build_stop_test_response(observation: dict[str, Any]) -> dict[str, Any]:
    return make_response(
        observation,
        "stop_test_policy",
        "server_returns_stop_action",
        {
            "steering": 0.0,
            "throttle": 0.0,
            "brake": 1.0,
            "targetSpeedKmh": 0.0,
            "direction": "Forward",
        },
    )


def build_invalid_test_response(observation: dict[str, Any], policy_mode: str) -> dict[str, Any]:
    action: dict[str, Any] = {
        "steering": 0.0,
        "throttle": 1.0,
        "brake": 0.0,
        "targetSpeedKmh": 3.0,
        "direction": "Forward",
    }

    if policy_mode == "invalid-speed":
        action["targetSpeedKmh"] = 100.0
    elif policy_mode == "invalid-steering":
        action["steering"] = 2.0
    elif policy_mode == "invalid-brake":
        action["brake"] = 2.0
    elif policy_mode == "invalid-direction":
        action["direction"] = "Sideways"

    return make_response(
        observation,
        f"{policy_mode}_policy",
        f"server_returns_{policy_mode}_action_for_validation_test",
        action,
    )


def build_version_mismatch_test_response(observation: dict[str, Any], policy_mode: str) -> dict[str, Any]:
    response = build_forward_test_response(observation)
    response["debug"]["policyName"] = f"{policy_mode}_policy"
    response["debug"]["reason"] = f"server_returns_{policy_mode}_for_version_validation_test"
    return response


def apply_version_mismatch_policy_mode(response: dict[str, Any], policy_mode: str) -> None:
    if policy_mode not in VERSION_MISMATCH_POLICY_MODES:
        return

    debug_info = response.setdefault("debug", {})
    if not isinstance(debug_info, dict):
        debug_info = {}
        response["debug"] = debug_info

    if policy_mode == "mismatch-episode-version":
        response["episodeVersion"] = int(response.get("episodeVersion", 0) or 0) + 1
        debug_info["versionMismatchTest"] = "episodeVersion_plus_one"
        return

    if policy_mode == "mismatch-config-version":
        response["configVersion"] = int(response.get("configVersion", 0) or 0) + 1
        debug_info["versionMismatchTest"] = "configVersion_plus_one"
        return

    if policy_mode == "mismatch-grid-version":
        response["gridVersion"] = int(response.get("gridVersion", 0) or 0) + 1
        debug_info["versionMismatchTest"] = "gridVersion_plus_one"
        return

    if policy_mode == "stale-episode-version":
        response["episodeVersion"] = max(int(response.get("episodeVersion", 0) or 0) - 1, 0)
        debug_info["versionMismatchTest"] = "episodeVersion_minus_one"
        return

    if policy_mode == "stale-config-version":
        response["configVersion"] = max(int(response.get("configVersion", 0) or 0) - 1, 0)
        debug_info["versionMismatchTest"] = "configVersion_minus_one"
        return

    if policy_mode == "stale-grid-version":
        response["gridVersion"] = max(int(response.get("gridVersion", 0) or 0) - 1, 0)
        debug_info["versionMismatchTest"] = "gridVersion_minus_one"


def build_policy_response(observation: dict[str, Any], policy_mode: str) -> dict[str, Any]:
    if policy_mode == "forward":
        return build_forward_test_response(observation)

    if policy_mode in VERSION_MISMATCH_POLICY_MODES:
        return build_version_mismatch_test_response(observation, policy_mode)

    if policy_mode == "left":
        return build_move_test_response(observation, policy_mode, -0.5, 3.0, "Forward")

    if policy_mode == "right":
        return build_move_test_response(observation, policy_mode, 0.5, 3.0, "Forward")

    if policy_mode == "reverse":
        return build_move_test_response(observation, policy_mode, 0.0, 1.5, "Reverse")

    if policy_mode == "reverse-left":
        return build_move_test_response(observation, policy_mode, -0.5, 1.5, "Reverse")

    if policy_mode == "reverse-right":
        return build_move_test_response(observation, policy_mode, 0.5, 1.5, "Reverse")

    if policy_mode == "stop":
        return build_stop_test_response(observation)

    if policy_mode in {"invalid-speed", "invalid-steering", "invalid-brake", "invalid-direction"}:
        return build_invalid_test_response(observation, policy_mode)

    if policy_mode == "missing-action":
        return make_response(
            observation,
            "missing_action_policy",
            "server_returns_status_ok_without_action_object",
            None,
        )

    if policy_mode == "error-status":
        return make_response(
            observation,
            "error_status_policy",
            "server_returns_error_status_for_failure_test",
            None,
            status="error",
        )

    return build_forward_test_response(observation)


class DeliveryBotPolicyHandler(BaseHTTPRequestHandler):
    server_version = "DeliveryBotPolicyServer/0.1"

    def do_GET(self) -> None:
        request_path = self.path.split("?", 1)[0]

        if request_path == "/health":
            self.send_json(200, build_episode_status_response(self.server))
            return

        if request_path == "/grid/status":
            self.send_json(200, build_grid_status_response(self.server))
            return

        if request_path == "/episode/status":
            self.send_json(200, build_episode_status_response(self.server))
            return

        if request_path == "/policy/catalog":
            self.send_json(200, build_policy_catalog_response(self.server))
            return

        if request_path == "/policy/catalog/sources":
            self.send_json(200, build_policy_catalog_sources_response(self.server))
            return

        if request_path == "/policy/spec/status":
            self.send_json(200, build_policy_spec_response(self.server))
            return

        self.send_json(404, {"status": "error", "message": "unknown endpoint"})

    def do_POST(self) -> None:
        request_path = self.path.split("?", 1)[0]

        if request_path == "/episode/start":
            self.handle_episode_start()
            return

        if request_path == "/episode/config/update":
            self.handle_episode_config_update()
            return

        if request_path == "/grid/update":
            self.handle_grid_update()
            return

        if request_path == "/policy/spec/update":
            self.handle_policy_spec_update()
            return

        if request_path == "/policy/catalog/source":
            self.handle_policy_catalog_source_update()
            return

        if request_path == "/policy/action":
            self.handle_policy_action()
            return

        self.send_json(404, {"status": "error", "message": "unknown endpoint"})

    def handle_episode_start(self) -> None:
        try:
            episode_payload = self.read_json_body()
        except ValueError as error:
            self.send_json(400, {"status": "error", "message": str(error)})
            return

        grid_info = episode_payload.get("grid", {})
        config_info = build_config_info_from_payload(episode_payload)
        policy_spec_payload = episode_payload.get("policySpec", {})

        with get_server_grid_lock(self.server):
            self.server.episode_version = get_server_episode_version(self.server) + 1
            self.server.episode_info = build_episode_info_from_payload(episode_payload)
            self.server.policy_runtime_state = {}

            if isinstance(config_info, dict) and config_info:
                self.server.config_version = get_server_config_version(self.server) + 1
                self.server.config_info = config_info

            if isinstance(grid_info, dict) and grid_info:
                store_grid_info(self.server, grid_info)

            if isinstance(policy_spec_payload, dict) and policy_spec_payload:
                self.server.policy_spec = normalize_policy_spec(
                    policy_spec_payload,
                    get_server_policy_catalog(self.server),
                )
                self.server.policy_spec_received = True

            response = build_episode_status_response(self.server)

        print(
            "episode start "
            f"episodeVersion={response['episodeVersion']} "
            f"episodeId={response['episodeId']} "
            f"robot={response['robotInstanceId']} "
            f"configVersion={response['configVersion']} "
            f"gridVersion={response['gridVersion']} "
            f"gridReceived={response['gridReceived']} "
            f"policySpecReceived={response['policySpecReceived']} "
            f"enabledPolicies={response['enabledPolicyCount']}"
        )

        self.send_json(200, response)

    def handle_episode_config_update(self) -> None:
        try:
            config_payload = self.read_json_body()
        except ValueError as error:
            self.send_json(400, {"status": "error", "message": str(error)})
            return

        config_update = build_config_info_from_payload(config_payload)
        if not config_update:
            self.send_json(400, {"status": "error", "message": "config update has no config fields"})
            return

        with get_server_grid_lock(self.server):
            previous_config_info = get_server_config_info(self.server)
            self.server.config_info = merge_dict_recursive(previous_config_info, config_update)
            self.server.config_version = get_server_config_version(self.server) + 1
            response = build_episode_status_response(self.server)

        print(
            "episode config update "
            f"configVersion={response['configVersion']} "
            f"episodeId={response['episodeId']} "
            f"robot={response['robotInstanceId']}"
        )

        self.send_json(200, response)

    def handle_policy_catalog_source_update(self) -> None:
        try:
            source_payload = self.read_json_body()
        except ValueError as error:
            self.send_json(400, {"status": "error", "message": str(error)})
            return

        catalog_id = str(source_payload.get("catalogId", ""))
        if not catalog_id:
            self.send_json(400, {"status": "error", "message": "policy catalog source update has no catalogId"})
            return

        try:
            policy_catalog = load_policy_catalog_by_id(catalog_id)
        except (FileNotFoundError, OSError, ValueError, json.JSONDecodeError) as error:
            self.send_json(400, {"status": "error", "message": str(error), "catalogId": catalog_id})
            return

        active_catalog_id = str(policy_catalog.get("catalogId", catalog_id))

        with get_server_grid_lock(self.server):
            self.server.active_catalog_id = active_catalog_id
            self.server.policy_catalog = policy_catalog
            self.server.policy_spec = build_default_policy_spec(policy_catalog)
            self.server.policy_spec_received = False
            response = build_policy_catalog_response(self.server)

        print(
            "policy catalog source update "
            f"catalogId={active_catalog_id} "
            f"catalogVersion={response.get('catalogVersion', 0)} "
            f"policies={len(response.get('policies', [])) if isinstance(response.get('policies', []), list) else 0}"
        )

        self.send_json(200, response)

    def handle_policy_spec_update(self) -> None:
        try:
            policy_payload = self.read_json_body()
        except ValueError as error:
            self.send_json(400, {"status": "error", "message": str(error)})
            return

        policy_spec_payload = policy_payload.get("policySpec", policy_payload)
        if not isinstance(policy_spec_payload, dict) or not policy_spec_payload:
            self.send_json(400, {"status": "error", "message": "policy spec update has no policySpec"})
            return

        with get_server_grid_lock(self.server):
            self.server.policy_spec = normalize_policy_spec(
                policy_spec_payload,
                get_server_policy_catalog(self.server),
            )
            self.server.policy_spec_received = True
            response = build_policy_spec_response(self.server)

        print(
            "policy spec update "
            f"catalogVersion={response['policyCatalogVersion']} "
            f"enabledPolicies={response['enabledPolicyCount']}"
        )

        self.send_json(200, response)

    def handle_grid_update(self) -> None:
        try:
            grid_info = self.read_json_body()
        except ValueError as error:
            self.send_json(400, {"status": "error", "message": str(error)})
            return

        with get_server_grid_lock(self.server):
            response = store_grid_info(self.server, grid_info)

        print(
            "grid update "
            f"version={response['gridVersion']} "
            f"size={response['gridSizeX']}x{response['gridSizeY']} "
            f"cells={response['cellCount']} "
            f"walkable={response['walkableCount']} "
            f"penalty={response['penaltyCount']} "
            f"blocked={response['blockedCount']}"
        )

        self.send_json(200, response)

    def handle_policy_action(self) -> None:
        decision_start_time = time.perf_counter()
        try:
            observation = self.read_json_body()
        except ValueError as error:
            self.send_json(400, {"status": "error", "message": str(error)})
            return

        lidar_rays = observation.get("lidarRays", [])
        observed_objects = observation.get("observedObjects", [])
        robot_grid_debug = build_robot_grid_debug(self.server, observation)
        goal_grid_debug = build_episode_goal_debug(self.server, observation)
        if should_log_runtime_messages(self.server):
            print(
                "observation "
                f"sequence={observation.get('sequence', 0)} "
                f"sensorSequence={observation.get('sensorSequence', 0)} "
                f"policyMode={getattr(self.server, 'policy_mode', 'forward')} "
                f"rays={len(lidar_rays) if isinstance(lidar_rays, list) else 0} "
                f"objects={len(observed_objects) if isinstance(observed_objects, list) else 0} "
                f"robotGridStatus={robot_grid_debug.get('robotGridStatus', 'unknown')} "
                f"robotGrid=({robot_grid_debug.get('robotGridX', '-')},{robot_grid_debug.get('robotGridY', '-')}) "
                f"robotCell={robot_grid_debug.get('robotCellAreaType', '-')} "
                f"goalGridStatus={goal_grid_debug.get('goalGridStatus', 'unknown')} "
                f"goalGrid=({goal_grid_debug.get('goalGridX', '-')},{goal_grid_debug.get('goalGridY', '-')})"
            )
            log_nearest_observed_object(observation)

        response_delay_second = getattr(self.server, "response_delay_second", 0.0)
        if response_delay_second > 0.0:
            time.sleep(response_delay_second)

        policy_mode = getattr(self.server, "policy_mode", "forward")
        grid_status = build_grid_status_response(self.server)
        enriched_observation = enrich_observation_with_server_config(self.server, observation)

        if policy_mode == "runtime" or has_runtime_policy_spec(self.server):
            runtime_context = build_runtime_policy_context(self.server, enriched_observation)
            response = build_runtime_policy_response(runtime_context)
        else:
            response = build_policy_response(enriched_observation, policy_mode)
            apply_version_mismatch_policy_mode(response, policy_mode)

        response["gridVersion"] = grid_status["gridVersion"]
        response["gridReceived"] = grid_status["gridReceived"]
        response["episodeVersion"] = get_server_episode_version(self.server)
        response["configVersion"] = get_server_config_version(self.server)
        response["debug"].update(robot_grid_debug)
        response["debug"].update(goal_grid_debug)
        response["debug"]["decisionElapsedMs"] = (time.perf_counter() - decision_start_time) * 1000.0
        if should_log_runtime_messages(self.server):
            log_selected_policy_action(response)
        self.send_json(200, response)

    def read_json_body(self) -> dict[str, Any]:
        content_length = int(self.headers.get("Content-Length", "0") or 0)
        if content_length <= 0:
            raise ValueError("empty request body")

        raw_body = self.rfile.read(content_length)
        try:
            body = json.loads(raw_body.decode("utf-8"))
        except json.JSONDecodeError as error:
            raise ValueError(f"invalid json: {error.msg}") from error

        if not isinstance(body, dict):
            raise ValueError("request body must be a json object")

        return body

    def send_json(self, status_code: int, payload: dict[str, Any]) -> None:
        response_body = json.dumps(payload, ensure_ascii=False).encode("utf-8")

        try:
            self.send_response(status_code)
            self.send_header("Content-Type", "application/json; charset=utf-8")
            self.send_header("Content-Length", str(len(response_body)))
            self.end_headers()
            self.wfile.write(response_body)
        except CLIENT_DISCONNECT_EXCEPTIONS as error:
            print(
                f"{self.client_address[0]} - client disconnected before response "
                f"({error.__class__.__name__})"
            )

    def log_message(self, format: str, *args: Any) -> None:
        if should_log_runtime_messages(self.server):
            print(f"{self.client_address[0]} - {format % args}")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="DeliveryBot HTTP policy server")
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=8000)
    parser.add_argument(
        "--response-delay-second",
        type=float,
        default=0.0,
        help="Artificial response delay for Unreal HTTP in-flight request tests.",
    )
    parser.add_argument(
        "--policy-mode",
        choices=POLICY_MODE_CHOICES,
        default="runtime",
        help="Policy response mode for success and validation failure tests.",
    )
    parser.add_argument(
        "--planner-mode",
        choices=PLANNER_MODE_CHOICES,
        default="auto",
        help="Default global planner: astar, hybrid-astar, or auto. PolicySpec planner fields override this.",
    )
    parser.add_argument(
        "--dwa-mode",
        choices=DWA_MODE_CHOICES,
        default="policy",
        help="DWA local avoidance mode. on forces DWA policy on; off removes DWA policy; policy follows PolicySpec.",
    )
    parser.add_argument(
        "--right-of-way-mode",
        choices=RIGHT_OF_WAY_MODE_CHOICES,
        default="policy",
        help="Default yielding behavior. pedestrian waits for people before reroute; robot reroutes immediately.",
    )
    parser.add_argument(
        "--policy-spec-file",
        default="",
        help="Optional startup PolicySpec JSON path or name under Json/Input/PolicySpecs.",
    )
    parser.add_argument(
        "--verbose-runtime-log",
        action="store_true",
        help="Print per-action observation, selected policy, and HTTP access logs.",
    )
    return parser.parse_args()


def main() -> None:
    args = parse_args()
    policy_catalog = load_policy_catalog()
    server = ThreadingHTTPServer((args.host, args.port), DeliveryBotPolicyHandler)
    server.response_delay_second = max(args.response_delay_second, 0.0)
    server.policy_mode = args.policy_mode
    server.planner_mode = normalize_cli_planner_mode(args.planner_mode)
    server.dwa_mode = normalize_cli_dwa_mode(args.dwa_mode)
    server.right_of_way_mode = args.right_of_way_mode
    server.verbose_runtime_log = bool(args.verbose_runtime_log)
    server.active_catalog_id = str(policy_catalog.get("catalogId", ""))
    server.policy_catalog = policy_catalog
    startup_policy_spec = load_startup_policy_spec(args.policy_spec_file)
    if startup_policy_spec:
        server.policy_spec = normalize_policy_spec(startup_policy_spec, policy_catalog)
        server.policy_spec_received = True
    else:
        server.policy_spec = build_default_policy_spec(policy_catalog)
        server.policy_spec_received = False
    server.episode_info = {}
    server.episode_version = 0
    server.config_info = {}
    server.config_version = 0
    server.grid_info = None
    server.grid_summary = {}
    server.grid_cell_lookup = {}
    server.grid_version = 0
    server.grid_lock = threading.RLock()
    print(f"DeliveryBot policy server listening on http://{args.host}:{args.port}")
    print("POST /episode/start")
    print("POST /episode/config/update")
    print("POST /policy/action")
    print("POST /policy/catalog/source")
    print("POST /policy/spec/update")
    print("POST /grid/update")
    print("GET  /episode/status")
    print("GET  /grid/status")
    print("GET  /policy/catalog/sources")
    print("GET  /policy/catalog")
    print("GET  /policy/spec/status")
    print("GET  /health")
    print(f"policy mode: {server.policy_mode}")
    print(f"planner mode: {server.planner_mode}")
    print(f"dwa mode: {server.dwa_mode}")
    print(f"right-of-way mode: {server.right_of_way_mode}")
    print(
        "policy catalog: "
        f"activeCatalogId={server.active_catalog_id} "
        f"version={policy_catalog.get('catalogVersion', 0)} "
        f"policies={len(policy_catalog.get('policies', [])) if isinstance(policy_catalog.get('policies', []), list) else 0}"
    )
    if startup_policy_spec:
        print(f"startup policy spec: enabledPolicies={len(server.policy_spec.get('enabledPolicies', []))}")
    if server.response_delay_second > 0.0:
        print(f"response delay: {server.response_delay_second:.3f}s")
    server.serve_forever()


def normalize_cli_planner_mode(value: str) -> str:
    planner_mode = str(value or "auto").strip().lower().replace("-", "_")
    return "hybrid_astar" if planner_mode == "hybrid_astar" else planner_mode


def normalize_cli_dwa_mode(value: str) -> str:
    dwa_mode = str(value or "policy").strip().lower()
    return dwa_mode if dwa_mode in DWA_MODE_CHOICES else "policy"


def load_startup_policy_spec(policy_spec_file: str) -> dict[str, Any]:
    trimmed = str(policy_spec_file or "").strip()
    if not trimmed:
        return {}

    for candidate in iter_policy_spec_candidates(trimmed):
        if not candidate.exists() or not candidate.is_file():
            continue

        with candidate.open("r", encoding="utf-8") as file:
            payload = json.load(file)

        policy_spec = payload.get("policySpec", payload) if isinstance(payload, dict) else {}
        if isinstance(policy_spec, dict):
            return policy_spec

    raise FileNotFoundError(f"policy spec file not found: {policy_spec_file}")


def iter_policy_spec_candidates(value: str) -> list[Path]:
    raw_path = Path(value)
    names = [value]
    if raw_path.suffix.lower() != ".json":
        names.append(f"{value}.json")

    candidates: list[Path] = []
    for name in names:
        path = Path(name)
        candidates.append(path)
        candidates.append(Path.cwd() / path)
        candidates.append(Path.cwd() / "Json" / "Input" / "PolicySpecs" / path.name)

    return candidates


if __name__ == "__main__":
    main()
