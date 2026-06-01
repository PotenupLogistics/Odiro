from __future__ import annotations

from copy import deepcopy
from typing import Any

from app.models.scenario import (
    ScenarioIntent,
    ScenarioPostProcessPatch,
    ScenarioPostProcessResult,
)
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
    return {
        "x": round((spawn["x"] + goal["x"]) / 2, 3),
        "y": round((spawn["y"] + goal["y"]) / 2, 3),
        "z": spawn["z"],
    }


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


def apply_scenario_intent_to_world_config(prompt: str, payload: dict) -> ScenarioPostProcessResult:
    return apply_scenario_intent_to_world_config_from_intent(
        extract_scenario_intent(prompt),
        payload,
    )


def apply_scenario_intent_to_world_config_from_intent(
    intent: ScenarioIntent,
    payload: dict,
) -> ScenarioPostProcessResult:
    patched = deepcopy(payload)
    patches: list[ScenarioPostProcessPatch] = []
    warnings: list[str] = []

    if "narrow_sidewalk" in intent.mapHints:
        map_config = patched.setdefault("map", {})
        width = map_config.get("sidewalkWidthCm")
        if not isinstance(width, (int, float)) or width <= 0 or width > 250:
            map_config["sidewalkWidthCm"] = NARROW_SIDEWALK_WIDTH_CM
            _append_patch(
                patches,
                "set_narrow_sidewalk_width",
                "map.sidewalkWidthCm",
                width,
                NARROW_SIDEWALK_WIDTH_CM,
                "User prompt includes a narrow sidewalk condition.",
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

    if intent.pathBlockingHints:
        target_obstacle = has_kickboard if isinstance(has_kickboard, dict) else None
        if target_obstacle is None and obstacles:
            target_obstacle = next((item for item in obstacles if isinstance(item, dict)), None)
        if target_obstacle is not None:
            current_ratio = target_obstacle.get("blockingRatio")
            if not isinstance(current_ratio, (int, float)) or current_ratio <= 0:
                target_obstacle["blockingRatio"] = PATH_BLOCKING_RATIO
                object_id = target_obstacle.get("objectId", "unknown")
                _append_patch(
                    patches,
                    "set_obstacle_blocking_ratio",
                    f"obstacles[{object_id}].blockingRatio",
                    current_ratio,
                    PATH_BLOCKING_RATIO,
                    "User prompt says the robot path is blocked.",
                )

    pedestrians = _items(patched, "pedestrians")
    if _has_hint(intent.pedestrianHints, "Pedestrian") and not pedestrians:
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

    if "pedestrian_crossing" in intent.crossingHints:
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

    return ScenarioPostProcessResult(
        applied=bool(patches),
        patches=patches,
        patchedPayload=patched,
        warnings=warnings,
    )
