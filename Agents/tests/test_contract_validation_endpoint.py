from __future__ import annotations

import pytest

from app.core.contract_types import ContractType
from app.services.json_contract_validator import validate_payload


def valid_world_config_payload() -> dict:
    location = {"x": 0, "y": 0, "z": 0}
    return {
        "schemaVersion": "1.0",
        "worldId": "world-1",
        "scenarioId": "scenario-1",
        "seed": 42,
        "map": {
            "type": "sidewalk",
            "lengthCm": 1000,
            "sidewalkWidthCm": 250,
            "surfaceCondition": "dry",
            "slopeDegree": 0,
        },
        "robot": {
            "botId": "bot-1",
            "spawn": location,
            "goal": {"x": 100, "y": 0, "z": 0},
            "policyId": "POLICY-MVP",
        },
        "runtime": {
            "maxDurationSec": 120,
            "captureReplay": False,
            "emitEventLog": True,
        },
        "obstacles": [],
        "pedestrians": [],
        "environmentObjects": [],
    }


def test_contract_validation_service_accepts_valid_world_config() -> None:
    result = validate_payload(ContractType.world_config, valid_world_config_payload())

    payload = result.model_dump(mode="json")
    assert payload["valid"] is True
    assert payload["contractType"] == "world_config"
    assert payload["errors"] == []


def test_contract_validation_service_rejects_invalid_world_config() -> None:
    invalid_payload = valid_world_config_payload()
    invalid_payload.pop("worldId")

    result = validate_payload(ContractType.world_config, invalid_payload)

    payload = result.model_dump(mode="json")
    assert payload["valid"] is False
    assert payload["errors"]


def test_contract_type_rejects_unknown_value_before_validation_service() -> None:
    with pytest.raises(ValueError):
        ContractType("not_a_contract")
