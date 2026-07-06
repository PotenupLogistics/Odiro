from __future__ import annotations

import pytest
from fastapi.testclient import TestClient
from pydantic import ValidationError

from app.main import app
from app.models.scenario_generation import ScenarioGenerateRequest


def test_v1_scenario_generate_returns_run_queue_removed_notice() -> None:
    response = TestClient(app).post("/api/v1/scenarios/generate", json={"prompt": "장애물이 경로를 막는 상황"})

    assert response.status_code == 410
    assert response.json()["detail"] == {
        "code": "RUN_QUEUE_REMOVED",
        "message": "RunQueue scenario generation was removed. Use /api/v2/scenarios/generate and user project runs.",
    }


def test_v1_scenario_generate_does_not_validate_legacy_body_shape() -> None:
    response = TestClient(app).post("/api/v1/scenarios/generate", json={"legacy": "payload"})

    assert response.status_code == 410
    assert response.json()["detail"]["code"] == "RUN_QUEUE_REMOVED"


def test_legacy_scenario_generate_request_validates_episode_count_and_extra_fields() -> None:
    """Keep the removed v1 request model strict for archived RunQueue tooling."""
    assert ScenarioGenerateRequest(prompt="장애물 경로", episode_count=2).episode_count == 2

    for payload in (
        {"prompt": "장애물 경로", "episode_count": 1.5},
        {"prompt": "장애물 경로", "episode_count": "3"},
        {"prompt": "장애물 경로", "episodeCount": 3},
        {"prompt": "장애물 경로", "episode_count": 1, "extra": "value"},
    ):
        with pytest.raises(ValidationError):
            ScenarioGenerateRequest(**payload)


def test_openapi_keeps_only_supported_api_v1_routes() -> None:
    schema = TestClient(app).get("/openapi.json").json()

    api_v1_paths = sorted(path for path in schema["paths"] if path.startswith("/api/v1/"))

    assert api_v1_paths == [
        "/api/v1/analysis/run",
        "/api/v1/scenarios/generate",
    ]


def test_removed_scenario_generation_endpoints_return_not_found() -> None:
    client = TestClient(app)

    artifacts_response = client.post("/api/v1/scenarios/generate-artifacts", json={"prompt": "test"})
    drive_response = client.post("/api/v1/scenarios/generate-drive", json={"prompt": "test"})

    assert artifacts_response.status_code == 404
    assert drive_response.status_code == 404
