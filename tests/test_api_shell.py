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
    assert "/api/v1/generation/world-config" in route_paths
    assert "/api/v1/generation/world-config/prompt-package" in route_paths
    assert "/api/v1/contracts/validate/{contract_type}" in route_paths
