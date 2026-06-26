from __future__ import annotations

import json

from app.models.handoff import UE5WorldConfigHandoffRequest
from app.models.generation import WorldConfigGenerationResult, WorldConfigValidationSummary
from app.models.episode_spec import EpisodeScenarioReflectionIssue, EpisodeScenarioReflectionResult
from app.models.llm import LlmGenerationRequest, LlmGenerationResponse, LlmProvider
from app.services.ue5_world_config_handoff_service import create_ue5_world_config_handoff
from app.services.world_config_generation_orchestrator import generate_world_config
from tests.test_ue5_handoff_episode_spec_response import (
    _environment_sampling_handoff_request,
)
from tests.test_ue5_world_config_handoff_service import _world_config


class FakeTraceClient:
    def generate(self, request: LlmGenerationRequest) -> LlmGenerationResponse:
        payload = _world_config()
        payload["map"]["sidewalkWidthCm"] = 150
        payload["robot"]["spawn"] = {"x": 0, "y": 0, "z": 0}
        payload["robot"]["goal"] = {"x": 800, "y": 0, "z": 0}
        payload["runtime"]["maxDurationSec"] = 120
        payload["obstacles"] = [
            {
                "objectId": "obstacle_001",
                "type": "Obstacle",
                "position": {"x": 800, "y": 0, "z": 0},
                "blockingRatio": 0.3,
            }
        ]
        payload["pedestrians"] = []
        content = json.dumps(payload, ensure_ascii=False)
        return LlmGenerationResponse(
            requestId=request.requestId,
            provider=request.provider,
            model=request.model,
            success=True,
            content=content,
            rawContent=content,
            warnings=[],
        )


def _response(include_diagnostics: bool = True):
    request_payload = _environment_sampling_handoff_request().model_dump()
    request_payload["includeDiagnostics"] = include_diagnostics
    request = UE5WorldConfigHandoffRequest(**request_payload)
    fake_client = FakeTraceClient()
    return create_ue5_world_config_handoff(
        request,
        provider=LlmProvider.custom,
        generator=lambda generation_request, provider: generate_world_config(
            generation_request,
            provider=provider,
            client_override=fake_client,
        ),
    )


def test_handoff_diagnostics_include_generation_trace_when_requested() -> None:
    response = _response(include_diagnostics=True)

    assert response.success is True
    assert response.diagnostics is not None
    trace = response.diagnostics.get("generationTrace")
    assert trace is not None
    assert trace["requestId"] == "GEN-SAMPLER-FINAL-001"
    source_types = {item["sourceType"] for item in trace["evidenceItems"]}
    assert "environment_sampling" in source_types
    assert "placement_rule" in source_types
    assert "episode_spec_adapter" in source_types
    assert "scenario_reflection" in source_types
    assert "validation" in source_types
    assert trace["summary"]["status"] == "success"
    assert trace["summary"]["failureStage"] is None
    assert any(item["fieldPath"] == "map.sidewalkWidthCm" and item["valueSummary"] == 120 for item in trace["evidenceItems"])
    assert any(item["fieldPath"] == "obstacles[].blockingRatio" and item["valueSummary"] == 0.6 for item in trace["evidenceItems"])


def test_handoff_omits_generation_trace_when_diagnostics_disabled() -> None:
    response = _response(include_diagnostics=False)

    assert response.success is True
    assert response.diagnostics is None


def test_generation_trace_diagnostics_do_not_include_full_payloads_or_secrets() -> None:
    response = _response(include_diagnostics=True)
    serialized = json.dumps(response.diagnostics["generationTrace"], ensure_ascii=False)

    assert "rawContent" not in serialized
    assert '"worldConfig"' not in serialized
    assert '"episodeSpec"' not in serialized
    assert "OPENAI_API_KEY" not in serialized
    assert "api_key" not in serialized.lower()


def test_trace_builder_error_does_not_break_handoff_success(monkeypatch) -> None:
    def raise_trace_error(*args, **kwargs):
        raise RuntimeError("trace boom")

    monkeypatch.setattr(
        "app.services.ue5_world_config_handoff_service.build_generation_trace",
        raise_trace_error,
    )

    response = _response(include_diagnostics=True)

    assert response.success is True
    assert response.episodeSpec is not None
    assert response.diagnostics is not None
    assert "generationTrace" not in response.diagnostics
    assert response.diagnostics["generationTraceError"] == "trace boom"


def test_adapter_failure_records_generation_trace_failure_stage(monkeypatch) -> None:
    def raise_adapter_error(*args, **kwargs):
        raise RuntimeError("adapter boom")

    monkeypatch.setattr(
        "app.services.ue5_world_config_handoff_service.convert_world_config_to_episode_spec_with_warnings",
        raise_adapter_error,
    )

    response = _response(include_diagnostics=True)

    assert response.success is False
    assert response.episodeSpec is None
    assert response.diagnostics is not None
    trace = response.diagnostics["generationTrace"]
    assert trace["summary"]["status"] == "failed"
    assert trace["summary"]["failureStage"] == "episode_spec_adapter"
    assert "adapter boom" in trace["summary"]["errorSummary"]


def test_world_config_validation_failure_records_generation_trace_failure_stage() -> None:
    request_payload = _environment_sampling_handoff_request().model_dump()
    request_payload["includeDiagnostics"] = True
    request = UE5WorldConfigHandoffRequest(**request_payload)
    result = WorldConfigGenerationResult(
        requestId="GEN-SAMPLER-FINAL-001",
        generationType="world_config",
        targetContractType="world_config",
        success=False,
        generatedPayload=None,
        validation=WorldConfigValidationSummary(status="failed", errors=["provider chain failed"]),
        retrievedContexts=[],
        warnings=[],
    )

    response = create_ue5_world_config_handoff(
        request,
        provider=LlmProvider.custom,
        generator=lambda generation_request, provider: result,
    )

    assert response.success is False
    assert response.episodeSpec is None
    assert response.diagnostics is not None
    assert response.diagnostics["failureStage"] == "world_config_validation"
    assert response.diagnostics["errorSummary"] == "provider chain failed"
    trace = response.diagnostics["generationTrace"]
    assert trace["summary"]["status"] == "failed"
    assert trace["summary"]["failureStage"] == "world_config_validation"


def test_episode_scenario_reflection_failure_records_generation_trace_failure_stage(monkeypatch) -> None:
    def fail_episode_reflection(*args, **kwargs):
        return EpisodeScenarioReflectionResult(
            passed=False,
            issues=[
                EpisodeScenarioReflectionIssue(
                    issueType="missing_blocking_ratio",
                    message="blocking_ratio is missing",
                )
            ],
            staticObstacleCount=1,
            hasBlockingRatio=False,
            ueCompilerReadiness=False,
        )

    monkeypatch.setattr(
        "app.services.ue5_world_config_handoff_service.validate_episode_spec_scenario_reflection",
        fail_episode_reflection,
    )

    response = _response(include_diagnostics=True)

    assert response.success is False
    assert response.diagnostics is not None
    assert response.diagnostics["failureStage"] == "episode_scenario_reflection"
    trace = response.diagnostics["generationTrace"]
    assert trace["summary"]["status"] == "failed"
    assert trace["summary"]["failureStage"] == "episode_scenario_reflection"
