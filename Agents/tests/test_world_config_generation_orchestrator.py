from __future__ import annotations

import json
from pathlib import Path

from app.main import app
from app.core.settings import Settings
from app.models.generation import WorldConfigGenerationRequest, WorldConfigGenerationConstraints
from app.models.llm import LlmError, LlmGenerationRequest, LlmGenerationResponse, LlmProvider
from app.services.world_config_generation_orchestrator import generate_world_config


ROOT = Path(__file__).resolve().parents[1]


def _request(max_repairs: int = 1) -> WorldConfigGenerationRequest:
    return WorldConfigGenerationRequest(
        schemaVersion="1.0",
        requestId="REQ-GEN-001",
        generationType="world_config",
        prompt="좁은 보도에서 킥보드가 경로를 막고 보행자가 횡단",
        targetContractType="world_config",
        policyId="POLICY-MVP",
        constraints=WorldConfigGenerationConstraints(
            unitSystem="cm_kmh_sec_degree",
            allowedMapTypes=["sidewalk"],
            allowedObjectTypes=["Pedestrian", "Obstacle", "Kickboard"],
            fixedPolicyId="POLICY-MVP",
            defaultSeed=42,
            requireValidation=True,
        ),
        maxRepairAttempts=max_repairs,
    )


def _generic_obstacle_request(max_repairs: int = 1) -> WorldConfigGenerationRequest:
    return WorldConfigGenerationRequest(
        schemaVersion="1.0",
        requestId="REQ-GENERIC-OBSTACLE-001",
        generationType="world_config",
        prompt=(
            "보도 폭은 120cm인 좁은 보도 상황을 만들어줘. "
            "로봇은 x=0, y=0, z=0에서 출발해서 x=800, y=0, z=0으로 이동한다. "
            "로봇 경로 중앙인 x=400, y=0, z=0 근처에 정적 장애물 1개를 배치하고, "
            "장애물이 경로를 막는 정도는 blockingRatio 0.6으로 설정해줘. "
            "보행자는 없는 시나리오로 만들어줘."
        ),
        targetContractType="world_config",
        policyId="POLICY-MVP",
        constraints=WorldConfigGenerationConstraints(
            unitSystem="cm_kmh_sec_degree",
            allowedMapTypes=["sidewalk"],
            allowedObjectTypes=["Pedestrian", "Obstacle", "Kickboard"],
            fixedPolicyId="POLICY-MVP",
            defaultSeed=42,
            requireValidation=True,
        ),
        maxRepairAttempts=max_repairs,
    )


def _environment_sampling_request(max_repairs: int = 1) -> WorldConfigGenerationRequest:
    request = _generic_obstacle_request(max_repairs=max_repairs)
    request.constraints.environmentSampling = {
        "enabled": True,
        "seed": 1001,
        "scenarioType": "obstacle_ahead",
        "fixedParameters": {
            "sidewalkWidthCm": 120,
            "obstacleBlockingRatio": 0.6,
            "timeLimitSec": 60,
        },
    }
    return request


def _environment_sampling_vague_request(max_repairs: int = 1) -> WorldConfigGenerationRequest:
    request = _environment_sampling_request(max_repairs=max_repairs)
    request.prompt = (
        "보도 폭과 장애물 차단 정도는 environmentSampling 결과를 우선 적용해줘. "
        "로봇은 x=0, y=0, z=0에서 출발해서 x=800, y=0, z=0으로 이동한다. "
        "로봇 경로 중앙 근처에 정적 장애물 1개가 경로를 막고 있는 상황을 만들어줘. "
        "보행자는 없는 시나리오로 만들어줘."
    )
    return request


