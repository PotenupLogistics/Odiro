from __future__ import annotations

from app.services.world_config_scenario_reflection import validate_scenario_reflection


PROMPT = "좁은 보도에서 공유 킥보드가 로봇 경로를 막고, 오른쪽에서 보행자가 횡단하는 상황을 만들어줘."


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
