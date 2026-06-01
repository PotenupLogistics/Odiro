from __future__ import annotations

from app.services.world_config_scenario_intent_extractor import (
    build_scenario_requirements,
    extract_scenario_intent,
)


PROMPT = "좁은 보도에서 공유 킥보드가 로봇 경로를 막고, 오른쪽에서 보행자가 횡단하는 상황을 만들어줘."
GENERIC_OBSTACLE_PROMPT = (
    "보도 폭은 120cm인 좁은 보도 상황을 만들어줘. "
    "로봇은 x=0, y=0, z=0에서 출발해서 x=800, y=0, z=0으로 이동한다. "
    "로봇 경로 중앙인 x=400, y=0, z=0 근처에 정적 장애물 1개를 배치하고, "
    "장애물이 경로를 막는 정도는 blockingRatio 0.6으로 설정해줘. "
    "보행자는 없는 시나리오로 만들어줘."
)
ROUTE_MIDPOINT_PROMPT = (
    "로봇은 x=0, y=0, z=0에서 출발해서 x=800, y=0, z=0으로 이동한다. "
    "로봇 경로 중앙 근처에 정적 장애물 1개를 배치해줘."
)


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


def test_generic_obstacle_prompt_extracts_explicit_numeric_intent() -> None:
    intent = extract_scenario_intent(GENERIC_OBSTACLE_PROMPT)

    assert intent.sidewalkWidthCm == 120
    assert "Obstacle" in intent.obstacleHints
    assert intent.pathBlockingHints is True
    assert intent.obstaclePositionHint == {"x": 400.0, "y": 0.0, "z": 0.0}
    assert intent.obstacleBlockingRatio == 0.6
    assert intent.explicitNoPedestrian is True
    assert "Pedestrian" not in intent.pedestrianHints


def test_generic_obstacle_requirements_include_explicit_paths() -> None:
    requirements = build_scenario_requirements(extract_scenario_intent(GENERIC_OBSTACLE_PROMPT))
    by_id = {requirement.requirementId: requirement for requirement in requirements}

    assert by_id["map_narrow_sidewalk"].expectedPath == "map.sidewalkWidthCm"
    assert "120" in by_id["map_narrow_sidewalk"].expectedValueHint
    assert by_id["obstacle_generic"].expectedPath == "obstacles[].type"
    assert by_id["obstacle_position"].expectedPath == "obstacles[].position"
    assert by_id["path_blocking_obstacle"].expectedPath == "obstacles[].blockingRatio"
    assert "0.6" in by_id["path_blocking_obstacle"].expectedValueHint
    assert by_id["no_pedestrians"].expectedPath == "pedestrians[]"


def test_route_midpoint_prompt_extracts_obstacle_placement_hint() -> None:
    intent = extract_scenario_intent(ROUTE_MIDPOINT_PROMPT)
    requirements = build_scenario_requirements(intent)
    by_id = {requirement.requirementId: requirement for requirement in requirements}

    assert intent.obstaclePlacementHint == "route_midpoint"
    assert intent.obstaclePositionHint is None
    assert by_id["obstacle_route_midpoint"].expectedPath == "obstacles[].position"
    assert "midpoint" in by_id["obstacle_route_midpoint"].expectedValueHint


def test_explicit_obstacle_coordinate_overrides_route_midpoint_requirement() -> None:
    prompt = (
        "로봇은 x=0, y=0, z=0에서 출발해서 x=800, y=0, z=0으로 이동한다. "
        "경로 중앙 근처라고 부르지만 장애물은 x=300, y=0, z=0 근처에 배치해줘."
    )
    intent = extract_scenario_intent(prompt)
    requirement_ids = {requirement.requirementId for requirement in build_scenario_requirements(intent)}

    assert intent.obstaclePlacementHint == "route_midpoint"
    assert intent.obstaclePositionHint == {"x": 300.0, "y": 0.0, "z": 0.0}
    assert "obstacle_position" in requirement_ids
    assert "obstacle_route_midpoint" not in requirement_ids