def _valid_world_config() -> dict:
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
        "obstacles": [
            {
                "objectId": "kickboard-1",
                "type": "Kickboard",
                "position": {"x": 50, "y": 0, "z": 0},
                "blockingRatio": 0.8,
            }
        ],
        "pedestrians": [
            {
                "objectId": "ped-1",
                "spawn": {"x": 60, "y": -100, "z": 0},
                "goal": {"x": 60, "y": 100, "z": 0},
                "speedKmh": 4,
                "behavior": "Crossing",
            }
        ],
        "environmentObjects": [],
    }


class FakeSuccessClient:
    def generate(self, request: LlmGenerationRequest) -> LlmGenerationResponse:
        return LlmGenerationResponse(
            requestId=request.requestId,
            provider=request.provider,
            model=request.model,
            success=True,
            content=json.dumps(_valid_world_config(), ensure_ascii=False),
            rawContent=json.dumps(_valid_world_config(), ensure_ascii=False),
            warnings=[],
        )


class FakeInvalidThenRepairClient:
    def __init__(self) -> None:
        self.calls = 0

    def generate(self, request: LlmGenerationRequest) -> LlmGenerationResponse:
        self.calls += 1
        content = '{"schemaVersion": "1.0"}' if self.calls == 1 else json.dumps(_valid_world_config())
        return LlmGenerationResponse(
            requestId=request.requestId,
            provider=request.provider,
            model=request.model,
            success=True,
            content=content,
            rawContent=content,
            warnings=[],
        )


class FakeScenarioRepairClient:
    def __init__(self) -> None:
        self.calls = 0

    def generate(self, request: LlmGenerationRequest) -> LlmGenerationResponse:
        self.calls += 1
        payload = _valid_world_config()
        if self.calls == 1:
            payload["obstacles"] = []
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


class FakeIncompleteScenarioClient:
    def generate(self, request: LlmGenerationRequest) -> LlmGenerationResponse:
        payload = _valid_world_config()
        payload["obstacles"] = []
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


class FakeGenericObstacleIncompleteClient:
    def generate(self, request: LlmGenerationRequest) -> LlmGenerationResponse:
        payload = _valid_world_config()
        payload["map"]["sidewalkWidthCm"] = 150
        payload["robot"]["spawn"] = {"x": 0, "y": 0, "z": 0}
        payload["robot"]["goal"] = {"x": 800, "y": 0, "z": 0}
        payload["obstacles"] = []
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


class CountingSuccessClient(FakeSuccessClient):
    def __init__(self) -> None:
        self.calls = 0

    def generate(self, request: LlmGenerationRequest) -> LlmGenerationResponse:
        self.calls += 1
        return super().generate(request)


class FakeOpenAIFailureClient:
    def generate(self, request: LlmGenerationRequest) -> LlmGenerationResponse:
        return LlmGenerationResponse(
            requestId=request.requestId,
            provider=LlmProvider.openai,
            model=request.model,
            success=False,
            error=LlmError(code="openai_timeout", message="timeout"),
            warnings=[],
        )


