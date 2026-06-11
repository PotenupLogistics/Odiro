from __future__ import annotations

from app.models.generation import (
    RetrievedPolicyContext,
    WorldConfigGenerationRequest,
    WorldConfigGenerationResult,
    WorldConfigGenerationError,
    WorldConfigValidationSummary,
)
from app.models.handoff import UE5WorldConfigHandoffRequest
from app.models.llm import LlmProvider
from app.models.scenario import ScenarioPostProcessPatch, ScenarioPostProcessResult
from app.services.ue5_world_config_handoff_service import create_ue5_world_config_handoff


def _generation_request() -> WorldConfigGenerationRequest:
    return WorldConfigGenerationRequest(
        schemaVersion="1.0",
        requestId="REQ-HANDOFF-001",
        generationType="world_config",
        prompt="좁은 보도에서 공유 킥보드가 로봇 경로를 막고 보행자가 횡단",
        targetContractType="world_config",
        policyId="policy_v1_basic_safety",
        constraints={
            "unitSystem": "cm_kmh_sec_degree",
            "allowedMapTypes": ["Sidewalk"],
            "allowedObjectTypes": ["Pedestrian", "Kickboard", "Obstacle"],
            "fixedPolicyId": "policy_v1_basic_safety",
            "defaultSeed": 1001,
            "requireValidation": True,
        },
        maxRepairAttempts=1,
    )


def _handoff_request(include_diagnostics: bool = True) -> UE5WorldConfigHandoffRequest:
    return UE5WorldConfigHandoffRequest(
        schemaVersion="1.0",
        requestId="HANDOFF-001",
        generationRequest=_generation_request(),
        handoffTarget="ue5",
        includeDiagnostics=include_diagnostics,
    )


def _world_config() -> dict:
    return {
        "schemaVersion": "1.0",
        "worldId": "world-1",
        "scenarioId": "scenario-1",
        "seed": 1001,
        "map": {
            "type": "Sidewalk",
            "lengthCm": 1000,
            "sidewalkWidthCm": 120,
            "surfaceCondition": "dry",
            "slopeDegree": 0,
        },
        "robot": {
            "botId": "bot-1",
            "spawn": {"x": 0, "y": 0, "z": 0},
            "goal": {"x": 100, "y": 0, "z": 0},
            "policyId": "policy_v1_basic_safety",
        },
        "obstacles": [
            {
                "objectId": "kickboard_001",
                "type": "Kickboard",
                "position": {"x": 50, "y": 0, "z": 0},
                "blockingRatio": 0.6,
            }
        ],
        "pedestrians": [
            {
                "objectId": "pedestrian_001",
                "spawn": {"x": 50, "y": -200, "z": 0},
                "goal": {"x": 50, "y": 200, "z": 0},
                "speedKmh": 3.0,
                "behavior": "Crossing",
            }
        ],
        "environmentObjects": [],
        "runtime": {
            "maxDurationSec": 300,
            "captureReplay": False,
            "emitEventLog": True,
        },
    }


def _success_generation_result() -> WorldConfigGenerationResult:
    return WorldConfigGenerationResult(
        requestId="REQ-HANDOFF-001",
        generationType="world_config",
        targetContractType="world_config",
        success=True,
        generatedPayload=_world_config(),
        validation=WorldConfigValidationSummary(status="passed"),
        attempts=[],
        retrievedContexts=[
            RetrievedPolicyContext(
                chunkId="chunk-1",
                cardId="card-1",
                category="perception_requirement",
                evidenceLocation="p.1",
                relatedActions=["Stop"],
                relatedPolicyParams=["safeDistanceCm"],
                shortText="test",
                score=1.0,
            )
        ],
        scenarioPostProcessing=ScenarioPostProcessResult(
            applied=True,
            patches=[
                ScenarioPostProcessPatch(
                    patchId="PATCH-001",
                    patchType="add_kickboard_obstacle",
                    targetPath="obstacles[]",
                    beforeValue=None,
                    afterValue={"type": "Kickboard"},
                    reason="test",
                )
            ],
            patchedPayload=_world_config(),
        ),
        warnings=[],
        error=None,
    )


def _failed_generation_result() -> WorldConfigGenerationResult:
    return WorldConfigGenerationResult(
        requestId="REQ-HANDOFF-001",
        generationType="world_config",
        targetContractType="world_config",
        success=False,
        generatedPayload=None,
        validation=WorldConfigValidationSummary(status="skipped"),
        attempts=[],
        retrievedContexts=[],
        warnings=[],
        error=WorldConfigGenerationError(code="provider_disabled", message="disabled"),
    )


def test_successful_generation_result_creates_ue5_handoff_response() -> None:
    response = create_ue5_world_config_handoff(
        _handoff_request(),
        provider=LlmProvider.disabled,
        generator=lambda request, provider: _success_generation_result(),
    )

    assert response.success is True
    assert response.worldConfig is None
    assert response.episodeSpec is not None
    assert response.metadata.units["distance"] == "cm"
    assert response.metadata.units["coordinate"] == "ue5_world_coordinate"
    assert response.validation.schemaValidationPassed is True
    assert response.validation.contractValidationPassed is True
    assert response.postProcessing is not None
    assert response.postProcessing.applied is True
    assert response.diagnostics is not None


def test_failed_generation_result_returns_no_world_config() -> None:
    response = create_ue5_world_config_handoff(
        _handoff_request(),
        provider=LlmProvider.disabled,
        generator=lambda request, provider: _failed_generation_result(),
    )

    assert response.success is False
    assert response.worldConfig is None
    assert response.error is not None
    assert response.error.code == "provider_disabled"
    assert response.validation.contractValidationPassed is False


def test_diagnostics_are_omitted_when_not_requested() -> None:
    response = create_ue5_world_config_handoff(
        _handoff_request(include_diagnostics=False),
        provider=LlmProvider.disabled,
        generator=lambda request, provider: _success_generation_result(),
    )

    assert response.success is True
    assert response.diagnostics is None
