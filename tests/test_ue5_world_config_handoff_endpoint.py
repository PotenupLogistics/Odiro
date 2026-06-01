from __future__ import annotations

from pathlib import Path

from fastapi.testclient import TestClient

import app.api.routes as routes
from app.main import app
from app.models.generation import WorldConfigGenerationError
from app.models.llm import LlmProvider
from app.services.ue5_world_config_handoff_service import create_ue5_world_config_handoff


ROOT = Path(__file__).resolve().parents[1]


def _handoff_request() -> dict:
    return {
        "schemaVersion": "1.0",
        "requestId": "HANDOFF-ENDPOINT-001",
        "handoffTarget": "ue5",
        "includeDiagnostics": True,
        "generationRequest": {
            "schemaVersion": "1.0",
            "requestId": "REQ-HANDOFF-ENDPOINT-001",
            "generationType": "world_config",
            "prompt": "좁은 보도에서 공유 킥보드가 로봇 경로를 막고 보행자가 횡단",
            "targetContractType": "world_config",
            "policyId": "policy_v1_basic_safety",
            "constraints": {
                "unitSystem": "cm_kmh_sec_degree",
                "allowedMapTypes": ["Sidewalk"],
                "allowedObjectTypes": ["Pedestrian", "Kickboard", "Obstacle"],
                "fixedPolicyId": "policy_v1_basic_safety",
                "defaultSeed": 1001,
                "requireValidation": True,
            },
            "maxRepairAttempts": 1,
        },
    }


def test_ue5_handoff_endpoint_exists_and_disabled_provider_fails_clearly() -> None:
    client = TestClient(app)

    response = client.post(
        "/api/v1/ue5/world-config/handoff?provider=disabled",
        json=_handoff_request(),
    )

    assert response.status_code == 200
    payload = response.json()
    assert payload["success"] is False
    assert payload["worldConfig"] is None
    assert payload["error"]["code"] == "provider_disabled"
    assert payload["metadata"]["handoffTarget"] == "ue5"
    assert payload["metadata"]["units"]["distance"] == "cm"


def test_ue5_handoff_endpoint_openai_provider_can_use_mocked_service(monkeypatch) -> None:
    def fake_handoff(request, provider):
        response = create_ue5_world_config_handoff(request, provider=LlmProvider.disabled)
        response.metadata.provider = provider.value
        response.error = WorldConfigGenerationError(code="mocked_openai", message="mocked")
        return response

    monkeypatch.setattr(routes, "create_ue5_world_config_handoff", fake_handoff)
    client = TestClient(app)

    response = client.post(
        "/api/v1/ue5/world-config/handoff?provider=openai",
        json=_handoff_request(),
    )

    assert response.status_code == 200
    payload = response.json()
    assert payload["success"] is False
    assert payload["worldConfig"] is None
    assert payload["metadata"]["provider"] == "openai"
    assert payload["error"]["code"] == "mocked_openai"


def test_existing_generation_and_contract_routes_remain_registered() -> None:
    route_paths = {route.path for route in app.routes}
    assert "/api/v1/ue5/world-config/handoff" in route_paths
    assert "/api/v1/generation/world-config" in route_paths
    assert "/api/v1/generation/world-config/prompt-package" in route_paths
    assert "/api/v1/contracts/validate/{contract_type}" in route_paths


def test_ue5_handoff_endpoint_does_not_create_forbidden_artifacts() -> None:
    client = TestClient(app)
    response = client.post(
        "/api/v1/ue5/world-config/handoff?provider=disabled",
        json=_handoff_request(),
    )
    assert response.status_code == 200

    assert not (ROOT / "samples").exists()
    assert not (ROOT / "fixtures").exists()
    assert not (ROOT / "data" / "rag" / "vector_db").exists()
    assert not (ROOT / "data" / "rag" / "embeddings").exists()
