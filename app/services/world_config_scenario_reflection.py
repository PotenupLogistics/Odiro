from __future__ import annotations

from typing import Any

from app.models.scenario import (
    ScenarioReflectionIssue,
    ScenarioReflectionResult,
    ScenarioRequirement,
)
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


def _is_satisfied(requirement: ScenarioRequirement, payload: dict[str, Any]) -> bool:
    if requirement.requirementId == "map_narrow_sidewalk":
        width = (payload.get("map") or {}).get("sidewalkWidthCm")
        return isinstance(width, (int, float)) and width > 0 and width <= 250

    if requirement.requirementId == "obstacle_kickboard":
        return any(_string_contains(item.get("type"), "kickboard") for item in _obstacles(payload))

    if requirement.requirementId == "path_blocking_obstacle":
        return any(
            isinstance(item.get("blockingRatio"), (int, float)) and item.get("blockingRatio") > 0
            for item in _obstacles(payload)
        )

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
        "obstacle_kickboard": "missing_kickboard_obstacle",
        "path_blocking_obstacle": "missing_path_blocking_obstacle",
        "pedestrian_present": "missing_pedestrian",
        "pedestrian_crossing": "missing_pedestrian_crossing_behavior",
        "terrain_risk": "weak_scenario_binding",
    }.get(requirement_id, "weak_scenario_binding")


def _actual_value_summary(requirement: ScenarioRequirement, payload: dict[str, Any]) -> str:
    if requirement.requirementId == "map_narrow_sidewalk":
        return f"map.sidewalkWidthCm={((payload.get('map') or {}).get('sidewalkWidthCm'))!r}"
    if requirement.requirementId in {"obstacle_kickboard", "path_blocking_obstacle"}:
        return f"obstacles={_obstacles(payload)!r}"
    if requirement.requirementId in {"pedestrian_present", "pedestrian_crossing"}:
        return f"pedestrians={_pedestrians(payload)!r}"
    return "No matching value found."


def _repair_instruction(requirement: ScenarioRequirement) -> str:
    return {
        "map_narrow_sidewalk": "Keep map.sidewalkWidthCm in a narrow sidewalk range.",
        "obstacle_kickboard": 'Add an obstacle object with type "Kickboard".',
        "path_blocking_obstacle": "Set an obstacle blockingRatio greater than 0.",
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
