from __future__ import annotations

from app.models.handoff import UE5WorldConfigHandoffRequest
from app.models.llm import LlmProvider
from app.services.ue5_world_config_handoff_service import create_ue5_world_config_handoff

from tests.test_ue5_world_config_handoff_service import _handoff_request, _success_generation_result


def _setup_pair_request() -> UE5WorldConfigHandoffRequest:
    base = _handoff_request()
    return base.model_copy(update={"responseFormat": "setup_pair", "includeDiagnostics": True})


def _contains_key(value, key: str) -> bool:
    if isinstance(value, dict):
        return key in value or any(_contains_key(child, key) for child in value.values())
    if isinstance(value, list):
        return any(_contains_key(child, key) for child in value)
    return False


def _contains_none(value) -> bool:
    if value is None:
        return True
    if isinstance(value, dict):
        return any(_contains_none(child) for child in value.values())
    if isinstance(value, list):
        return any(_contains_none(child) for child in value)
    return False


def test_handoff_response_can_include_setup_pair_only() -> None:
    response = create_ue5_world_config_handoff(
        _setup_pair_request(),
        provider=LlmProvider.disabled,
        generator=lambda generation_request, provider: _success_generation_result(),
    )

    assert response.success is True
    assert response.worldConfig is None
    assert response.episodeSpec is None
    assert response.episodeSetup is not None
    assert response.deliveryBotSetup is not None
    assert response.episodeSetupValidation is not None
    assert response.episodeSetupValidation.valid is True
    assert response.deliveryBotSetupValidation is not None
    assert response.deliveryBotSetupValidation.valid is True
    assert response.diagnostics is not None
    assert response.diagnostics["effectiveResponseFormat"] == "setup_pair"
    assert response.diagnostics["setupPairTrace"]


def test_setup_pair_response_uses_latest_ue_contract_fields_without_legacy_fields() -> None:
    response = create_ue5_world_config_handoff(
        _setup_pair_request(),
        provider=LlmProvider.disabled,
        generator=lambda generation_request, provider: _success_generation_result(),
    )

    episode_setup = response.episodeSetup or {}
    delivery_bot_setup = response.deliveryBotSetup or {}

    assert _contains_key(episode_setup, "xy_m")
    assert _contains_key(episode_setup, "goal_xy_m")
    assert _contains_key(episode_setup, "center_xy_m")
    assert _contains_key(episode_setup, "points_xy_m")
    for key in ["units", "transform", "location_m", "rotation_deg", "scale"]:
        assert not _contains_key(episode_setup, key)

    for key in ["route", "instance_id", "xy_m", "yaw_deg", "location", "transform"]:
        assert not _contains_key(delivery_bot_setup, key)
    assert set(delivery_bot_setup["robot"]) == {"drive", "path_follow", "lidar"}
    assert _contains_none(episode_setup) is False
    assert _contains_none(delivery_bot_setup) is False


def test_setup_pair_trace_does_not_store_full_payloads_or_secrets() -> None:
    response = create_ue5_world_config_handoff(
        _setup_pair_request(),
        provider=LlmProvider.disabled,
        generator=lambda generation_request, provider: _success_generation_result(),
    )

    assert response.diagnostics is not None
    trace_text = str(response.diagnostics["setupPairTrace"])

    assert "map.sidewalkWidthCm" in trace_text
    assert "robot.spawn" in trace_text
    assert "robot.goal" in trace_text
    assert "obstacle.position" in trace_text
    assert "runtime.maxDurationSec" in trace_text
    assert "OPENAI_API_KEY" not in trace_text
    assert "rawContent" not in trace_text
    assert "worldConfig" not in trace_text
    assert "episodeSetup" not in trace_text
    assert "deliveryBotSetup" not in trace_text
