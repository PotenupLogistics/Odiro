from __future__ import annotations

from fastapi.testclient import TestClient

from app.main import app


def test_health_endpoint_returns_service_status() -> None:
    client = TestClient(app)

    response = client.get("/health")

    assert response.status_code == 200
    assert response.json() == {
        "status": "ok",
        "service": "proto-ai",
        "version": "0.1.0",
    }


def test_api_routes_are_registered() -> None:
    route_paths = {route.path for route in app.routes}

    assert "/health" in route_paths
    assert "/api/v1/scenarios/generate" in route_paths
    assert "/api/v1/scenarios/generate-drive" in route_paths
    assert "/api/v1/scenarios/generate-artifacts" in route_paths
    assert "/api/v1/generation/world-config" not in route_paths
    assert "/api/v1/generation/world-config/prompt-package" not in route_paths
    assert "/api/v1/contracts/validate/{contract_type}" not in route_paths


def test_openapi_exposes_only_scenario_generate_under_api_v1() -> None:
    schema = TestClient(app).get("/openapi.json").json()

    api_v1_paths = sorted(path for path in schema["paths"] if path.startswith("/api/v1/"))

    assert api_v1_paths == [
        "/api/v1/scenarios/generate",
        "/api/v1/scenarios/generate-artifacts",
        "/api/v1/scenarios/generate-drive",
    ]


def test_removed_api_v1_endpoints_return_not_found() -> None:
    client = TestClient(app)

    for path in [
        "/api/v1/generation/world-config",
        "/api/v1/generation/world-config/prompt-package",
        "/api/v1/contracts/validate/world_config",
        "/api/v1/ue5/world-config/handoff",
    ]:
        response = client.post(path, json={})
        assert response.status_code == 404
