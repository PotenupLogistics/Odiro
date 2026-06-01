from __future__ import annotations

from app.models.handoff import UE5WorldConfigHandoffRequest
from app.models.llm import LlmProvider
from app.services.ue5_world_config_handoff_service import create_ue5_world_config_handoff

from tests.test_ue5_world_config_handoff_service import (
    _handoff_request,
    _success_generation_result,
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
