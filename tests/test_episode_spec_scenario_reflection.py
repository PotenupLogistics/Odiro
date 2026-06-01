from __future__ import annotations

from app.services.episode_spec_scenario_reflection import validate_episode_spec_scenario_reflection
from app.services.world_config_to_episode_spec_adapter import convert_world_config_to_episode_spec

from tests.test_world_config_to_episode_spec_adapter import _world_config


PROMPT = "좁은 보도에서 공유 킥보드가 로봇 경로를 막고, 오른쪽에서 보행자가 횡단하는 상황"
GENERIC_OBSTACLE_PROMPT = (
    "보도 폭은 120cm인 좁은 보도 상황을 만들어줘. "
    "로봇은 x=0, y=0, z=0에서 출발해서 x=800, y=0, z=0으로 이동한다. "
    "로봇 경로 중앙인 x=400, y=0, z=0 근처에 정적 장애물 1개를 배치하고, "
    "장애물이 경로를 막는 정도는 blockingRatio 0.6으로 설정해줘. "
    "보행자는 없는 시나리오로 만들어줘."
)
ROUTE_MIDPOINT_PROMPT = (
    "로봇은 x=0, y=0, z=0에서 출발해서 x=800, y=0, z=0으로 이동한다. "
    "경로 중앙 근처에 정적 장애물 1개가 경로를 막고 있는 상황을 만들어줘."
)


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


def test_reflection_passes_for_generic_obstacle_without_pedestrians_when_requested() -> None:
    world_config = _world_config()
    world_config["map"]["sidewalkWidthCm"] = 120
    world_config["robot"]["goal"] = {"x": 800, "y": 0, "z": 0}
    world_config["obstacles"] = [
        {
            "objectId": "obstacle_001",
            "type": "Obstacle",
            "position": {"x": 400, "y": 0, "z": 0},
            "blockingRatio": 0.6,
        }
    ]
    world_config["pedestrians"] = []
    episode = convert_world_config_to_episode_spec(world_config).model_dump(mode="json", by_alias=True)

    result = validate_episode_spec_scenario_reflection(GENERIC_OBSTACLE_PROMPT, episode)

    assert result.passed is True
    assert result.staticObstacleCount == 1
    assert result.hasBlockingRatio is True
    assert result.pedestrianCount == 0
    assert result.pathCount == 0
    assert result.pedestrianPathLinked is True
    assert result.ueCompilerReadiness is True


def test_reflection_fails_when_obstacle_required_but_static_obstacles_empty() -> None:
    world_config = _world_config()
    world_config["obstacles"] = []
    world_config["pedestrians"] = []
    episode = convert_world_config_to_episode_spec(world_config).model_dump(mode="json", by_alias=True)

    result = validate_episode_spec_scenario_reflection(GENERIC_OBSTACLE_PROMPT, episode)

    assert result.passed is False
    assert result.staticObstacleCount == 0
    assert result.ueCompilerReadiness is False
    assert any(issue.issueType == "missing_static_obstacle" for issue in result.issues)


def test_reflection_uses_environment_sampling_to_require_width_and_blocking_ratio() -> None:
    world_config = _world_config()
    world_config["map"]["sidewalkWidthCm"] = 150
    world_config["obstacles"] = []
    world_config["pedestrians"] = []
    episode = convert_world_config_to_episode_spec(world_config).model_dump(mode="json", by_alias=True)

    result = validate_episode_spec_scenario_reflection(
        "보도 폭과 장애물 차단 정도는 environmentSampling 결과를 우선 적용해줘.",
        episode,
        environment_sampling={
            "parameters": {
                "sidewalkWidthCm": 120,
                "obstacleBlockingRatio": 0.6,
                "timeLimitSec": 60,
            }
        },
    )

    assert result.passed is False
    assert result.hasBlockingRatio is False
    assert result.ueCompilerReadiness is False
    issue_types = {issue.issueType for issue in result.issues}
    assert "missing_static_obstacle" in issue_types
    assert "missing_blocking_ratio" in issue_types
    assert "environment_sidewalk_width_mismatch" in issue_types


def test_episode_reflection_fails_when_route_midpoint_obstacle_is_near_goal() -> None:
    world_config = _world_config()
    world_config["robot"]["spawn"] = {"x": 0, "y": 0, "z": 0}
    world_config["robot"]["goal"] = {"x": 800, "y": 0, "z": 0}
    world_config["obstacles"] = [
        {
            "objectId": "obstacle_001",
            "type": "Obstacle",
            "position": {"x": 800, "y": 0, "z": 0},
            "blockingRatio": 0.6,
        }
    ]
    world_config["pedestrians"] = []
    episode = convert_world_config_to_episode_spec(world_config).model_dump(mode="json", by_alias=True)

    result = validate_episode_spec_scenario_reflection(ROUTE_MIDPOINT_PROMPT, episode)

    assert result.passed is False
    assert result.ueCompilerReadiness is False
    assert any(issue.issueType == "obstacle_not_near_route_midpoint" for issue in result.issues)


def test_episode_reflection_passes_when_route_midpoint_obstacle_is_in_meters() -> None:
    world_config = _world_config()
    world_config["robot"]["spawn"] = {"x": 0, "y": 0, "z": 0}
    world_config["robot"]["goal"] = {"x": 800, "y": 0, "z": 0}
    world_config["obstacles"] = [
        {
            "objectId": "obstacle_001",
            "type": "Obstacle",
            "position": {"x": 400, "y": 0, "z": 0},
            "blockingRatio": 0.6,
        }
    ]
    world_config["pedestrians"] = []
    episode = convert_world_config_to_episode_spec(world_config).model_dump(mode="json", by_alias=True)

    result = validate_episode_spec_scenario_reflection(ROUTE_MIDPOINT_PROMPT, episode)

    assert result.passed is True
    assert result.ueCompilerReadiness is True
