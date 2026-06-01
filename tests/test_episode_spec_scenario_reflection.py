from __future__ import annotations

from app.services.episode_spec_scenario_reflection import validate_episode_spec_scenario_reflection
from app.services.world_config_to_episode_spec_adapter import convert_world_config_to_episode_spec

from tests.test_world_config_to_episode_spec_adapter import _world_config


PROMPT = "좁은 보도에서 공유 킥보드가 로봇 경로를 막고, 오른쪽에서 보행자가 횡단하는 상황"


def _episode_dict() -> dict:
    return convert_world_config_to_episode_spec(_world_config()).model_dump(mode="json", by_alias=True)


def test_reflection_fails_when_kickboard_semantic_is_missing() -> None:
    episode = _episode_dict()
    episode["actors"]["static_obstacles"][0]["properties"].pop("semantic_type")

    result = validate_episode_spec_scenario_reflection(PROMPT, episode)

    assert result.passed is False
    assert any(issue.issueType == "missing_kickboard_semantic" for issue in result.issues)


def test_reflection_fails_when_blocking_ratio_is_missing() -> None:
    episode = _episode_dict()
    episode["actors"]["static_obstacles"][0]["properties"].pop("blocking_ratio")

    result = validate_episode_spec_scenario_reflection(PROMPT, episode)

    assert result.passed is False
    assert any(issue.issueType == "missing_blocking_ratio" for issue in result.issues)


def test_reflection_fails_when_pedestrian_path_is_not_linked() -> None:
    episode = _episode_dict()
    episode["actors"]["pedestrians"][0]["path_id"] = "missing_path"

    result = validate_episode_spec_scenario_reflection(PROMPT, episode)

    assert result.passed is False
    assert any(issue.issueType == "pedestrian_path_not_linked" for issue in result.issues)


def test_reflection_passes_for_kickboard_blocking_crossing_scenario() -> None:
    result = validate_episode_spec_scenario_reflection(PROMPT, _episode_dict())

    assert result.passed is True
    assert result.staticObstacleCount == 1
    assert result.hasKickboardSemantic is True
    assert result.hasBlockingRatio is True
    assert result.pedestrianCount == 1
    assert result.pathCount == 1
    assert result.pedestrianPathLinked is True
    assert result.hasCrossingPedestrian is True
    assert result.ueCompilerReadiness is True

