from __future__ import annotations

from pathlib import Path

from fastapi.testclient import TestClient

from app.main import app


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


def test_ue5_handoff_endpoint_is_removed_and_returns_not_found() -> None:
    client = TestClient(app)

    response = client.post(
        "/api/v1/ue5/world-config/handoff?provider=disabled",
        json=_handoff_request(),
    )

    assert response.status_code == 404


def test_existing_generation_and_contract_routes_remain_registered() -> None:
    route_paths = {route.path for route in app.routes}
    assert "/api/v1/ue5/world-config/handoff" not in route_paths
    assert "/api/v1/generation/world-config" in route_paths
    assert "/api/v1/generation/world-config/prompt-package" in route_paths
    assert "/api/v1/scenarios/generate" in route_paths
    assert "/api/v1/contracts/validate/{contract_type}" in route_paths


def test_ue5_handoff_endpoint_is_not_exposed_in_openapi() -> None:
    schema = TestClient(app).get("/openapi.json").json()

    assert "/api/v1/ue5/world-config/handoff" not in schema["paths"]
    assert "/api/v1/scenarios/generate" in schema["paths"]


def test_removed_ue5_handoff_endpoint_does_not_create_forbidden_artifacts() -> None:
    assert not (ROOT / "samples").exists()
    assert not (ROOT / "fixtures").exists()
    assert not (ROOT / "data" / "rag" / "vector_db").exists()
    assert not (ROOT / "data" / "rag" / "embeddings").exists()
