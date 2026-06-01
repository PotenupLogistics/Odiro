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


def test_openai_failure_can_fallback_to_fake_ollama_success() -> None:
    result = generate_world_config(
        _request(max_repairs=0),
        provider=LlmProvider.openai,
        client_override=FakeOpenAIFailureClient(),
        fallback_client_overrides={LlmProvider.ollama: FakeSuccessClient()},
        settings=Settings(_env_file=None, openaiApiKey="test-key"),
    )

    assert result.success is True
    assert result.generatedPayload is not None
    assert result.fallbackTrace
    assert result.fallbackTrace[0].fromProvider == "openai"
    assert result.fallbackTrace[0].toProvider == "ollama"
    assert result.fallbackTrace[0].success is True


def test_orchestrator_keeps_expected_fastapi_endpoints_and_forbidden_artifacts_absent() -> None:
    route_paths = {route.path for route in app.routes}
    assert "/api/v1/generation/world-config" in route_paths
    assert "/api/v1/generation/world-config/prompt-package" in route_paths
    assert "/api/v1/contracts/validate/{contract_type}" in route_paths
    assert not (ROOT / "samples").exists()
    assert not (ROOT / "fixtures").exists()
    assert not (ROOT / "data" / "rag" / "vector_db").exists()
    assert not (ROOT / "data" / "rag" / "embeddings").exists()
