from __future__ import annotations

import re
from typing import Any

from app.models.scenario import (
    ScenarioReflectionIssue,
    ScenarioReflectionResult,
    ScenarioRequirement,
)
from app.services.route_geometry_utils import compute_midpoint, distance_between_points, is_point_near_route_midpoint
from app.services.world_config_scenario_intent_extractor import (
    build_scenario_requirements,
    extract_scenario_intent,
)


def _string_contains(value: Any, needle: str) -> bool:
    return needle.lower() in str(value or "").lower()


def _obstacles(payload: dict[str, Any]) -> list[dict[str, Any]]:
    values: list[dict[str, Any]] = []
    for key in ("obstacles", "environmentObjects"):
        for item in payload.get(key) or []:
            if isinstance(item, dict):
                values.append(item)
    return values


def _pedestrians(payload: dict[str, Any]) -> list[dict[str, Any]]:
    return [item for item in payload.get("pedestrians") or [] if isinstance(item, dict)]


def _number_hint(text: str) -> float | None:
    match = re.search(r"(-?\d+(?:\.\d+)?)", text)
    return float(match.group(1)) if match else None


def _position_hint(text: str) -> dict[str, float] | None:
    match = re.search(
        r"x\s*=\s*(-?\d+(?:\.\d+)?),\s*y\s*=\s*(-?\d+(?:\.\d+)?),\s*z\s*=\s*(-?\d+(?:\.\d+)?)",
        text,
    )
    if not match:
        return None
    return {"x": float(match.group(1)), "y": float(match.group(2)), "z": float(match.group(3))}


def _close(a: Any, b: float, tolerance: float = 0.001) -> bool:
    return isinstance(a, (int, float)) and abs(float(a) - b) <= tolerance


def _position_matches(position: Any, hint: dict[str, float], tolerance: float = 1.0) -> bool:
    return (
        isinstance(position, dict)
        and _close(position.get("x"), hint["x"], tolerance)
        and _close(position.get("y"), hint["y"], tolerance)
        and _close(position.get("z"), hint["z"], tolerance)
    )


def _robot_path(payload: dict[str, Any]) -> tuple[dict[str, Any], dict[str, Any]]:
    robot = payload.get("robot") if isinstance(payload.get("robot"), dict) else {}
    return robot.get("spawn") or {}, robot.get("goal") or {}


def _is_satisfied(requirement: ScenarioRequirement, payload: dict[str, Any]) -> bool:
    if requirement.requirementId == "map_narrow_sidewalk":
        width = (payload.get("map") or {}).get("sidewalkWidthCm")
        exact_width = _number_hint(requirement.expectedValueHint) if "exactly" in requirement.expectedValueHint else None
        if exact_width is not None:
            return _close(width, exact_width)
        return isinstance(width, (int, float)) and width > 0 and width <= 250

    if requirement.requirementId == "environment_sidewalk_width":
        width = (payload.get("map") or {}).get("sidewalkWidthCm")
        expected = _number_hint(requirement.expectedValueHint)
        return expected is not None and _close(width, expected)

    if requirement.requirementId == "obstacle_kickboard":
        return any(_string_contains(item.get("type"), "kickboard") for item in _obstacles(payload))

    if requirement.requirementId == "obstacle_generic":
        return any(
            _string_contains(item.get("type"), "obstacle")
            or _string_contains(item.get("type"), "kickboard")
            or bool(item.get("type"))
            for item in _obstacles(payload)
        )

    if requirement.requirementId == "obstacle_position":
        hint = _position_hint(requirement.expectedValueHint)
        return bool(hint) and any(_position_matches(item.get("position"), hint) for item in _obstacles(payload))

    if requirement.requirementId == "obstacle_route_midpoint":
        spawn, goal = _robot_path(payload)
        return any(
            isinstance(item.get("position"), dict)
            and is_point_near_route_midpoint(item["position"], spawn, goal)
            for item in _obstacles(payload)
        )

    if requirement.requirementId == "path_blocking_obstacle":
        exact_ratio = _number_hint(requirement.expectedValueHint) if "exactly" in requirement.expectedValueHint else None
        if exact_ratio is not None:
            return any(_close(item.get("blockingRatio"), exact_ratio) for item in _obstacles(payload))
        return any(
            isinstance(item.get("blockingRatio"), (int, float)) and item.get("blockingRatio") > 0
            for item in _obstacles(payload)
        )

    if requirement.requirementId == "environment_obstacle_blocking_ratio":
        expected = _number_hint(requirement.expectedValueHint)
        return expected is not None and any(
            _close(item.get("blockingRatio"), expected) for item in _obstacles(payload)
        )

    if requirement.requirementId == "environment_time_limit":
        expected = _number_hint(requirement.expectedValueHint)
        duration = (payload.get("runtime") or {}).get("maxDurationSec")
        return expected is not None and _close(duration, expected)

    if requirement.requirementId == "no_pedestrians":
        return not _pedestrians(payload)

    if requirement.requirementId == "pedestrian_present":
        return bool(_pedestrians(payload))

    if requirement.requirementId == "pedestrian_crossing":
        return any(
            _string_contains(item.get("behavior"), "cross")
            or _string_contains(item.get("behavior"), "횡단")
            for item in _pedestrians(payload)
        )

    if requirement.requirementId == "terrain_risk":
        slope = (payload.get("map") or {}).get("slopeDegree")
        return isinstance(slope, (int, float)) and slope != 0

    return False


