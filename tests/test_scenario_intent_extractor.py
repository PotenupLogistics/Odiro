from __future__ import annotations

from app.services.world_config_scenario_intent_extractor import (
    build_scenario_requirements,
    extract_scenario_intent,
)


PROMPT = "좁은 보도에서 공유 킥보드가 로봇 경로를 막고, 오른쪽에서 보행자가 횡단하는 상황을 만들어줘."


def test_korean_prompt_extracts_scenario_intent() -> None:
    intent = extract_scenario_intent(PROMPT)

    assert "narrow_sidewalk" in intent.mapHints
    assert "Kickboard" in intent.obstacleHints
    assert "Pedestrian" in intent.pedestrianHints
    assert "pedestrian_crossing" in intent.crossingHints
    assert intent.pathBlockingHints is True
    assert "perception_requirement" in intent.suggestedCategories
    assert "sidewalk_operation" in intent.suggestedCategories
    assert "LocalAvoidance" in intent.suggestedActions


def test_scenario_requirements_include_world_config_expectations() -> None:
    requirements = build_scenario_requirements(extract_scenario_intent(PROMPT))
    paths = {requirement.expectedPath for requirement in requirements}

    assert "map.sidewalkWidthCm" in paths
    assert "obstacles[].type" in paths
    assert "obstacles[].blockingRatio" in paths
    assert "pedestrians[].behavior" in paths
