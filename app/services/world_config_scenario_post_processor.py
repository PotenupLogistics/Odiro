from __future__ import annotations

from copy import deepcopy
from typing import Any

from app.models.environment import EnvironmentSamplingContext
from app.models.scenario import (
    ScenarioIntent,
    ScenarioPostProcessPatch,
    ScenarioPostProcessResult,
)
from app.services.route_geometry_utils import compute_midpoint, is_point_near_route_midpoint
from app.services.world_config_scenario_intent_extractor import extract_scenario_intent


NARROW_SIDEWALK_WIDTH_CM = 120.0
PATH_BLOCKING_RATIO = 0.6


def _location(value: Any) -> dict[str, float]:
    if not isinstance(value, dict):
        return {"x": 0.0, "y": 0.0, "z": 0.0}
    return {
        "x": float(value.get("x", 0) or 0),
        "y": float(value.get("y", 0) or 0),
        "z": float(value.get("z", 0) or 0),
    }


def _robot_path(payload: dict[str, Any]) -> tuple[dict[str, float], dict[str, float]]:
    robot = payload.get("robot") if isinstance(payload.get("robot"), dict) else {}
    return _location(robot.get("spawn")), _location(robot.get("goal"))


def _path_midpoint(payload: dict[str, Any]) -> dict[str, float]:
    spawn, goal = _robot_path(payload)
    return compute_midpoint(spawn, goal)


def _crossing_endpoints(payload: dict[str, Any]) -> tuple[dict[str, float], dict[str, float]]:
    midpoint = _path_midpoint(payload)
    return (
        {"x": midpoint["x"], "y": midpoint["y"] - 200.0, "z": midpoint["z"]},
        {"x": midpoint["x"], "y": midpoint["y"] + 200.0, "z": midpoint["z"]},
    )


def _append_patch(
    patches: list[ScenarioPostProcessPatch],
    patch_type: str,
    target_path: str,
    before_value: Any,
    after_value: Any,
    reason: str,
) -> None:
    patches.append(
        ScenarioPostProcessPatch(
            patchId=f"PATCH-{len(patches) + 1:03d}",
            patchType=patch_type,
            targetPath=target_path,
            beforeValue=before_value,
            afterValue=after_value,
            reason=reason,
        )
    )


def _items(payload: dict[str, Any], key: str) -> list[dict[str, Any]]:
    values = payload.setdefault(key, [])
    if not isinstance(values, list):
        payload[key] = []
        return payload[key]
    return values


def _find_type(items: list[dict[str, Any]], object_type: str) -> dict[str, Any] | None:
    for item in items:
        if isinstance(item, dict) and str(item.get("type", "")).lower() == object_type.lower():
            return item
    return None


def _has_hint(values: list[str], expected: str) -> bool:
    return any(value.lower() == expected.lower() for value in values)


def _first_obstacle(obstacles: list[dict[str, Any]]) -> dict[str, Any] | None:
    return next((item for item in obstacles if isinstance(item, dict)), None)


def _world_obstacle_type(intent: ScenarioIntent) -> str:
    return intent.obstacleType if intent.obstacleType and intent.obstacleType != "static_obstacle" else "Obstacle"


def apply_scenario_intent_to_world_config(
    prompt: str,
    payload: dict,
    environment_context: EnvironmentSamplingContext | None = None,
) -> ScenarioPostProcessResult:
    return apply_scenario_intent_to_world_config_from_intent(
        extract_scenario_intent(prompt),
        payload,
        environment_context=environment_context,
    )


