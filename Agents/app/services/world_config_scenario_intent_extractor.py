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
    if match:
        return float(match.group(1))
    match = re.search(r"보도\s*폭(?:은|이)?\s*(?:약\s*)?(\d+(?:\.\d+)?)\s*m", text, re.IGNORECASE)
    if match:
        return round(float(match.group(1)) * 100.0, 3)
    meter_patterns = (
        r"폭(?:은|이)?\s*(?:약\s*)?(\d+(?:\.\d+)?)\s*m\s*(?:인|의)?\s*(?:극단적으로\s*)?(?:좁은\s*)?(?:보도|sidewalk)",
        r"(\d+(?:\.\d+)?)\s*m\s*폭(?:의)?\s*(?:극단적으로\s*)?(?:좁은\s*)?(?:보도|sidewalk|통로)",
    )
    for pattern in meter_patterns:
        match = re.search(pattern, text, re.IGNORECASE)
        if match:
            return round(float(match.group(1)) * 100.0, 3)
    return None


def _extract_goal_distance_m(text: str) -> float | None:
    patterns = (
        r"출발(?:점|지)?(?:에서|으로부터)\s*(\d+(?:\.\d+)?)\s*m\s*(?:앞|전방|거리|떨어진)?\s*목적지",
        r"목적지(?:는|가)?\s*출발(?:점|지)?(?:에서|으로부터)\s*(\d+(?:\.\d+)?)\s*m",
        r"(?:로봇이\s*)?(\d+(?:\.\d+)?)\s*m\s*앞\s*목적지",
    )
    for pattern in patterns:
        match = re.search(pattern, text, re.IGNORECASE)
        if match:
            return float(match.group(1))
    return None


def _extract_count_near(text: str, keywords: tuple[str, ...], units: tuple[str, ...]) -> int | None:
    keyword_pattern = "|".join(re.escape(keyword) for keyword in keywords)
    unit_pattern = "|".join(re.escape(unit) for unit in units)
    patterns = (
        rf"(?:{keyword_pattern})[^\d]{{0,12}}(\d+)\s*(?:{unit_pattern})",
        rf"(\d+)\s*(?:{unit_pattern})[^\n.]{{0,12}}(?:{keyword_pattern})",
    )
    for pattern in patterns:
        match = re.search(pattern, text, re.IGNORECASE)
        if match:
            return int(match.group(1))
    return None


_OBSTACLE_TYPE_KEYWORDS: tuple[tuple[str, tuple[str, ...]], ...] = (
    ("trash_bin", ("쓰레기통", "trash can", "trash_can", "trash bin", "trash_bin", "garbage can", "trash", "bin")),
    ("traffic_cone", ("안전콘", "라바콘", "traffic cone", "traffic_cone", "road cone", "road_cone", "cone", "콘")),
    ("box", ("상자", "박스", "박스형", "box", "crate")),
    ("road_barrier", ("바리케이드", "차단막", "barrier", "road barrier", "road_barrier", "barricade")),
    ("manhole", ("맨홀", "manhole")),
    ("fire_hydrant", ("소화전", "hydrant", "fire hydrant")),
    ("mailbox", ("우편함", "mailbox", "mail box")),
    ("street_bank", ("벤치", "street bench", "street bank", "bench", "bank")),
)


def _mentioned_obstacle_types(text: str) -> list[str]:
    obstacle_types: list[str] = []
    for obstacle_type, keywords in _OBSTACLE_TYPE_KEYWORDS:
        if _contains_any(text, keywords):
            obstacle_types.append(obstacle_type)
    return obstacle_types


def _extract_obstacle_type(text: str) -> str | None:
    mentioned_types = _mentioned_obstacle_types(text)
    if mentioned_types:
        return mentioned_types[0]
    if _contains_any(text, ("정적 장애물", "static obstacle")):
        return "static_obstacle"
    return None


def _extract_obstacle_types(text: str) -> list[str]:
    type_patterns = (
        ("box", r"(?:박스형|박스|box)\s*(?:정적\s*)?장애물\s*(\d+)\s*개"),
        ("kickboard", r"(?:킥보드\s*형태|킥보드|전동킥보드)\s*(?:형태\s*)?(?:장애물\s*)?(\d+)\s*개"),
        ("traffic_cone", r"(?:라바콘|안전콘|콘|traffic cone|road cone|cone)\s*(?:장애물\s*)?(\d+)\s*개"),
    )
    obstacle_types: list[str] = []
    for obstacle_type, pattern in type_patterns:
        for match in re.finditer(pattern, text, re.IGNORECASE):
            obstacle_types.extend([obstacle_type] * int(match.group(1)))
    for obstacle_type in _mentioned_obstacle_types(text):
        if obstacle_type not in obstacle_types:
            obstacle_types.append(obstacle_type)
    return obstacle_types


