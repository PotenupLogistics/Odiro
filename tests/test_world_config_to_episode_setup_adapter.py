from __future__ import annotations

from copy import deepcopy

from app.services.episode_setup_validator import validate_episode_setup
from app.services.world_config_to_episode_setup_adapter import convert_world_config_to_episode_setup
from app.utils.json_sanitizer import remove_json_nulls


def _world_config() -> dict:
    return {
        "schemaVersion": "1.0",
        "worldId": "world-1",
        "scenarioId": "obstacle_ahead",
        "seed": 1001,
        "map": {"type": "Sidewalk", "lengthCm": 800, "sidewalkWidthCm": 120},
        "robot": {
            "botId": "robot_01",
            "spawn": {"x": 0, "y": 0, "z": 0},
            "goal": {"x": 800, "y": 0, "z": 0},
            "policyId": "policy_v1_basic_safety",
        },
        "obstacles": [
            {"objectId": "obstacle_01", "type": "Obstacle", "position": {"x": 400, "y": -40, "z": 0}, "blockingRatio": 0.6}
        ],
        "pedestrians": [],
        "runtime": {"maxDurationSec": 60},
    }


def test_converts_world_config_to_episode_setup_without_mutating_input() -> None:
    world_config = _world_config()
    before = deepcopy(world_config)

    episode = convert_world_config_to_episode_setup(world_config)

    assert world_config == before
    assert episode.scenario_id == "obstacle_ahead"
    assert episode.run.base_seed == 1001
    assert episode.run.time_limit_s == 60
    assert episode.ground_model.default_region_type == "blocked"
    assert episode.ground_model.regions[0].shape.size_m == [12.0, 1.5]
    assert episode.ground_model.regions[0].region_type == "walkable"
    assert episode.ground_model.regions[0].shape.center_xy_m == [4.0, 0.0]
    assert episode.actors.robot.xy_m == [0.0, 0.0]
    assert episode.actors.robot.route is not None
    assert episode.actors.robot.route.goal_xy_m == [8.0, 0.0]
    assert episode.actors.static_obstacles[0].xy_m == [4.0, -0.4]
    assert episode.actors.static_obstacles[0].prop_id == "obstacle.box_01"
    assert episode.actors.static_obstacles[0].properties["blocking_ratio"] == 0.6
    assert episode.paths == []
    assert episode.actors.pedestrians == []
    assert "policyId" not in episode.model_dump(mode="json", by_alias=True)
    assert validate_episode_setup(episode).valid is True


def test_walkable_region_wraps_robot_start_and_goal_with_margin() -> None:
    episode = convert_world_config_to_episode_setup(_world_config())
    region = episode.ground_model.regions[0]
    center_x, _ = region.shape.center_xy_m
    size_x, _ = region.shape.size_m
    x_min = center_x - size_x / 2.0
    x_max = center_x + size_x / 2.0

    assert x_min == -2.0
    assert x_max == 10.0
    assert episode.actors.robot.xy_m[0] - x_min >= 2.0
    assert x_max - episode.actors.robot.route.goal_xy_m[0] >= 2.0


def test_explicit_one_meter_sidewalk_width_is_preserved() -> None:
    world_config = _world_config()
    world_config["map"]["sidewalkWidthCm"] = 100
    world_config["semanticFixedConstraints"] = {"sidewalkWidthCm": 100}

    episode = convert_world_config_to_episode_setup(world_config)

    assert episode.ground_model.regions[0].shape.size_m[1] == 1.0


def test_episode_setup_export_payload_is_null_free_and_uses_evaluation_defaults() -> None:
    episode = convert_world_config_to_episode_setup(_world_config())

    payload = remove_json_nulls(
        episode.model_dump(mode="json", by_alias=True),
        drop_empty_object_keys={"properties"},
    )
    payload_text = str(payload)

    assert "None" not in payload_text
    assert "penalty" not in payload["ground_model"]["regions"][0]
    assert "collision_tag" not in payload["ground_model"]["regions"][0]
    assert "properties" not in payload["actors"]["robot"]
    assert payload["evaluation"]["near_miss"] == {"distance_m": 0.5}
    assert payload["evaluation"]["scoring"]["pedestrian_collision"] == -10


def test_converts_world_config_pedestrians_to_paths_and_pedestrian_actors() -> None:
    world_config = _world_config()
    world_config["pedestrians"] = [
        {"objectId": "ped_01", "spawn": {"x": 100, "y": -50}, "goal": {"x": 100, "y": 50}, "speedKmh": 3.6}
    ]

    episode = convert_world_config_to_episode_setup(world_config)

    assert episode.paths[0].path_id == "ped_01_path"
    assert episode.paths[0].points_xy_m == [[1.0, -0.5], [1.0, 0.5]]
    assert episode.actors.pedestrians[0].path_id == "ped_01_path"
    assert episode.actors.pedestrians[0].movement.speed_mps == 1.0
