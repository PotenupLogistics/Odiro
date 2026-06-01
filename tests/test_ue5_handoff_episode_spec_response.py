from __future__ import annotations

import json

from app.models.handoff import UE5WorldConfigHandoffRequest
from app.models.generation import (
    WorldConfigGenerationConstraints,
    WorldConfigGenerationRequest,
    WorldConfigGenerationResult,
    WorldConfigValidationSummary,
)
from app.models.llm import LlmGenerationRequest, LlmGenerationResponse, LlmProvider
from app.services.world_config_generation_orchestrator import generate_world_config
from app.services.ue5_world_config_handoff_service import create_ue5_world_config_handoff

from tests.test_ue5_world_config_handoff_service import (
    _handoff_request,
    _success_generation_result,
    _world_config,
)


_ENVIRONMENT_SAMPLING_MIDPOINT_PROMPT = (
    "보도 폭과 장애물 차단 정도는 environmentSampling 결과를 우선 적용해줘. "
    "로봇은 x=0, y=0, z=0에서 출발해서 x=800, y=0, z=0으로 이동한다. "
    "로봇 경로 중앙 근처에 정적 장애물 1개가 경로를 막고 있는 상황을 만들어줘. "
    "보행자는 없는 시나리오로 만들어줘."
)


def _environment_sampling_generation_request() -> WorldConfigGenerationRequest:
    return WorldConfigGenerationRequest(
        schemaVersion="1.0",
        requestId="GEN-SAMPLER-FINAL-001",
        generationType="world_config",
        targetContractType="world_config",
        prompt=_ENVIRONMENT_SAMPLING_MIDPOINT_PROMPT,
        policyId="policy_v1_basic_safety",
        maxRepairAttempts=1,
        constraints=WorldConfigGenerationConstraints(
            unitSystem="cm_kmh_sec_degree",
            allowedMapTypes=["Sidewalk"],
            allowedObjectTypes=["Obstacle"],
            fixedPolicyId="policy_v1_basic_safety",
            defaultSeed=1001,
            requireValidation=True,
            environmentSampling={
                "enabled": True,
                "seed": 1001,
                "scenarioType": "obstacle_ahead",
                "fixedParameters": {
                    "sidewalkWidthCm": 120,
                    "obstacleBlockingRatio": 0.6,
                    "timeLimitSec": 60,
                },
            },
        ),
    )


def _environment_sampling_handoff_request() -> UE5WorldConfigHandoffRequest:
    return UE5WorldConfigHandoffRequest(
        schemaVersion="1.0",
        requestId="UE-HANDOFF-SAMPLER-FINAL-001",
        handoffTarget="ue5",
        includeDiagnostics=True,
        responseFormat="episode_spec",
        generationRequest=_environment_sampling_generation_request(),
    )


class FakeGoalPlacedObstacleClient:
    def generate(self, request: LlmGenerationRequest) -> LlmGenerationResponse:
        payload = _world_config()
        payload["scenarioId"] = "scenario-midpoint"
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
        payload["pedestrians"] = [
            {
                "objectId": "ped-wrong",
                "spawn": {"x": 400, "y": -100, "z": 0},
                "goal": {"x": 400, "y": 100, "z": 0},
                "speedKmh": 3,
                "behavior": "Walking",
            }
        ]
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


def test_handoff_generation_path_removes_penalties_and_corrects_route_midpoint() -> None:
    fake_client = FakeGoalPlacedObstacleClient()
    response = create_ue5_world_config_handoff(
        _environment_sampling_handoff_request(),
        provider=LlmProvider.custom,
        generator=lambda generation_request, provider: generate_world_config(
            generation_request,
            provider=provider,
            client_override=fake_client,
            allow_fallback=False,
        ),
    )

    assert response.success is True
    assert response.episodeSpec is not None
    region = response.episodeSpec["ground_model"]["regions"][0]
    assert "penalties" not in region
    assert region["shape"]["size_m"] == [10.0, 1.2]
    assert response.episodeValidation is not None
    assert response.episodeValidation.valid is True
    assert response.episodeScenarioReflection is not None
    assert response.episodeScenarioReflection.passed is True
    assert response.episodeScenarioReflection.ueCompilerReadiness is True

    static_obstacles = response.episodeSpec["actors"]["static_obstacles"]
    assert len(static_obstacles) == 1
    obstacle = static_obstacles[0]
    assert obstacle["prop_id"] == "obstacle.box_01"
    assert obstacle["transform"]["location_m"] == [4.0, 0.0, 0.0]
    assert obstacle["properties"]["semantic_type"] == "Obstacle"
    assert obstacle["properties"]["blocking_ratio"] == 0.6
    assert response.episodeSpec["run"]["time_limit_s"] == 60.0
    assert response.episodeSpec["actors"]["pedestrians"] == []
    assert response.episodeSpec["paths"] == []
