from __future__ import annotations

from app.services.world_config_output_contract_builder import build_world_config_output_contract
from app.services.world_config_scenario_reflection import validate_scenario_reflection
from app.services.world_config_scenario_repair_prompt_builder import build_scenario_repair_prompt


PROMPT = "좁은 보도에서 공유 킥보드가 로봇 경로를 막고, 오른쪽에서 보행자가 횡단하는 상황을 만들어줘."


def test_scenario_repair_prompt_includes_path_based_instructions() -> None:
    previous_json = {
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
    reflection = validate_scenario_reflection(PROMPT, previous_json)

    prompt = build_scenario_repair_prompt(
        previous_json,
        reflection,
        build_world_config_output_contract(),
    )

    assert "schema validation already passed" in prompt
    assert "obstacles[].type" in prompt
    assert 'type "Kickboard"' in prompt
    assert "blockingRatio" in prompt
    assert "pedestrians[].behavior" in prompt
    assert 'behavior "Crossing"' in prompt
    assert "Do not add extra keys outside the schema" in prompt
    assert "Preserve required fields already present" in prompt
