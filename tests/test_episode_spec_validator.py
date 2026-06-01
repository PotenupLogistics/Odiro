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