class FakeEnvironmentSamplingMismatchClient:
    def generate(self, request: LlmGenerationRequest) -> LlmGenerationResponse:
        payload = _valid_world_config()
        payload["map"]["sidewalkWidthCm"] = 150
        payload["runtime"]["maxDurationSec"] = 120
        payload["robot"]["spawn"] = {"x": 0, "y": 0, "z": 0}
        payload["robot"]["goal"] = {"x": 800, "y": 0, "z": 0}
        payload["obstacles"] = [
            {
                "objectId": "obstacle_001",
                "type": "Obstacle",
                "position": {"x": 400, "y": 0, "z": 0},
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


def test_disabled_provider_returns_failed_generation_without_payload() -> None:
    result = generate_world_config(_request(), provider=LlmProvider.disabled)

    assert result.success is False
    assert result.generatedPayload is None
    assert result.error is not None
    assert result.error.code == "provider_disabled"
    assert result.validation.status == "skipped"
    assert result.retrievedContexts


def test_fake_success_client_generates_valid_payload() -> None:
    result = generate_world_config(
        _request(),
        provider=LlmProvider.custom,
        client_override=FakeSuccessClient(),
    )

    assert result.success is True
    assert result.generatedPayload is not None
    assert result.generatedPayload["worldId"] == "world-1"
    assert result.validation.status == "passed"
    assert result.scenarioReflection is not None
    assert result.scenarioReflection.passed is True
    assert len(result.attempts) == 1


def test_fake_invalid_client_records_repair_attempt() -> None:
    fake_client = FakeInvalidThenRepairClient()

    result = generate_world_config(
        _request(max_repairs=1),
        provider=LlmProvider.custom,
        client_override=fake_client,
    )

    assert result.success is True
    assert fake_client.calls == 2
    assert [attempt.promptType for attempt in result.attempts] == ["initial", "schema_repair"]
    assert result.attempts[0].validationPassed is False
    assert result.attempts[0].validationErrors
    assert result.attempts[0].rawContentPreview
    assert result.attempts[0].rawContentLength > 0
    assert result.attempts[0].jsonExtractionSuccess is True
    assert "schemaVersion" in result.attempts[0].extractedJsonKeys
    assert result.attempts[0].validationErrorSummary
    assert result.attempts[1].repairPromptPreview
    assert result.attempts[1].validationPassed is True


def test_schema_valid_scenario_invalid_payload_uses_post_processing_before_scenario_repair() -> None:
    fake_client = FakeScenarioRepairClient()

    result = generate_world_config(
        _request(max_repairs=1),
        provider=LlmProvider.custom,
        client_override=fake_client,
    )

    assert result.success is True
    assert fake_client.calls == 1
    assert [attempt.promptType for attempt in result.attempts] == ["initial"]
    assert result.attempts[0].validationPassed is True
    assert result.attempts[0].scenarioPostProcessingApplied is True
    assert result.attempts[0].scenarioPostProcessingPatches
    assert result.attempts[0].scenarioReflectionPassed is True
    assert result.scenarioPostProcessing is not None
    assert result.scenarioPostProcessing.applied is True
    assert result.scenarioReflection is not None
    assert result.scenarioReflection.passed is True


def test_schema_valid_scenario_invalid_payload_uses_post_processing_before_repair() -> None:
    result = generate_world_config(
        _request(max_repairs=1),
        provider=LlmProvider.custom,
        client_override=FakeIncompleteScenarioClient(),
    )

    assert result.success is True
    assert result.generatedPayload is not None
    assert result.scenarioPostProcessing is not None
    assert result.scenarioPostProcessing.applied is True
    assert result.scenarioReflection is not None
    assert result.scenarioReflection.passed is True
    assert [attempt.promptType for attempt in result.attempts] == ["initial"]


def test_generic_obstacle_prompt_uses_post_processing_for_missing_obstacle_and_no_pedestrian() -> None:
    result = generate_world_config(
        _generic_obstacle_request(max_repairs=1),
        provider=LlmProvider.custom,
        client_override=FakeGenericObstacleIncompleteClient(),
    )

    assert result.success is True
    assert result.generatedPayload is not None
    assert result.generatedPayload["map"]["sidewalkWidthCm"] == 120
    assert result.generatedPayload["obstacles"][0]["type"] == "Obstacle"
    assert result.generatedPayload["obstacles"][0]["position"] == {"x": 400.0, "y": 0.0, "z": 0.0}
    assert result.generatedPayload["obstacles"][0]["blockingRatio"] == 0.6
    assert result.generatedPayload["pedestrians"] == []
    assert result.scenarioReflection is not None
    assert result.scenarioReflection.passed is True
    assert result.scenarioPostProcessing is not None
    patch_types = {patch.patchType for patch in result.scenarioPostProcessing.patches}
    assert "add_generic_obstacle" in patch_types
    assert "remove_pedestrians_for_no_pedestrian_prompt" in patch_types


def test_environment_sampling_constraints_are_applied_by_orchestrator_post_processing() -> None:
    result = generate_world_config(
        _environment_sampling_request(max_repairs=1),
        provider=LlmProvider.custom,
        client_override=FakeGenericObstacleIncompleteClient(),
    )

    assert result.success is True
    assert result.generatedPayload is not None
    assert result.environmentSampling is not None
    assert result.environmentSampling["parameters"]["sidewalkWidthCm"] == 120
    assert result.generatedPayload["map"]["sidewalkWidthCm"] == 120
    assert result.generatedPayload["obstacles"][0]["blockingRatio"] == 0.6
    assert result.generatedPayload["runtime"]["maxDurationSec"] == 60
    assert result.scenarioPostProcessing is not None
    patch_types = {patch.patchType for patch in result.scenarioPostProcessing.patches}
    assert "set_runtime_limit_from_environment_sampler" in patch_types


def test_environment_sampling_post_processing_runs_even_when_prompt_reflection_passes() -> None:
    result = generate_world_config(
        _environment_sampling_vague_request(max_repairs=1),
        provider=LlmProvider.custom,
        client_override=FakeEnvironmentSamplingMismatchClient(),
    )

    assert result.success is True
    assert result.generatedPayload is not None
    assert result.generatedPayload["map"]["sidewalkWidthCm"] == 120
    assert result.generatedPayload["runtime"]["maxDurationSec"] == 60
    assert result.generatedPayload["obstacles"][0]["blockingRatio"] == 0.6
    assert result.scenarioPostProcessing is not None
    assert result.scenarioPostProcessing.applied is True
    patch_types = {patch.patchType for patch in result.scenarioPostProcessing.patches}
    assert "set_sidewalk_width_from_environment_sampler" in patch_types
    assert "set_runtime_limit_from_environment_sampler" in patch_types
    assert "set_obstacle_blocking_ratio_from_environment_sampler" in patch_types


def test_openai_failure_does_not_fallback_to_ollama_by_default() -> None:
    result = generate_world_config(
        _request(max_repairs=0),
        provider=LlmProvider.openai,
        client_override=FakeOpenAIFailureClient(),
        settings=Settings(_env_file=None, openaiApiKey="test-key"),
    )

    assert result.success is False
    assert result.fallbackTrace == []


def test_openai_failure_does_not_call_ollama_even_when_legacy_flag_is_supplied(monkeypatch) -> None:
    ollama_client = CountingSuccessClient()
    legacy_flag_name = "llm" + "Allow" + "Openai" + "Fallback"

    def fake_create_llm_client(provider: LlmProvider, **kwargs):
        if provider == LlmProvider.ollama:
            return ollama_client
        raise AssertionError(f"Unexpected provider factory call: {provider}")

    monkeypatch.setattr(
        "app.services.world_config_generation_orchestrator.create_llm_client",
        fake_create_llm_client,
    )

    result = generate_world_config(
        _request(max_repairs=0),
        provider=LlmProvider.openai,
        client_override=FakeOpenAIFailureClient(),
        settings=Settings(
            _env_file=None,
            openaiApiKey="test-key",
            llmProviderChain=["openai", "ollama"],
            **{legacy_flag_name: True},
        ),
    )

    assert result.success is False
    assert result.generatedPayload is None
    assert result.fallbackTrace == []
    assert ollama_client.calls == 0


def test_orchestrator_keeps_expected_fastapi_endpoints_and_forbidden_artifacts_absent() -> None:
    route_paths = {route.path for route in app.routes}
    assert "/api/v1/scenarios/generate" in route_paths
    assert "/api/v1/generation/world-config" not in route_paths
    assert "/api/v1/generation/world-config/prompt-package" not in route_paths
    assert "/api/v1/contracts/validate/{contract_type}" not in route_paths
    assert not (ROOT / "samples").exists()
    assert not (ROOT / "fixtures").exists()
    assert not (ROOT / "data" / "rag" / "vector_db").exists()
    assert not (ROOT / "data" / "rag" / "embeddings").exists()
