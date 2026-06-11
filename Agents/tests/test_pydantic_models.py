from __future__ import annotations

from app.models.decision import (
    ActionName,
    DecisionCommand,
    DecisionRequest,
    DecisionResponse,
    EventType,
)
from app.models.evaluation import EvaluationSpec
from app.models.policy import PolicyConfig, PolicyParameters
from app.models.run_result import RunResult
from app.models.world import WorldConfig


def test_model_imports() -> None:
    assert PolicyConfig
    assert WorldConfig
    assert DecisionRequest
    assert DecisionResponse
    assert EvaluationSpec
    assert RunResult


def test_action_enum_values_match_contract() -> None:
    assert [item.value for item in ActionName] == [
        "Continue",
        "SlowDown",
        "Stop",
        "EmergencyStop",
        "LocalAvoidance",
        "ReplanPath",
        "YieldWait",
        "RequestOperator",
    ]


def test_event_type_enum_values_match_contract() -> None:
    assert "PedestrianAhead" in [item.value for item in EventType]
    assert "CommunicationIssue" in [item.value for item in EventType]
    assert "Unknown" in [item.value for item in EventType]


def test_policy_parameters_include_core_fields() -> None:
    fields = PolicyParameters.model_fields
    assert "maxSpeedKmh" in fields
    assert "emergencyStopDistanceCm" in fields
    assert "traversabilityThreshold" in fields


def test_decision_models_include_core_fields() -> None:
    assert "detectedObjects" in DecisionRequest.model_fields
    assert "botState" in DecisionRequest.model_fields
    assert "terrain" in DecisionRequest.model_fields
    assert "pathContext" in DecisionRequest.model_fields
    assert "selectedAction" in DecisionResponse.model_fields
    assert "command" in DecisionResponse.model_fields
    assert "appliedRules" in DecisionResponse.model_fields
    assert "targetSpeedKmh" in DecisionCommand.model_fields
