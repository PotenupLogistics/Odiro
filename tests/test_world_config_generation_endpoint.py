from __future__ import annotations

from pathlib import Path

from fastapi.testclient import TestClient

import app.api.routes as routes
from app.main import app
from app.models.generation import WorldConfigGenerationResult, WorldConfigValidationSummary
from app.models.scenario import ScenarioPostProcessPatch, ScenarioPostProcessResult


ROOT = Path(__file__).resolve().parents[1]


def _generation_request() -> dict:
    return {
        "schemaVersion": "1.0",
        "requestId": "REQ-GENERATE-ENDPOINT-001",
        "generationType": "world_config",
        "prompt": "좁은 보도에서 킥보드가 경로를 막고 보행자가 횡단",
        "targetContractType": "world_config",
        "policyId": "POLICY-MVP",
        "constraints": {
            "unitSystem": "cm_kmh_sec_degree",
            "allowedMapTypes": ["sidewalk"],
            "allowedObjectTypes": ["Pedestrian", "Obstacle", "Kickboard"],
            "fixedPolicyId": "POLICY-MVP",
            "defaultSeed": 12,
            "requireValidation": True,
        },
        "maxRepairAttempts": 2,
    }


def _korean_generation_request() -> dict:
    payload = _generation_request()
    payload["prompt"] = "좁은 보도에서 공유 킥보드가 로봇 경로를 막고, 오른쪽에서 보행자가 횡단하는 상황을 만들어줘."
    return payload


def test_generation_endpoint_disabled_provider_returns_failed_result() -> None:
    client = TestClient(app)

    response = client.post("/api/v1/generation/world-config", json=_generation_request())

    assert response.status_code == 200
    payload = response.json()
    assert payload["success"] is False
    assert payload["error"]["code"] == "provider_disabled"
    assert payload["generatedPayload"] is None
    assert payload["retrievedContexts"]
    assert payload["attempts"]


def test_generation_endpoint_openai_provider_can_use_mocked_orchestrator(monkeypatch) -> None:
    def fake_generate_world_config(request, provider):
        return WorldConfigGenerationResult(
            requestId=request.requestId,
            generationType="world_config",
            targetContractType="world_config",
            success=False,
            generatedPayload=None,
            validation=WorldConfigValidationSummary(status="skipped"),
            attempts=[],
            retrievedContexts=[],
            assumptions=[],
            warnings=["mocked openai path"],
            error=None,
        )

    monkeypatch.setattr(routes, "generate_world_config", fake_generate_world_config)
    client = TestClient(app)

    response = client.post(
        "/api/v1/generation/world-config?provider=openai",
        json=_generation_request(),
    )

    assert response.status_code == 200
    assert response.json()["warnings"] == ["mocked openai path"]


def test_generation_endpoint_ollama_provider_can_use_mocked_orchestrator(monkeypatch) -> None:
    def fake_generate_world_config(request, provider):
        return WorldConfigGenerationResult(
            requestId=request.requestId,
            generationType="world_config",
            targetContractType="world_config",
            success=False,
            generatedPayload=None,
            validation=WorldConfigValidationSummary(status="skipped"),
            attempts=[],
            retrievedContexts=[],
            assumptions=[],
            warnings=["mocked ollama path"],
            error=None,
        )

    monkeypatch.setattr(routes, "generate_world_config", fake_generate_world_config)
    client = TestClient(app)

    response = client.post(
        "/api/v1/generation/world-config?provider=ollama",
        json=_generation_request(),
    )

    assert response.status_code == 200
    assert response.json()["warnings"] == ["mocked ollama path"]


def test_generation_endpoint_serializes_scenario_post_processing(monkeypatch) -> None:
    def fake_generate_world_config(request, provider):
        return WorldConfigGenerationResult(
            requestId=request.requestId,
            generationType="world_config",
            targetContractType="world_config",
            success=True,
            generatedPayload={"schemaVersion": "1.0"},
            validation=WorldConfigValidationSummary(status="passed"),
            attempts=[],
            retrievedContexts=[],
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
                patchedPayload={"schemaVersion": "1.0"},
            ),
            assumptions=[],
            warnings=[],
            error=None,
        )

    monkeypatch.setattr(routes, "generate_world_config", fake_generate_world_config)
    client = TestClient(app)

    response = client.post(
        "/api/v1/generation/world-config?provider=ollama",
        json=_generation_request(),
    )

    assert response.status_code == 200
    payload = response.json()
    assert payload["scenarioPostProcessing"]["applied"] is True
    assert payload["scenarioPostProcessing"]["patches"][0]["patchType"] == "add_kickboard_obstacle"


def test_existing_prompt_package_endpoint_still_works() -> None:
    client = TestClient(app)

    response = client.post(
        "/api/v1/generation/world-config/prompt-package",
        json=_generation_request(),
    )

    assert response.status_code == 200
    assert response.json()["retrievedContexts"]


def test_prompt_package_endpoint_returns_context_and_scenario_intent_for_korean_prompt() -> None:
    client = TestClient(app)

    response = client.post(
        "/api/v1/generation/world-config/prompt-package",
        json=_korean_generation_request(),
    )

    assert response.status_code == 200
    payload = response.json()
    assert len(payload["retrievedContexts"]) >= 1
    assert payload["scenarioIntent"]["pathBlockingHints"] is True
    assert payload["scenarioRequirements"]


def test_generation_endpoint_does_not_create_forbidden_artifacts() -> None:
    client = TestClient(app)
    response = client.post("/api/v1/generation/world-config", json=_generation_request())
    assert response.status_code == 200

    assert not (ROOT / "samples").exists()
    assert not (ROOT / "fixtures").exists()
    assert not (ROOT / "data" / "rag" / "vector_db").exists()
    assert not (ROOT / "data" / "rag" / "embeddings").exists()