def _extract_obstacle_positions_from_start_m(text: str) -> list[float]:
    match = re.search(r"((?:\d+(?:\.\d+)?\s*m\s*,?\s*)+)(?:지점|위치)", text, re.IGNORECASE)
    if match:
        window = text[max(0, match.start() - 80) : min(len(text), match.end() + 40)]
        if _contains_any(window, ("장애물", "정적 장애물", "놓여", "배치")):
            return [float(value) for value in re.findall(r"(\d+(?:\.\d+)?)\s*m", match.group(1), re.IGNORECASE)]

    positions: list[float] = []
    for match in re.finditer(r"(\d+(?:\.\d+)?)\s*m\s*지점", text, re.IGNORECASE):
        window = text[max(0, match.start() - 80) : min(len(text), match.end() + 40)]
        if _contains_any(window, ("장애물", "정적 장애물", "놓여", "배치")):
            positions.append(float(match.group(1)))
    return positions


def _extract_obstacle_lateral_position(text: str) -> str | None:
    if _contains_any(text, ("보도 중앙", "보도 가운데", "중앙")) and _contains_any(text, ("장애물", "정적 장애물")):
        return "center"
    return None


def _extract_pedestrian_direction(text: str) -> str | None:
    if _contains_any(text, ("가로질러", "횡단", "건너")):
        return "crossing"
    if _contains_any(text, ("같은 방향", "동일 방향")):
        return "same_direction"
    if _contains_any(text, ("반대편", "반대 방향", "맞은편", "마주")):
        return "opposite_direction"
    return None


def _extract_expected_robot_behavior(text: str) -> list[str]:
    actions: list[str] = []
    if _contains_any(text, ("감속", "속도를 줄", "천천히")):
        actions.append("SlowDown")
    if _contains_any(text, ("정지", "멈춤", "멈춰")):
        actions.append("Stop")
    if _contains_any(text, ("우회", "회피", "피해")):
        actions.append("ReplanPath")
    return actions


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
    intent.goalDistanceM = _extract_goal_distance_m(text)
    intent.obstacleTypes = _extract_obstacle_types(text)
    intent.obstacleCount = len(intent.obstacleTypes) if intent.obstacleTypes else _extract_count_near(text, ("장애물", "정적 장애물"), ("개", "대"))
    intent.obstacleType = _extract_obstacle_type(text)
    intent.obstaclePositionsFromStartM = _extract_obstacle_positions_from_start_m(text)
    intent.obstacleLateralPosition = _extract_obstacle_lateral_position(text)
    intent.pedestrianCount = _extract_count_near(text, ("보행자", "사람"), ("명", "사람"))
    intent.pedestrianDirection = _extract_pedestrian_direction(text)
    intent.expectedRobotBehavior = _extract_expected_robot_behavior(text)
    intent.obstacleBlockingRatio = _extract_blocking_ratio(text)
    intent.obstaclePositionHint = _extract_xyz_near_obstacle(text)
    if _route_midpoint_intent(text):
        intent.obstaclePlacementHint = "route_midpoint"
    intent.explicitNoPedestrian = _explicit_no_pedestrian(text)
    if intent.explicitNoPedestrian:
        intent.pedestrianCount = 0

    if _contains_any(text, ("좁은 보도", "좁은 길", "보도 폭", "보도")):
        _append_unique(intent.mapHints, ["narrow_sidewalk"])
        _append_unique(intent.suggestedCategories, ["sidewalk_operation", "speed_policy"])
        _append_unique(intent.suggestedPolicyParams, ["sidewalkWidthCm", "maxSpeedKmh"])

    if _contains_any(text, ("킥보드", "공유 킥보드", "전동킥보드")):
        _append_unique(intent.obstacleHints, ["Kickboard"])
        _append_unique(intent.suggestedCategories, ["perception_requirement"])
        _append_unique(intent.suggestedActions, ["SlowDown", "Stop", "LocalAvoidance", "ReplanPath"])
        _append_unique(intent.suggestedPolicyParams, ["safeDistanceCm", "perceptionMinRangeM"])

    if intent.obstacleType is not None or _contains_any(text, ("장애물", "정적 장애물", "로봇 경로 중앙", "경로 중앙", "막고", "막힘", "경로를 막", "막는 정도", "blockingratio", "blocking ratio", "차단")):
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

    if not intent.explicitNoPedestrian and ("Pedestrian" in intent.pedestrianHints or "pedestrian_crossing" in intent.crossingHints):
        intent.pedestrianCount = intent.pedestrianCount if intent.pedestrianCount is not None else 1

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
