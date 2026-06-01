from __future__ import annotations

from app.models.scenario import ScenarioIntent, ScenarioRequirement
from app.services.natural_language_normalizer import normalize_prompt


def _append_unique(items: list[str], values: list[str]) -> None:
    for value in values:
        if value not in items:
            items.append(value)


def _contains_any(text: str, keywords: tuple[str, ...]) -> bool:
    return any(keyword in text for keyword in keywords)


def extract_scenario_intent(prompt: str) -> ScenarioIntent:
    text = normalize_prompt(prompt).lower()
    intent = ScenarioIntent()

    if _contains_any(text, ("좁은 보도", "좁은 길", "보도 폭", "보도")):
        _append_unique(intent.mapHints, ["narrow_sidewalk"])
        _append_unique(intent.suggestedCategories, ["sidewalk_operation", "speed_policy"])
        _append_unique(intent.suggestedPolicyParams, ["sidewalkWidthCm", "maxSpeedKmh"])

    if _contains_any(text, ("킥보드", "공유 킥보드", "전동킥보드")):
        _append_unique(intent.obstacleHints, ["Kickboard"])
        _append_unique(intent.suggestedCategories, ["perception_requirement"])
        _append_unique(intent.suggestedActions, ["SlowDown", "Stop", "LocalAvoidance", "ReplanPath"])
        _append_unique(intent.suggestedPolicyParams, ["safeDistanceCm", "perceptionMinRangeM"])

    if _contains_any(text, ("장애물", "막고", "막힘", "경로를 막", "차단")):
        intent.pathBlockingHints = True
        _append_unique(intent.obstacleHints, ["Obstacle"])
        _append_unique(intent.suggestedCategories, ["perception_requirement"])
        _append_unique(intent.suggestedActions, ["Stop", "LocalAvoidance", "ReplanPath"])

    if _contains_any(text, ("보행자", "사람")):
        _append_unique(intent.pedestrianHints, ["Pedestrian"])
        _append_unique(intent.suggestedCategories, ["sidewalk_operation", "perception_requirement"])
        _append_unique(intent.suggestedActions, ["SlowDown", "YieldWait", "Stop"])

    if _contains_any(text, ("횡단", "건너", "가로질러")):
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
        requirements.append(
            ScenarioRequirement(
                requirementId="map_narrow_sidewalk",
                type="map",
                description="Represent the user's narrow sidewalk condition.",
                requiredInWorldConfig=True,
                expectedPath="map.sidewalkWidthCm",
                expectedValueHint="Use a relatively narrow sidewalk width in cm.",
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

    if intent.pathBlockingHints:
        requirements.append(
            ScenarioRequirement(
                requirementId="path_blocking_obstacle",
                type="path_blocking",
                description="If the user says the path is blocked, include a blocking obstacle near the robot path.",
                requiredInWorldConfig=True,
                expectedPath="obstacles[].blockingRatio",
                expectedValueHint="Use a positive blockingRatio, preferably 0.5 or higher.",
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
