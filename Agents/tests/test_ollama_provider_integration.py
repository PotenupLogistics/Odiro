from __future__ import annotations

import json
from pathlib import Path

from app.models.generation import WorldConfigGenerationConstraints, WorldConfigGenerationRequest
from app.models.llm import LlmGenerationRequest, LlmGenerationResponse, LlmProvider
from app.services.llm_client_factory import create_llm_client
from app.services.llm_ollama_client import OllamaLlmClient
from app.services.world_config_generation_orchestrator import generate_world_config


ROOT = Path(__file__).resolve().parents[1]


def _request() -> WorldConfigGenerationRequest:
    return WorldConfigGenerationRequest(
        schemaVersion="1.0",
        requestId="REQ-OLLAMA-ORCH-001",
        generationType="world_config",
        prompt="좁은 보도에서 킥보드가 경로를 막고 보행자가 횡단",
        targetContractType="world_config",
        policyId="POLICY-MVP",
        constraints=WorldConfigGenerationConstraints(
            unitSystem="cm_kmh_sec_degree",
            allowedMapTypes=["sidewalk"],
            allowedObjectTypes=["Pedestrian", "Obstacle", "Kickboard"],
            fixedPolicyId="POLICY-MVP",
            defaultSeed=99,
            requireValidation=True,
        ),
        maxRepairAttempts=1,
    )


def _valid_world_config() -> dict:
    location = {"x": 0, "y": 0, "z": 0}
    return {
        "schemaVersion": "1.0",
        "worldId": "world-ollama",
        "scenarioId": "scenario-ollama",
        "seed": 99,
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
                "position": {"x": 40, "y": 0, "z": 0},
                "blockingRatio": 0.8,
            }
        ],
        "pedestrians": [
            {
                "objectId": "ped-1",
                "spawn": {"x": 50, "y": -100, "z": 0},
                "goal": {"x": 50, "y": 100, "z": 0},
                "speedKmh": 4,
                "behavior": "Crossing",
            }
        ],
        "environmentObjects": [],
    }


class FakeOllamaClient:
    def generate(self, request: LlmGenerationRequest) -> LlmGenerationResponse:
        return LlmGenerationResponse(
            requestId=request.requestId,
            provider=LlmProvider.ollama,
            model=request.model,
            success=True,
            content=json.dumps(_valid_world_config(), ensure_ascii=False),
            rawContent=json.dumps(_valid_world_config(), ensure_ascii=False),
            warnings=[],
        )


def test_factory_returns_ollama_client_for_ollama_provider() -> None:
    assert isinstance(create_llm_client(LlmProvider.ollama), OllamaLlmClient)


def test_orchestrator_with_fake_ollama_client_returns_valid_payload() -> None:
    result = generate_world_config(
        _request(),
        provider=LlmProvider.ollama,
        client_override=FakeOllamaClient(),
    )

    assert result.success is True
    assert result.generatedPayload is not None
    assert result.generatedPayload["worldId"] == "world-ollama"
    assert result.validation.status == "passed"
    assert result.scenarioReflection is not None
    assert result.scenarioReflection.passed is True


def test_no_sample_fixture_vector_or_embedding_artifacts_exist() -> None:
    assert not (ROOT / "samples").exists()
    assert not (ROOT / "fixtures").exists()
    assert not (ROOT / "data" / "rag" / "vector_db").exists()
    assert not (ROOT / "data" / "rag" / "embeddings").exists()
