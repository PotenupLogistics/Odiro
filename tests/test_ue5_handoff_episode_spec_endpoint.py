from __future__ import annotations

from fastapi.testclient import TestClient

from app.main import app
from tests.test_ue5_world_config_handoff_endpoint import _handoff_request


def test_handoff_endpoint_accepts_response_format_query_parameter() -> None:
    client = TestClient(app)

    response = client.post(
        "/api/v1/ue5/world-config/handoff?provider=disabled&responseFormat=episode_spec",
        json=_handoff_request(),
    )

    assert response.status_code == 200
    payload = response.json()
    assert payload["success"] is False
    assert "episodeSpec" in payload
    assert payload["episodeSpec"] is None
    assert payload["error"]["code"] == "provider_disabled"

