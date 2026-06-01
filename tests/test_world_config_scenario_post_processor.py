from __future__ import annotations

from copy import deepcopy

from app.core.contract_types import ContractType
from app.models.environment import EnvironmentSamplingContext, EnvironmentParameterSet
from app.services.json_contract_validator import validate_payload
from app.services.world_config_scenario_post_processor import (
    apply_scenario_intent_to_world_config,
)
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
    "로봇 경로 중앙 근처에 정적 장애물 1개가 경로를 막고 있는 상황을 만들어줘."
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


def test_post_processor_adds_generic_obstacle_and_removes_pedestrians_from_explicit_prompt() -> None:
    payload = _base_payload()
    payload["map"]["sidewalkWidthCm"] = 150
    payload["robot"]["goal"] = {"x": 800, "y": 0, "z": 0}
    payload["pedestrians"] = [
        {
            "objectId": "ped_wrong",
            "spawn": {"x": 400, "y": -100, "z": 0},
            "goal": {"x": 400, "y": 100, "z": 0},
            "speedKmh": 3,
            "behavior": "Walking",
        }
    ]

    result = apply_scenario_intent_to_world_config(GENERIC_OBSTACLE_PROMPT, payload)

    patch_types = {patch.patchType for patch in result.patches}
    assert "set_sidewalk_width_from_prompt" in patch_types
    assert "add_generic_obstacle" in patch_types
    assert "remove_pedestrians_for_no_pedestrian_prompt" in patch_types
    assert result.patchedPayload["map"]["sidewalkWidthCm"] == 120
    assert result.patchedPayload["obstacles"][0]["type"] == "Obstacle"
    assert result.patchedPayload["obstacles"][0]["position"] == {"x": 400.0, "y": 0.0, "z": 0.0}
    assert result.patchedPayload["obstacles"][0]["blockingRatio"] == 0.6
    assert result.patchedPayload["pedestrians"] == []

    validation = validate_payload(ContractType.world_config, result.patchedPayload)
    reflection = validate_scenario_reflection(GENERIC_OBSTACLE_PROMPT, result.patchedPayload)
    assert validation.valid is True
    assert reflection.passed is True


def test_post_processor_applies_environment_sampling_parameters_over_payload_values() -> None:
    payload = _base_payload()
    payload["map"]["sidewalkWidthCm"] = 200
    payload["runtime"]["maxDurationSec"] = 300
    payload["obstacles"] = [
        {
            "objectId": "obstacle_existing",
            "type": "Obstacle",
            "position": {"x": 500, "y": 0, "z": 0},
            "blockingRatio": 0.3,
        }
    ]
    context = EnvironmentSamplingContext(
        enabled=True,
        seed=1001,
        scenarioType="obstacle_ahead",
        parameters=EnvironmentParameterSet(
            sidewalkWidthCm=120,
            pedestrianCount=1,
            pedestrianSpeedMps=1.2,
            obstacleBlockingRatio=0.6,
            obstacleLateralOffsetM=0.0,
            crossingAngleDeg=90,
            robotSpeedKmh=5,
            slopeDegree=0,
            curbHeightCm=0,
            timeLimitSec=60,
        ),
        fixedParameters={},
        warnings=[],
    )

    result = apply_scenario_intent_to_world_config(
        "정적 장애물이 경로를 막는 상황",
        payload,
        environment_context=context,
    )

    patch_types = {patch.patchType for patch in result.patches}
    assert "set_sidewalk_width_from_environment_sampler" in patch_types
    assert "set_obstacle_blocking_ratio_from_environment_sampler" in patch_types
    assert "set_runtime_limit_from_environment_sampler" in patch_types
    assert result.patchedPayload["map"]["sidewalkWidthCm"] == 120
    assert result.patchedPayload["obstacles"][0]["blockingRatio"] == 0.6
    assert result.patchedPayload["runtime"]["maxDurationSec"] == 60


def test_post_processor_adds_obstacle_at_route_midpoint_when_requested() -> None:
    payload = _base_payload()
    payload["robot"]["goal"] = {"x": 800, "y": 0, "z": 0}

    result = apply_scenario_intent_to_world_config(ROUTE_MIDPOINT_PROMPT, payload)

    patch_types = {patch.patchType for patch in result.patches}
    assert "add_generic_obstacle_at_route_midpoint" in patch_types
    assert result.patchedPayload["obstacles"][0]["position"] == {"x": 400.0, "y": 0.0, "z": 0.0}


def test_post_processor_moves_existing_obstacle_to_route_midpoint() -> None:
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

    result = apply_scenario_intent_to_world_config(ROUTE_MIDPOINT_PROMPT, payload)

    patch_types = {patch.patchType for patch in result.patches}
    assert "set_obstacle_position_to_route_midpoint" in patch_types
    assert result.patchedPayload["obstacles"][0]["position"] == {"x": 400.0, "y": 0.0, "z": 0.0}
