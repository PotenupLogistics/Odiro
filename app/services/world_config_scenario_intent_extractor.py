from __future__ import annotations

import re

from app.models.scenario import ScenarioIntent, ScenarioRequirement
from app.services.natural_language_normalizer import normalize_prompt


def _append_unique(items: list[str], values: list[str]) -> None:
    for value in values:
        if value not in items:
            items.append(value)


def _contains_any(text: str, keywords: tuple[str, ...]) -> bool:
    return any(keyword in text for keyword in keywords)


def _extract_sidewalk_width_cm(text: str) -> float | None:
    match = re.search(r"보도\s*폭(?:은|이)?\s*(?:약\s*)?(\d+(?:\.\d+)?)\s*cm", text, re.IGNORECASE)
    return float(match.group(1)) if match else None


def _extract_blocking_ratio(text: str) -> float | None:
    match = re.search(r"blocking\s*ratio\s*(?:는|은|=|:)?\s*(\d+(?:\.\d+)?)", text, re.IGNORECASE)
    if not match:
        match = re.search(r"blockingratio\s*(?:는|은|=|:)?\s*(\d+(?:\.\d+)?)", text, re.IGNORECASE)
    if not match:
        return None
    return float(match.group(1))


def _extract_xyz_near_obstacle(text: str) -> dict[str, float] | None:
    pattern = re.compile(
        r"x\s*=\s*(-?\d+(?:\.\d+)?)\s*,?\s*y\s*=\s*(-?\d+(?:\.\d+)?)\s*,?\s*z\s*=\s*(-?\d+(?:\.\d+)?)",
        re.IGNORECASE,
    )
    matches = list(pattern.finditer(text))
    if not matches:
        return None
    for match in matches:
        after = text[match.end() : min(len(text), match.end() + 36)]
        immediate_after = text[match.end() : min(len(text), match.end() + 16)]
        if _contains_any(immediate_after, ("출발", "이동", "이동한다")):
            continue
        if _contains_any(after, ("근처", "정적 장애물", "배치")):
            return {"x": float(match.group(1)), "y": float(match.group(2)), "z": float(match.group(3))}
    obstacle_keywords = ("장애물", "경로 중앙", "로봇 경로 중앙", "경로를 막", "blocking")
    scored_matches: list[tuple[int, re.Match[str]]] = []
    for match in matches:
        window = text[max(0, match.start() - 80) : min(len(text), match.end() + 80)]
        score = 0
        if _contains_any(window, obstacle_keywords):
            score += 2
        if _contains_any(window, ("근처", "배치", "정적 장애물")):
            score += 3
        immediate_after = text[match.end() : min(len(text), match.end() + 16)]
        if _contains_any(immediate_after, ("출발", "이동", "이동한다")):
            score -= 8
        elif _contains_any(window, ("출발", "이동한다", "이동한다.")):
            score -= 2
        scored_matches.append((score, match))
    best_score, match = max(scored_matches, key=lambda item: item[0])
    if best_score > 0:
        return {"x": float(match.group(1)), "y": float(match.group(2)), "z": float(match.group(3))}
    if len(matches) >= 3:
        match = matches[2]
        return {"x": float(match.group(1)), "y": float(match.group(2)), "z": float(match.group(3))}
    if len(matches) == 1 and _contains_any(text, obstacle_keywords):
        match = matches[0]
        return {"x": float(match.group(1)), "y": float(match.group(2)), "z": float(match.group(3))}
    return None


def _explicit_no_pedestrian(text: str) -> bool:
    return _contains_any(
        text,
        (
            "보행자는 없는",
            "보행자 없는",
            "보행자 없음",
            "보행자는 없음",
            "보행자는 없",
            "보행자가 없는",
            "사람 없는",
        ),
    )


def _route_midpoint_intent(text: str) -> bool:
    return _contains_any(
        text,
        (
            "경로 중앙",
            "경로 중간",
            "로봇 경로 중앙",
            "로봇 경로 중간",
            "중앙 근처",
            "경로 중앙 근처",
            "path center",
            "middle of the path",
        ),
    )


