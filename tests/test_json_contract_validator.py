from __future__ import annotations

from app.core.contract_types import ContractType
from app.services.json_contract_validator import validate_payload


def valid_policy_config() -> dict:
    return {
        "schemaVersion": "1.0",
        "policyId": "policy-kor-003",
        "policyName": "MVP policy",
        "priority": 1,
        "parameters": {
            "maxSpeedKmh": 5,
            "emergencyStopDistanceCm": 100,
            "traversabilityThreshold": 0.7,
        },
        "availableActions": ["SlowDown", "Stop", "EmergencyStop"],
    }


def valid_decision_request() -> dict:
    location = {"x": 0, "y": 0, "z": 0}
    return {
        "schemaVersion": "1.0",
        "requestId": "req-1",
        "simulationId": "sim-1",
        "timestampSec": 1.2,
        "policyId": "policy-kor-003",
        "event": {
            "eventId": "event-1",
            "type": "ObstacleAhead",
            "severity": "high",
            "confidence": 0.95,
            "source": "ue5",
            "description": "obstacle ahead",
        },
        "botState": {
            "location": location,
            "yawDegree": 0,
            "pitchDegree": 0,
            "rollDegree": 0,
            "speedKmh": 4,
        },
        "remainTarget": {"location": {"x": 100, "y": 0, "z": 0}, "distanceCm": 100},
        "detectedObjects": [
            {
                "objectId": "obj-1",
                "type": "Obstacle",
                "location": {"x": 10, "y": 0, "z": 0},
                "relativeLocation": {"x": 10, "y": 0, "z": 0},
                "distanceCm": 100,
                "relativeSpeedKmh": 0,
                "relativeDirection": "front",
                "isOnPath": True,
                "pathOverlapDistanceCm": 20,
                "timeToCollisionSec": 2,
            }
        ],
        "environments": [{"type": "sidewalk", "state": "normal"}],
        "pathContext": {
            "pathBlocked": True,
            "leftClearanceCm": 50,
            "rightClearanceCm": 30,
            "leftTraversable": True,
            "rightTraversable": False,
            "pathOverlapRatio": 0.5,
        },
        "terrain": {
            "traversabilityScore": 0.8,
            "slopeDegree": 2,
            "curbHeightCm": 3,
            "surfaceType": "flat",
        },
        "communicationStatus": "connected",
    }


def valid_decision_response() -> dict:
    return {
        "schemaVersion": "1.0",
        "requestId": "req-1",
        "decisionId": "dec-1",
        "selectedAction": "Stop",
        "priority": 10,
        "confidence": 0.9,
        "command": {
            "targetSpeedKmh": 0,
            "durationSec": 1,
            "allowRecheck": True,
            "recheckAfterSec": 1,
            "replanRequired": False,
        },
        "reason": "obstacle ahead",
        "recordTags": ["safety"],
        "appliedRules": [
            {"ruleId": "r1", "ruleName": "stop for obstacle", "matchedCondition": "ObstacleAhead"}
        ],
    }


def test_contract_type_contains_all_values() -> None:
    assert {item.value for item in ContractType} == {
        "policy_config",
        "world_config",
        "decision_request",
        "decision_response",
        "evaluation_spec",
        "run_result",
    }


def test_valid_policy_config_passes() -> None:
    result = validate_payload(ContractType.policy_config, valid_policy_config())
    assert result.valid is True
    assert result.normalizedPayload is not None


def test_invalid_policy_config_fails() -> None:
    payload = valid_policy_config()
    payload.pop("policyId")
    result = validate_payload(ContractType.policy_config, payload)
    assert result.valid is False
    assert result.errors
    assert result.errorSummary["missingRequiredFields"]
    assert any(error["errorType"] == "missing_required" for error in result.structuredErrors)


def test_valid_decision_request_passes() -> None:
    result = validate_payload(ContractType.decision_request, valid_decision_request())
    assert result.valid is True


def test_invalid_decision_request_fails() -> None:
    payload = valid_decision_request()
    payload["event"]["type"] = "NotARealEvent"
    result = validate_payload(ContractType.decision_request, payload)
    assert result.valid is False
    assert result.errors
    assert result.errorSummary["enumErrors"] or result.errorSummary["typeErrors"]


def test_valid_decision_response_passes() -> None:
    result = validate_payload(ContractType.decision_response, valid_decision_response())
    assert result.valid is True


def test_invalid_decision_response_fails() -> None:
    payload = valid_decision_response()
    payload["selectedAction"] = "FlyAway"
    result = validate_payload(ContractType.decision_response, payload)
    assert result.valid is False
    assert result.errors
    assert result.structuredErrors


def test_validation_summary_groups_extra_fields() -> None:
    payload = valid_policy_config()
    payload["unexpected"] = True

    result = validate_payload(ContractType.policy_config, payload)

    assert result.valid is False
    assert result.errorSummary["extraFields"]
