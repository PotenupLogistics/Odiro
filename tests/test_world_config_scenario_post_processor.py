from __future__ import annotations

from copy import deepcopy

from app.core.contract_types import ContractType
from app.services.json_contract_validator import validate_payload
from app.services.world_config_scenario_post_processor import (
    apply_scenario_intent_to_world_config,
)
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
            "sidewalkWidthCm": 400,
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


def test_post_processor_adds_missing_kickboard_blocking_pedestrian_and_narrow_sidewalk() -> None:
    original = _base_payload()
    original_copy = deepcopy(original)

    result = apply_scenario_intent_to_world_config(PROMPT, original)

    assert original == original_copy
    assert result.applied is True
    patch_types = {patch.patchType for patch in result.patches}
    assert "set_narrow_sidewalk_width" in patch_types
    assert "add_kickboard_obstacle" in patch_types
    assert "set_obstacle_blocking_ratio" in patch_types
    assert "add_crossing_pedestrian" in patch_types
    assert result.patchedPayload["map"]["sidewalkWidthCm"] == 120.0
    assert result.patchedPayload["obstacles"][0]["type"] == "Kickboard"
    assert result.patchedPayload["obstacles"][0]["blockingRatio"] == 0.6
    assert "yawDegree" not in result.patchedPayload["obstacles"][0]
    assert result.patchedPayload["pedestrians"][0]["behavior"] == "Crossing"


def test_post_processed_payload_passes_schema_and_scenario_reflection() -> None:
    result = apply_scenario_intent_to_world_config(PROMPT, _base_payload())

    validation = validate_payload(ContractType.world_config, result.patchedPayload)
    reflection = validate_scenario_reflection(PROMPT, result.patchedPayload)

    assert validation.valid is True
    assert reflection.passed is True


def test_post_processor_preserves_existing_schema_valid_objects_when_possible() -> None:
    payload = _base_payload()
    payload["map"]["sidewalkWidthCm"] = 180
    payload["obstacles"] = [
        {
            "objectId": "kickboard_existing",
            "type": "Kickboard",
            "position": {"x": 500, "y": 0, "z": 0},
            "blockingRatio": 0.0,
        }
    ]
    payload["pedestrians"] = [
        {
            "objectId": "ped_existing",
            "spawn": {"x": 500, "y": -200, "z": 0},
            "goal": {"x": 500, "y": 200, "z": 0},
            "speedKmh": 2.5,
            "behavior": "Walking",
        }
    ]

    result = apply_scenario_intent_to_world_config(PROMPT, payload)

    assert result.patchedPayload["map"]["sidewalkWidthCm"] == 180
    assert len(result.patchedPayload["obstacles"]) == 1
    assert result.patchedPayload["obstacles"][0]["objectId"] == "kickboard_existing"
    assert result.patchedPayload["obstacles"][0]["blockingRatio"] == 0.6
    assert len(result.patchedPayload["pedestrians"]) == 1
    assert result.patchedPayload["pedestrians"][0]["objectId"] == "ped_existing"
    assert result.patchedPayload["pedestrians"][0]["behavior"] == "Crossing"
