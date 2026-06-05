from __future__ import annotations

from fastapi.testclient import TestClient

import app.api.routes as routes
from app.core.settings import Settings
from app.main import app
from app.models.run_queue import EpisodeRunQueue, EpisodeRunQueueItem


def _queue(run_count: int = 1) -> EpisodeRunQueue:
    return EpisodeRunQueue(
        runs=[
            EpisodeRunQueueItem(
                pair_id=f"obstacle_ahead_{index:03d}",
                episode_setup=f"Json/Input/EpisodeSetup_obstacle_ahead_{index:03d}.json",
                delivery_bot_setup=f"Json/Input/DeliveryBotSetup_obstacle_ahead_{index:03d}.json",
            )
            for index in range(run_count)
        ]
    )


def test_scenario_generation_route_accepts_prompt_only_and_returns_run_queue(monkeypatch) -> None:
    def stub_generate(request):
        assert request.episode_count is None
        return _queue(run_count=Settings().scenarioEpisodeDefaultCount)

    monkeypatch.setattr(routes, "generate_scenario_run_queue", stub_generate)

    response = TestClient(app).post("/api/v1/scenarios/generate", json={"prompt": "장애물이 경로를 막는 상황"})

    assert response.status_code == 200
    payload = response.json()
    assert set(payload) == {"schema", "version", "runs"}
    assert len(payload["runs"]) == Settings().scenarioEpisodeDefaultCount
    assert set(payload["runs"][0]) == {"pair_id", "episode_setup", "delivery_bot_setup"}


def test_scenario_generation_route_accepts_optional_episode_count(monkeypatch) -> None:
    observed_counts = []

    def stub_generate(request):
        observed_counts.append(request.episode_count)
        return _queue(run_count=request.episode_count)

    monkeypatch.setattr(routes, "generate_scenario_run_queue", stub_generate)
    client = TestClient(app)

    one_response = client.post("/api/v1/scenarios/generate", json={"prompt": "test", "episode_count": 1})
    three_response = client.post("/api/v1/scenarios/generate", json={"prompt": "test", "episode_count": 3})

    assert one_response.status_code == 200
    assert three_response.status_code == 200
    assert len(one_response.json()["runs"]) == 1
    assert len(three_response.json()["runs"]) == 3
    assert observed_counts == [1, 3]


def test_scenario_generation_route_accepts_episode_count_at_max(monkeypatch) -> None:
    max_count = Settings().scenarioEpisodeMaxCount

    def stub_generate(request):
        return _queue(run_count=request.episode_count)

    monkeypatch.setattr(routes, "generate_scenario_run_queue", stub_generate)

    response = TestClient(app).post("/api/v1/scenarios/generate", json={"prompt": "test", "episode_count": max_count})

    assert response.status_code == 200
    assert len(response.json()["runs"]) == max_count


def test_scenario_generation_route_rejects_extra_fields_and_empty_prompt() -> None:
    client = TestClient(app)

    extra_response = client.post("/api/v1/scenarios/generate", json={"prompt": "x", "episodeCount": 3})
    empty_response = client.post("/api/v1/scenarios/generate", json={"prompt": "   "})

    assert extra_response.status_code == 422
    assert empty_response.status_code == 422


def test_scenario_generation_route_rejects_invalid_episode_count_values() -> None:
    client = TestClient(app)

    max_count = Settings().scenarioEpisodeMaxCount
    for value in [0, -1, 1.5, "3", max_count + 1]:
        response = client.post("/api/v1/scenarios/generate", json={"prompt": "test", "episode_count": value})
        assert response.status_code == 422


def test_scenario_generation_openapi_marks_episode_count_optional() -> None:
    schema = TestClient(app).get("/openapi.json").json()
    request_ref = schema["paths"]["/api/v1/scenarios/generate"]["post"]["requestBody"]["content"]["application/json"]["schema"]["$ref"]
    component_name = request_ref.rsplit("/", 1)[-1]
    request_schema = schema["components"]["schemas"][component_name]

    assert request_schema["required"] == ["prompt"]
    assert "episode_count" in request_schema["properties"]
    assert "minimum" in request_schema["properties"]["episode_count"]["anyOf"][0]
    assert request_schema["properties"]["episode_count"]["anyOf"][0]["maximum"] == Settings().scenarioEpisodeMaxCount