def apply_scenario_intent_to_world_config_from_intent(
    intent: ScenarioIntent,
    payload: dict,
    environment_context: EnvironmentSamplingContext | None = None,
) -> ScenarioPostProcessResult:
    patched = deepcopy(payload)
    patches: list[ScenarioPostProcessPatch] = []
    warnings: list[str] = []

    if "narrow_sidewalk" in intent.mapHints:
        map_config = patched.setdefault("map", {})
        width = map_config.get("sidewalkWidthCm")
        target_width = intent.sidewalkWidthCm if intent.sidewalkWidthCm is not None else NARROW_SIDEWALK_WIDTH_CM
        should_patch_width = (
            intent.sidewalkWidthCm is not None and width != target_width
        ) or (
            intent.sidewalkWidthCm is None
            and (not isinstance(width, (int, float)) or width <= 0 or width > 250)
        )
        if should_patch_width:
            map_config["sidewalkWidthCm"] = target_width
            _append_patch(
                patches,
                "set_sidewalk_width_from_prompt" if intent.sidewalkWidthCm is not None else "set_narrow_sidewalk_width",
                "map.sidewalkWidthCm",
                width,
                target_width,
                "User prompt includes an explicit sidewalk width." if intent.sidewalkWidthCm is not None else "User prompt includes a narrow sidewalk condition.",
            )

    obstacles = _items(patched, "obstacles")
    environment_objects = _items(patched, "environmentObjects")
    has_kickboard = _find_type(obstacles, "Kickboard") or _find_type(environment_objects, "Kickboard")

    if _has_hint(intent.obstacleHints, "Kickboard") and has_kickboard is None:
        kickboard = {
            "objectId": "kickboard_001",
            "type": "Kickboard",
            "position": _path_midpoint(patched),
            "blockingRatio": 0.0,
        }
        obstacles.append(kickboard)
        has_kickboard = kickboard
        _append_patch(
            patches,
            "add_kickboard_obstacle",
            "obstacles[]",
            None,
            kickboard,
            "User prompt includes a Kickboard obstacle.",
        )

    target_obstacle = has_kickboard if isinstance(has_kickboard, dict) else _first_obstacle(obstacles)
    route_midpoint = _path_midpoint(patched)
    if target_obstacle is None and (_has_hint(intent.obstacleHints, "Obstacle") or intent.pathBlockingHints):
        target_obstacle = {
            "objectId": "obstacle_001",
            "type": _world_obstacle_type(intent),
            "position": intent.obstaclePositionHint or route_midpoint,
            "blockingRatio": intent.obstacleBlockingRatio if intent.obstacleBlockingRatio is not None else PATH_BLOCKING_RATIO,
        }
        obstacles.append(target_obstacle)
        _append_patch(
            patches,
            "add_generic_obstacle_at_route_midpoint"
            if intent.obstaclePlacementHint == "route_midpoint" and intent.obstaclePositionHint is None
            else "add_generic_obstacle",
            "obstacles[]",
            None,
            target_obstacle,
            "User prompt includes a generic/static obstacle or path blocking condition.",
        )

    if target_obstacle is not None and intent.obstaclePositionHint is not None:
        current_position = target_obstacle.get("position")
        if current_position != intent.obstaclePositionHint:
            target_obstacle["position"] = intent.obstaclePositionHint
            _append_patch(
                patches,
                "set_obstacle_position_from_prompt",
                f"obstacles[{target_obstacle.get('objectId', 'unknown')}].position",
                current_position,
                intent.obstaclePositionHint,
                "User prompt includes an explicit obstacle position.",
            )

    if (
        target_obstacle is not None
        and intent.obstaclePlacementHint == "route_midpoint"
        and intent.obstaclePositionHint is None
    ):
        current_position = target_obstacle.get("position")
        spawn, goal = _robot_path(patched)
        if not isinstance(current_position, dict) or not is_point_near_route_midpoint(current_position, spawn, goal):
            target_obstacle["position"] = route_midpoint
            _append_patch(
                patches,
                "set_obstacle_position_to_route_midpoint",
                f"obstacles[{target_obstacle.get('objectId', 'unknown')}].position",
                current_position,
                route_midpoint,
                "User prompt places the obstacle near the route midpoint.",
            )

    if intent.pathBlockingHints:
        if target_obstacle is not None:
            current_ratio = target_obstacle.get("blockingRatio")
            target_ratio = intent.obstacleBlockingRatio if intent.obstacleBlockingRatio is not None else PATH_BLOCKING_RATIO
            should_patch_ratio = (
                intent.obstacleBlockingRatio is not None and current_ratio != target_ratio
            ) or (
                intent.obstacleBlockingRatio is None
                and (not isinstance(current_ratio, (int, float)) or current_ratio <= 0)
            )
            if should_patch_ratio:
                target_obstacle["blockingRatio"] = target_ratio
                object_id = target_obstacle.get("objectId", "unknown")
                _append_patch(
                    patches,
                    "set_obstacle_blocking_ratio_from_prompt" if intent.obstacleBlockingRatio is not None else "set_obstacle_blocking_ratio",
                    f"obstacles[{object_id}].blockingRatio",
                    current_ratio,
                    target_ratio,
                    "User prompt includes an explicit blockingRatio." if intent.obstacleBlockingRatio is not None else "User prompt says the robot path is blocked.",
                )

    pedestrians = _items(patched, "pedestrians")
    if intent.explicitNoPedestrian and pedestrians:
        before = list(pedestrians)
        patched["pedestrians"] = []
        pedestrians = patched["pedestrians"]
        _append_patch(
            patches,
            "remove_pedestrians_for_no_pedestrian_prompt",
            "pedestrians[]",
            before,
            [],
            "User prompt explicitly says there are no pedestrians.",
        )

    if _has_hint(intent.pedestrianHints, "Pedestrian") and not intent.explicitNoPedestrian and not pedestrians:
        spawn, goal = _crossing_endpoints(patched)
        pedestrian = {
            "objectId": "pedestrian_001",
            "spawn": spawn,
            "goal": goal,
            "speedKmh": 3.0,
            "behavior": "Crossing" if "pedestrian_crossing" in intent.crossingHints else "Walking",
        }
        pedestrians.append(pedestrian)
        _append_patch(
            patches,
            "add_crossing_pedestrian",
            "pedestrians[]",
            None,
            pedestrian,
            "User prompt includes a pedestrian crossing or pedestrian presence.",
        )

    if "pedestrian_crossing" in intent.crossingHints and not intent.explicitNoPedestrian:
        crossing = next(
            (
                pedestrian
                for pedestrian in pedestrians
                if isinstance(pedestrian, dict)
                and "cross" in str(pedestrian.get("behavior", "")).lower()
            ),
            None,
        )
        if crossing is None and pedestrians:
            pedestrian = next((item for item in pedestrians if isinstance(item, dict)), None)
            if pedestrian is not None:
                before = pedestrian.get("behavior")
                pedestrian["behavior"] = "Crossing"
                _append_patch(
                    patches,
                    "set_pedestrian_crossing_behavior",
                    f"pedestrians[{pedestrian.get('objectId', 'unknown')}].behavior",
                    before,
                    "Crossing",
                    "User prompt says the pedestrian is crossing.",
                )

    if "crosswalk" in intent.crossingHints:
        warnings.append(
            "Crosswalk context was requested, but current world_config schema has no dedicated crosswalk context field."
        )

    if environment_context is not None and environment_context.parameters is not None:
        parameters = environment_context.parameters
        map_config = patched.setdefault("map", {})
        width = map_config.get("sidewalkWidthCm")
        if intent.sidewalkWidthCm is None and width != parameters.sidewalkWidthCm:
            map_config["sidewalkWidthCm"] = parameters.sidewalkWidthCm
            _append_patch(
                patches,
                "set_sidewalk_width_from_environment_sampler",
                "map.sidewalkWidthCm",
                width,
                parameters.sidewalkWidthCm,
                "Environment sampler numeric constraints override vague map width values.",
            )

        runtime = patched.setdefault("runtime", {})
        duration = runtime.get("maxDurationSec")
        if duration != parameters.timeLimitSec:
            runtime["maxDurationSec"] = parameters.timeLimitSec
            _append_patch(
                patches,
                "set_runtime_limit_from_environment_sampler",
                "runtime.maxDurationSec",
                duration,
                parameters.timeLimitSec,
                "Environment sampler numeric constraints set runtime duration.",
            )

        if not obstacles and parameters.obstacleBlockingRatio > 0:
            obstacle = {
                "objectId": "obstacle_001",
                "type": "Obstacle",
                "position": _path_midpoint(patched),
                "blockingRatio": parameters.obstacleBlockingRatio,
            }
            obstacles.append(obstacle)
            _append_patch(
                patches,
                "apply_environment_sampling_parameters",
                "obstacles[]",
                None,
                obstacle,
                "Environment sampler requires an obstacle blocking ratio.",
            )

        obstacle_for_ratio = _first_obstacle(obstacles)
        if obstacle_for_ratio is not None:
            current_ratio = obstacle_for_ratio.get("blockingRatio")
            if current_ratio != parameters.obstacleBlockingRatio:
                obstacle_for_ratio["blockingRatio"] = parameters.obstacleBlockingRatio
                _append_patch(
                    patches,
                    "set_obstacle_blocking_ratio_from_environment_sampler",
                    f"obstacles[{obstacle_for_ratio.get('objectId', 'unknown')}].blockingRatio",
                    current_ratio,
                    parameters.obstacleBlockingRatio,
                    "Environment sampler numeric constraints set obstacle blocking ratio.",
                )

        pedestrians = _items(patched, "pedestrians")
        if parameters.pedestrianCount == 0 and pedestrians:
            before = list(pedestrians)
            patched["pedestrians"] = []
            _append_patch(
                patches,
                "apply_environment_sampling_parameters",
                "pedestrians[]",
                before,
                [],
                "Environment sampler requires zero pedestrians.",
            )
        elif parameters.pedestrianCount > 0 and "pedestrian_crossing" in intent.crossingHints and not pedestrians:
            spawn, goal = _crossing_endpoints(patched)
            pedestrian = {
                "objectId": "pedestrian_001",
                "spawn": spawn,
                "goal": goal,
                "speedKmh": round(parameters.pedestrianSpeedMps * 3.6, 3),
                "behavior": "Crossing",
            }
            pedestrians.append(pedestrian)
            _append_patch(
                patches,
                "apply_environment_sampling_parameters",
                "pedestrians[]",
                None,
                pedestrian,
                "Environment sampler provides pedestrian count and speed for crossing scenario.",
            )

        for pedestrian in _items(patched, "pedestrians"):
            if not isinstance(pedestrian, dict):
                continue
            current_speed = pedestrian.get("speedKmh")
            target_speed = round(parameters.pedestrianSpeedMps * 3.6, 3)
            if current_speed != target_speed:
                pedestrian["speedKmh"] = target_speed
                _append_patch(
                    patches,
                    "set_pedestrian_speed_from_environment_sampler",
                    f"pedestrians[{pedestrian.get('objectId', 'unknown')}].speedKmh",
                    current_speed,
                    target_speed,
                    "Environment sampler numeric constraints set pedestrian speed.",
                )

    return ScenarioPostProcessResult(
        applied=bool(patches),
        patches=patches,
        patchedPayload=patched,
        warnings=warnings,
    )