def _issue_type(requirement_id: str) -> str:
    return {
        "map_narrow_sidewalk": "missing_narrow_sidewalk",
        "environment_sidewalk_width": "environment_sidewalk_width_mismatch",
        "obstacle_kickboard": "missing_kickboard_obstacle",
        "obstacle_generic": "missing_generic_obstacle",
        "obstacle_position": "missing_obstacle_position",
        "obstacle_route_midpoint": "obstacle_not_near_route_midpoint",
        "path_blocking_obstacle": "missing_path_blocking_obstacle",
        "environment_obstacle_blocking_ratio": "environment_obstacle_blocking_ratio_mismatch",
        "environment_time_limit": "environment_time_limit_mismatch",
        "no_pedestrians": "unexpected_pedestrians",
        "pedestrian_present": "missing_pedestrian",
        "pedestrian_crossing": "missing_pedestrian_crossing_behavior",
        "terrain_risk": "weak_scenario_binding",
    }.get(requirement_id, "weak_scenario_binding")


def _actual_value_summary(requirement: ScenarioRequirement, payload: dict[str, Any]) -> str:
    if requirement.requirementId == "map_narrow_sidewalk":
        return f"map.sidewalkWidthCm={((payload.get('map') or {}).get('sidewalkWidthCm'))!r}"
    if requirement.requirementId == "environment_sidewalk_width":
        return f"map.sidewalkWidthCm={((payload.get('map') or {}).get('sidewalkWidthCm'))!r}"
    if requirement.requirementId == "environment_time_limit":
        return f"runtime.maxDurationSec={((payload.get('runtime') or {}).get('maxDurationSec'))!r}"
    if requirement.requirementId in {"obstacle_kickboard", "obstacle_generic", "obstacle_position", "path_blocking_obstacle", "environment_obstacle_blocking_ratio"}:
        return f"obstacles={_obstacles(payload)!r}"
    if requirement.requirementId == "obstacle_route_midpoint":
        spawn, goal = _robot_path(payload)
        midpoint = compute_midpoint(spawn, goal)
        distances = [
            round(distance_between_points(item.get("position") or {}, midpoint), 3)
            for item in _obstacles(payload)
            if isinstance(item.get("position"), dict)
        ]
        return f"midpoint={midpoint!r}, obstacleDistancesCm={distances!r}, obstacles={_obstacles(payload)!r}"
    if requirement.requirementId in {"pedestrian_present", "pedestrian_crossing", "no_pedestrians"}:
        return f"pedestrians={_pedestrians(payload)!r}"
    return "No matching value found."


def _repair_instruction(requirement: ScenarioRequirement) -> str:
    return {
        "map_narrow_sidewalk": "Keep map.sidewalkWidthCm in a narrow sidewalk range.",
        "environment_sidewalk_width": "Set map.sidewalkWidthCm to the sampled numeric value.",
        "obstacle_kickboard": 'Add an obstacle object with type "Kickboard".',
        "obstacle_generic": 'Add an obstacle object with type "Obstacle".',
        "obstacle_position": "Place the obstacle at the explicit prompt coordinate.",
        "obstacle_route_midpoint": "Place the obstacle at the midpoint between robot.spawn and robot.goal.",
        "path_blocking_obstacle": "Set an obstacle blockingRatio greater than 0.",
        "environment_obstacle_blocking_ratio": "Set obstacles[].blockingRatio to the sampled numeric value.",
        "environment_time_limit": "Set runtime.maxDurationSec to the sampled numeric value.",
        "no_pedestrians": "Remove all pedestrian objects from pedestrians[].",
        "pedestrian_present": "Add at least one pedestrian object.",
        "pedestrian_crossing": 'Set at least one pedestrians[].behavior value to "Crossing".',
        "terrain_risk": "Represent terrain risk with schema-valid map fields.",
    }.get(requirement.requirementId, "Strengthen scenario binding using schema-valid fields.")


def validate_scenario_reflection(prompt: str, generated_payload: dict[str, Any]) -> ScenarioReflectionResult:
    requirements = build_scenario_requirements(extract_scenario_intent(prompt))
    missing: list[ScenarioRequirement] = []
    issues: list[ScenarioReflectionIssue] = []

    for requirement in requirements:
        if _is_satisfied(requirement, generated_payload):
            continue
        missing.append(requirement)
        issues.append(
            ScenarioReflectionIssue(
                requirementId=requirement.requirementId,
                issueType=_issue_type(requirement.requirementId),
                severity="error",
                message=f"Scenario requirement not represented: {requirement.description}",
                expectedPath=requirement.expectedPath,
                expectedValueHint=requirement.expectedValueHint,
                actualValueSummary=_actual_value_summary(requirement, generated_payload),
                repairInstruction=_repair_instruction(requirement),
            )
        )

    passed = not missing
    return ScenarioReflectionResult(
        passed=passed,
        checkedRequirements=requirements,
        missingRequirements=missing,
        partiallySatisfiedRequirements=[],
        issues=issues,
        summary="All scenario requirements are represented." if passed else f"{len(missing)} scenario requirements are missing.",
    )
