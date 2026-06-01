from __future__ import annotations

from app.services.world_config_scenario_reflection import validate_scenario_reflection


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
    "경로 중앙 근처에 정적 장애물 1개가 경로를 막고 있는 상황을 만들어줘."
)


def _base_payload() -> dict:
    return {
        "schemaVersion": "1.0",
        "worldId": "world",
        "scenarioId": "scenario",
        "seed": 1001,
        "map": {
            "type": "Sidewalk",
            "lengthCm": 10000,
            "sidewalkWidthCm": 180,
            "surfaceCondition": "dry",
            "slopeDegree": 0,
        },
        "robot": {
            "botId": "bot",
            "spawn": {"x": 0, "y": 0, "z": 0},
            "goal": {"x": 1000, "y": 0, "z": 0},
            "policyId": "policy_v1_basic_safety",
        },
        "runtime": {"maxDurationSec": 300, "captureReplay": False, "emitEventLog": True},
        "obstacles": [],
        "pedestrians": [],
        "environmentObjects": [],
    }


def test_reflection_fails_when_kickboard_and_crossing_are_missing() -> None:
    result = validate_scenario_reflection(PROMPT, _base_payload())

    assert result.passed is False
    kickboard_issue = next(issue for issue in result.issues if issue.requirementId == "obstacle_kickboard")
    crossing_issue = next(issue for issue in result.issues if issue.requirementId == "pedestrian_crossing")
    assert kickboard_issue.issueType == "missing_kickboard_obstacle"
    assert kickboard_issue.expectedPath == "obstacles[].type"
    assert kickboard_issue.expectedValueHint == "Kickboard"
    assert "type \"Kickboard\"" in kickboard_issue.repairInstruction
    assert crossing_issue.issueType == "missing_pedestrian_crossing_behavior"
    assert crossing_issue.expectedPath == "pedestrians[].behavior"


def test_reflection_fails_when_blocking_ratio_is_missing() -> None:
    payload = _base_payload()
    payload["obstacles"] = [
        {
            "objectId": "kickboard-1",
            "type": "Kickboard",
            "position": {"x": 400, "y": 0, "z": 0},
            "blockingRatio": 0,
        }
    ]
    payload["pedestrians"] = [
        {
            "objectId": "ped-1",
            "spawn": {"x": 500, "y": -200, "z": 0},
            "goal": {"x": 500, "y": 200, "z": 0},
            "speedKmh": 4,
            "behavior": "Crossing",
        }
    ]

    result = validate_scenario_reflection(PROMPT, payload)

    assert result.passed is False
    issue = next(item for item in result.issues if item.requirementId == "path_blocking_obstacle")
    assert issue.issueType == "missing_path_blocking_obstacle"
    assert issue.expectedPath == "obstacles[].blockingRatio"


def test_reflection_passes_when_prompt_conditions_are_represented() -> None:
    payload = _base_payload()
    payload["obstacles"] = [
        {
            "objectId": "kickboard-1",
            "type": "Kickboard",
            "position": {"x": 400, "y": 0, "z": 0},
            "blockingRatio": 0.8,
        }
    ]
    payload["pedestrians"] = [
        {
            "objectId": "ped-1",
            "spawn": {"x": 500, "y": -200, "z": 0},
            "goal": {"x": 500, "y": 200, "z": 0},
            "speedKmh": 4,
            "behavior": "Crossing",
        }
    ]

    result = validate_scenario_reflection(PROMPT, payload)

    assert result.passed is True
    assert not result.missingRequirements


def test_generic_obstacle_reflection_fails_when_llm_omits_obstacle_and_numeric_values() -> None:
    payload = _base_payload()
    payload["map"]["sidewalkWidthCm"] = 150
    payload["robot"]["goal"] = {"x": 800, "y": 0, "z": 0}

    result = validate_scenario_reflection(GENERIC_OBSTACLE_PROMPT, payload)

    assert result.passed is False
    requirement_ids = {issue.requirementId for issue in result.issues}
    assert "map_narrow_sidewalk" in requirement_ids
    assert "obstacle_generic" in requirement_ids
    assert "path_blocking_obstacle" in requirement_ids
    assert result.checkedRequirements


def test_generic_obstacle_reflection_passes_with_exact_obstacle_and_no_pedestrians() -> None:
    payload = _base_payload()
    payload["map"]["sidewalkWidthCm"] = 120
    payload["robot"]["goal"] = {"x": 800, "y": 0, "z": 0}
    payload["obstacles"] = [
        {
            "objectId": "obstacle_001",
            "type": "Obstacle",
            "position": {"x": 400, "y": 0, "z": 0},
            "blockingRatio": 0.6,
        }
    ]
    payload["pedestrians"] = []

    result = validate_scenario_reflection(GENERIC_OBSTACLE_PROMPT, payload)

    assert result.passed is True
    assert not result.missingRequirements


def test_route_midpoint_reflection_fails_when_obstacle_is_near_goal() -> None:
    payload = _base_payload()
    payload["robot"]["goal"] = {"x": 800, "y": 0, "z": 0}
    payload["obstacles"] = [
        {
            "objectId": "obstacle_001",
            "type": "Obstacle",
            "position": {"x": 800, "y": 0, "z": 0},
            "blockingRatio": 0.6,
        }
    ]

    result = validate_scenario_reflection(ROUTE_MIDPOINT_PROMPT, payload)

    assert result.passed is False
    issue = next(item for item in result.issues if item.requirementId == "obstacle_route_midpoint")
    assert issue.issueType == "obstacle_not_near_route_midpoint"
    assert "midpoint" in issue.actualValueSummary


def test_route_midpoint_reflection_passes_when_obstacle_is_near_midpoint() -> None:
    payload = _base_payload()
    payload["robot"]["goal"] = {"x": 800, "y": 0, "z": 0}
    payload["obstacles"] = [
        {
            "objectId": "obstacle_001",
            "type": "Obstacle",
            "position": {"x": 400, "y": 0, "z": 0},
            "blockingRatio": 0.6,
        }
    ]

    result = validate_scenario_reflection(ROUTE_MIDPOINT_PROMPT, payload)

    assert result.passed is True
