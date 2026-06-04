from __future__ import annotations

from fastapi.testclient import TestClient

import app.api.routes as routes
from app.main import app
from app.models.run_queue import EpisodeRunQueue, EpisodeRunQueueItem


def test_scenario_generation_route_accepts_prompt_only_and_returns_run_queue(monkeypatch) -> None:
    def stub_generate(request):
        return EpisodeRunQueue(
            runs=[
                EpisodeRunQueueItem(
                    pair_id="obstacle_ahead_000",
                    episode_setup="Json/Input/EpisodeSetup_obstacle_ahead_000.json",
                    delivery_bot_setup="Json/Input/DeliveryBotSetup_obstacle_ahead_000.json",
                )
            ]
        )

    monkeypatch.setattr(routes, "generate_scenario_run_queue", stub_generate)

    response = TestClient(app).post("/api/v1/scenarios/generate", json={"prompt": "장애물이 경로를 막는 상황"})

    assert response.status_code == 200
    payload = response.json()
    assert set(payload) == {"schema", "version", "runs"}
    assert set(payload["runs"][0]) == {"pair_id", "episode_setup", "delivery_bot_setup"}


def test_scenario_generation_route_rejects_extra_fields_and_empty_prompt() -> None:
    client = TestClient(app)

    extra_response = client.post("/api/v1/scenarios/generate", json={"prompt": "x", "episodeCount": 3})
    empty_response = client.post("/api/v1/scenarios/generate", json={"prompt": "   "})

    assert extra_response.status_code == 422
    assert empty_response.status_code == 422
