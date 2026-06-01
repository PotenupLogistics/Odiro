from __future__ import annotations

from pathlib import Path

from fastapi.testclient import TestClient

from app.main import app


ROOT = Path(__file__).resolve().parents[1]


def _generation_request() -> dict:
    return {
        "schemaVersion": "1.0",
        "requestId": "REQ-API-PROMPT-001",
        "generationType": "world_config",
        "prompt": "좁은 보도에서 킥보드가 경로를 막고 보행자가 횡단",
        "targetContractType": "world_config",
        "policyId": "POLICY-MVP",
        "constraints": {
            "unitSystem": "cm_kmh_sec_degree",
            "allowedMapTypes": ["sidewalk"],
            "allowedObjectTypes": ["Pedestrian", "Obstacle", "Kickboard"],
            "fixedPolicyId": "POLICY-MVP",
            "defaultSeed": 11,
            "requireValidation": True,
        },
        "maxRepairAttempts": 2,
    }


def test_prompt_package_endpoint_returns_prompt_package_without_llm_output() -> None:
    client = TestClient(app)

    response = client.post(
        "/api/v1/generation/world-config/prompt-package",
        json=_generation_request(),
    )

    assert response.status_code == 200
    payload = response.json()
    assert payload["requestId"] == "REQ-API-PROMPT-001"
    assert payload["systemPrompt"]
    assert payload["userPrompt"]
    assert payload["retrievedContexts"]
    assert payload["schemaSummary"]
    assert payload["validationPolicy"]
    assert "generatedPayload" not in payload


def test_prompt_package_endpoint_does_not_create_forbidden_artifacts() -> None:
    client = TestClient(app)
    response = client.post(
        "/api/v1/generation/world-config/prompt-package",
        json=_generation_request(),
    )
    assert response.status_code == 200

    assert not (ROOT / "samples").exists()
    assert not (ROOT / "fixtures").exists()
    assert not (ROOT / "data" / "rag" / "vector_db").exists()
    assert not (ROOT / "data" / "rag" / "embeddings").exists()

