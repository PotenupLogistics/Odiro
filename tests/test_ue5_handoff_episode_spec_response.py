from __future__ import annotations

from app.models.handoff import UE5WorldConfigHandoffRequest
from app.models.generation import WorldConfigGenerationResult, WorldConfigValidationSummary
from app.models.llm import LlmProvider
from app.services.ue5_world_config_handoff_service import create_ue5_world_config_handoff

from tests.test_ue5_world_config_handoff_service import (
    _handoff_request,
    _success_generation_result,
    _world_config,
)


def test_handoff_response_can_include_episode_spec_only() -> None:
    base_request = _handoff_request()
    request_payload = base_request.model_dump()
    request_payload.pop("responseFormat", None)
    request = UE5WorldConfigHandoffRequest(
        **request_payload,
        responseFormat="episode_spec",
    )

    response = create_ue5_world_config_handoff(
        request,
        provider=LlmProvider.disabled,
        generator=lambda generation_request, provider: _success_generation_result(),
    )

    assert response.success is True
    assert response.worldConfig is None
    assert response.episodeSpec is not None
    assert response.episodeSpec["schema"] == "episode_actor_spawn_mvp"
    assert response.episodeValidation is not None
    assert response.episodeValidation.valid is True
    assert response.episodeScenarioReflection is not None
    assert response.episodeScenarioReflection.passed is True
    assert response.conversionWarnings


def test_handoff_request_defaults_to_episode_spec_response_format() -> None:
    base_request = _handoff_request()
    request_payload = base_request.model_dump()
    request_payload.pop("responseFormat", None)
    request = UE5WorldConfigHandoffRequest(**request_payload)

    response = create_ue5_world_config_handoff(
        request,
        provider=LlmProvider.disabled,
        generator=lambda generation_request, provider: _success_generation_result(),
    )

    assert request.responseFormat == "episode_spec"
    assert response.success is True
    assert response.worldConfig is None
    assert response.episodeSpec is not None
    assert response.diagnostics is not None
    assert response.diagnostics["effectiveResponseFormat"] == "episode_spec"


def test_handoff_response_can_include_both_world_config_and_episode_spec() -> None:
    base_request = _handoff_request()
    request_payload = base_request.model_dump()
    request_payload.pop("responseFormat", None)
    request = UE5WorldConfigHandoffRequest(**request_payload, responseFormat="both")

    response = create_ue5_world_config_handoff(
        request,
        provider=LlmProvider.disabled,
        generator=lambda generation_request, provider: _success_generation_result(),
    )

    assert response.success is True
    assert response.worldConfig is not None
    assert response.episodeSpec is not None


def test_episode_spec_response_fails_when_obstacle_requirement_loses_static_obstacles() -> None:
    payload = _world_config()
    payload["obstacles"] = []
    payload["pedestrians"] = []
    generation_result = WorldConfigGenerationResult(
        requestId="REQ-HANDOFF-001",
        generationType="world_config",
        targetContractType="world_config",
        success=True,
        generatedPayload=payload,
        validation=WorldConfigValidationSummary(status="passed"),
        attempts=[],
        retrievedContexts=[],
        warnings=[],
    )
    request = _handoff_request()

    response = create_ue5_world_config_handoff(
        request,
        provider=LlmProvider.disabled,
        generator=lambda generation_request, provider: generation_result,
    )

    assert request.responseFormat == "episode_spec"
    assert response.success is False
    assert response.episodeSpec is None
    assert response.episodeScenarioReflection is not None
    assert response.episodeScenarioReflection.passed is False
    assert response.episodeScenarioReflection.staticObstacleCount == 0
    assert response.episodeScenarioReflection.ueCompilerReadiness is False
