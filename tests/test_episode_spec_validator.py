from __future__ import annotations

from app.services.episode_spec_validator import validate_episode_spec
from app.services.world_config_to_episode_spec_adapter import convert_world_config_to_episode_spec

from tests.test_world_config_to_episode_spec_adapter import _world_config


def test_validator_accepts_valid_episode_spec() -> None:
    episode = convert_world_config_to_episode_spec(_world_config())

    result = validate_episode_spec(episode)

    assert result.valid is True
    assert result.errors == []


def test_validator_rejects_duplicate_instance_ids() -> None:
    episode = convert_world_config_to_episode_spec(_world_config())
    duplicate = episode.model_copy(deep=True)
    duplicate.actors.static_obstacles[0].instance_id = duplicate.actors.robot.instance_id

    result = validate_episode_spec(duplicate)

    assert result.valid is False
    assert any(error.code == "duplicate_instance_id" for error in result.errors)


def test_validator_rejects_unknown_static_obstacle_prop_id() -> None:
    episode = convert_world_config_to_episode_spec(_world_config())
    invalid = episode.model_copy(deep=True)
    invalid.actors.static_obstacles[0].prop_id = "obstacle.kickboard"

    result = validate_episode_spec(invalid)

    assert result.valid is False
    assert any(error.code == "unknown_prop_id" for error in result.errors)


def test_validator_rejects_guide_incompatible_penalties_field() -> None:
    episode = convert_world_config_to_episode_spec(_world_config()).model_dump(mode="json", by_alias=True)
    episode["ground_model"]["regions"][0]["penalties"] = []

    result = validate_episode_spec(episode)

    assert result.valid is False
    assert any(error.code == "model_validation_error" and "penalties" in str(error.path) for error in result.errors)


def test_validator_rejects_nested_or_general_array_properties() -> None:
    episode = convert_world_config_to_episode_spec(_world_config()).model_dump(mode="json", by_alias=True)
    episode["actors"]["static_obstacles"][0]["properties"]["nested"] = {"bad": True}
    episode["actors"]["static_obstacles"][0]["properties"]["general_array"] = [1.0, 2.0]

    result = validate_episode_spec(episode)

    assert result.valid is False
    assert sum(1 for error in result.errors if error.code == "invalid_property_value") >= 2