def extract_scenario_intent(prompt: str) -> ScenarioIntent:
    text = normalize_prompt(prompt).lower()
    intent = ScenarioIntent()
    intent.sidewalkWidthCm = _extract_sidewalk_width_cm(text)
    intent.obstacleBlockingRatio = _extract_blocking_ratio(text)
    intent.obstaclePositionHint = _extract_xyz_near_obstacle(text)
    if _route_midpoint_intent(text):
        intent.obstaclePlacementHint = "route_midpoint"
    intent.explicitNoPedestrian = _explicit_no_pedestrian(text)

    if _contains_any(text, ("좁은 보도", "좁은 길", "보도 폭", "보도")):
        _append_unique(intent.mapHints, ["narrow_sidewalk"])
        _append_unique(intent.suggestedCategories, ["sidewalk_operation", "speed_policy"])
        _append_unique(intent.suggestedPolicyParams, ["sidewalkWidthCm", "maxSpeedKmh"])

    if _contains_any(text, ("킥보드", "공유 킥보드", "전동킥보드")):
        _append_unique(intent.obstacleHints, ["Kickboard"])
        _append_unique(intent.suggestedCategories, ["perception_requirement"])
        _append_unique(intent.suggestedActions, ["SlowDown", "Stop", "LocalAvoidance", "ReplanPath"])
        _append_unique(intent.suggestedPolicyParams, ["safeDistanceCm", "perceptionMinRangeM"])

    if _contains_any(text, ("장애물", "정적 장애물", "로봇 경로 중앙", "경로 중앙", "막고", "막힘", "경로를 막", "막는 정도", "blockingratio", "blocking ratio", "차단")):
        intent.pathBlockingHints = True
        _append_unique(intent.obstacleHints, ["Obstacle"])
        _append_unique(intent.suggestedCategories, ["perception_requirement"])
        _append_unique(intent.suggestedActions, ["Stop", "LocalAvoidance", "ReplanPath"])
        _append_unique(intent.suggestedPolicyParams, ["perceptionMinRangeM", "safeDistanceCm"])

    if _contains_any(text, ("보행자", "사람")) and not intent.explicitNoPedestrian:
        _append_unique(intent.pedestrianHints, ["Pedestrian"])
        _append_unique(intent.suggestedCategories, ["sidewalk_operation", "perception_requirement"])
        _append_unique(intent.suggestedActions, ["SlowDown", "YieldWait", "Stop"])

    if _contains_any(text, ("횡단", "건너", "가로질러")) and not intent.explicitNoPedestrian:
        _append_unique(intent.crossingHints, ["pedestrian_crossing"])
        _append_unique(intent.suggestedCategories, ["sidewalk_operation", "crosswalk_operation"])
        _append_unique(intent.suggestedActions, ["YieldWait", "Stop", "Continue"])

    if _contains_any(text, ("횡단보도", "신호등")):
        _append_unique(intent.crossingHints, ["crosswalk"])
        _append_unique(intent.trafficSignalHints, ["traffic_signal"])
        _append_unique(intent.suggestedCategories, ["sidewalk_operation"])

    if _contains_any(text, ("경사", "턱", "넘어짐", "기울어짐")):
        _append_unique(intent.terrainHints, ["terrain_risk"])
        _append_unique(intent.suggestedCategories, ["terrain_or_dynamic_safety"])
        _append_unique(intent.suggestedActions, ["SlowDown", "Stop", "ReplanPath"])

    if _contains_any(text, ("관제", "원격", "수동")):
        _append_unique(intent.suggestedCategories, ["operator_control"])
        _append_unique(intent.suggestedActions, ["RequestOperator"])
        _append_unique(intent.suggestedPolicyParams, ["operatorOverrideEnabled"])

    return intent


def build_scenario_requirements(intent: ScenarioIntent) -> list[ScenarioRequirement]:
    requirements: list[ScenarioRequirement] = []

    if "narrow_sidewalk" in intent.mapHints:
        expected_width = (
            f"Must be exactly {intent.sidewalkWidthCm:g} when the user explicitly specifies sidewalk width."
            if intent.sidewalkWidthCm is not None
            else "Use a relatively narrow sidewalk width in cm."
        )
        requirements.append(
            ScenarioRequirement(
                requirementId="map_narrow_sidewalk",
                type="map",
                description="Represent the user's narrow sidewalk condition.",
                requiredInWorldConfig=True,
                expectedPath="map.sidewalkWidthCm",
                expectedValueHint=expected_width,
            )
        )

    if "Kickboard" in intent.obstacleHints:
        requirements.append(
            ScenarioRequirement(
                requirementId="obstacle_kickboard",
                type="obstacle",
                description="If the user mentions Kickboard, include at least one obstacle with type Kickboard.",
                requiredInWorldConfig=True,
                expectedPath="obstacles[].type",
                expectedValueHint="Kickboard",
            )
        )

    if "Obstacle" in intent.obstacleHints:
        requirements.append(
            ScenarioRequirement(
                requirementId="obstacle_generic",
                type="obstacle",
                description="If the user mentions a generic/static obstacle, include at least one obstacle.",
                requiredInWorldConfig=True,
                expectedPath="obstacles[].type",
                expectedValueHint="Obstacle or another schema-valid obstacle type.",
            )
        )

    if intent.obstaclePositionHint is not None:
        requirements.append(
            ScenarioRequirement(
                requirementId="obstacle_position",
                type="obstacle_position",
                description="Preserve the explicit obstacle position from the user prompt.",
                requiredInWorldConfig=True,
                expectedPath="obstacles[].position",
                expectedValueHint=(
                    f"x={intent.obstaclePositionHint['x']:g}, "
                    f"y={intent.obstaclePositionHint['y']:g}, "
                    f"z={intent.obstaclePositionHint['z']:g}"
                ),
            )
        )

    if intent.obstaclePlacementHint == "route_midpoint" and intent.obstaclePositionHint is None:
        requirements.append(
            ScenarioRequirement(
                requirementId="obstacle_route_midpoint",
                type="obstacle_position",
                description="Place the obstacle near the midpoint between robot.spawn and robot.goal.",
                requiredInWorldConfig=True,
                expectedPath="obstacles[].position",
                expectedValueHint="Use midpoint between robot.spawn and robot.goal",
            )
        )

    if intent.pathBlockingHints:
        expected_ratio = (
            f"Must be exactly {intent.obstacleBlockingRatio:g} when the user explicitly specifies blockingRatio."
            if intent.obstacleBlockingRatio is not None
            else "Use a positive blockingRatio, preferably 0.5 or higher."
        )
        requirements.append(
            ScenarioRequirement(
                requirementId="path_blocking_obstacle",
                type="path_blocking",
                description="If the user says the path is blocked, include a blocking obstacle near the robot path.",
                requiredInWorldConfig=True,
                expectedPath="obstacles[].blockingRatio",
                expectedValueHint=expected_ratio,
            )
        )

    if intent.explicitNoPedestrian:
        requirements.append(
            ScenarioRequirement(
                requirementId="no_pedestrians",
                type="pedestrian_absence",
                description="If the user says there are no pedestrians, do not generate pedestrian objects.",
                requiredInWorldConfig=True,
                expectedPath="pedestrians[]",
                expectedValueHint="Empty or absent pedestrians list.",
            )
        )

    if "Pedestrian" in intent.pedestrianHints:
        requirements.append(
            ScenarioRequirement(
                requirementId="pedestrian_present",
                type="pedestrian",
                description="Represent at least one pedestrian.",
                requiredInWorldConfig=True,
                expectedPath="pedestrians[]",
                expectedValueHint="At least one pedestrian object.",
            )
        )

    if "pedestrian_crossing" in intent.crossingHints:
        requirements.append(
            ScenarioRequirement(
                requirementId="pedestrian_crossing",
                type="crossing",
                description="If the user mentions pedestrian crossing, include a pedestrian with crossing behavior.",
                requiredInWorldConfig=True,
                expectedPath="pedestrians[].behavior",
                expectedValueHint="Crossing",
            )
        )

    if intent.terrainHints:
        requirements.append(
            ScenarioRequirement(
                requirementId="terrain_risk",
                type="terrain",
                description="Represent terrain risk with slope or curb-related map fields when mentioned.",
                requiredInWorldConfig=True,
                expectedPath="map.slopeDegree",
                expectedValueHint="Use non-zero slope when terrain risk is requested.",
            )
        )

    return requirements
