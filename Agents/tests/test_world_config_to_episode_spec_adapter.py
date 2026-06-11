from __future__ import annotations

from copy import deepcopy

from app.services.world_config_to_episode_spec_adapter import (
    convert_world_config_to_episode_spec_with_warnings,
)


def _world_config() -> dict:
    return {
        "schemaVersion": "1.0",
        "worldId": "world-1",
        "scenarioId": "scenario-1",
        "seed": 1001,
        "map": {
            "type": "Sidewalk",
            "lengthCm": 1000,
            "sidewalkWidthCm": 120,
            "surfaceCondition": "dry",
            "slopeDegree": 0,
        },
        "robot": {
            "botId": "bot-1",
            "spawn": {"x": 0, "y": 0, "z": 0},
            "goal": {"x": 1000, "y": 0, "z": 0},
            "policyId": "policy_v1_basic_safety",
        },
        "obstacles": [
            {
                "objectId": "kickboard_001",
                "type": "Kickboard",
                "position": {"x": 500, "y": 0, "z": 0},
                "blockingRatio": 0.7,
            }
        ],
        "pedestrians": [
            {
                "objectId": "pedestrian_001",
                "spawn": {"x": 500, "y": -200, "z": 0},
                "goal": {"x": 500, "y": 200, "z": 0},
                "speedKmh": 3.6,
                "behavior": "Crossing",
            }
        ],
        "environmentObjects": [],
        "runtime": {"maxDurationSec": 300, "captureReplay": False, "emitEventLog": True},
    }


def test_converts_world_config_to_episode_spec_without_mutating_input() -> None:
    world_config = _world_config()
    before = deepcopy(world_config)

    episode, warnings = convert_world_config_to_episode_spec_with_warnings(world_config)

    assert world_config == before
    assert episode.scenario_id == "scenario-1"
    assert episode.run.base_seed == 1001
    assert episode.run.time_limit_s == 300
    assert episode.ground_model.regions[0].shape.size_m == [10.0, 1.2]
    assert episode.ground_model.regions[0].model_dump(mode="json", exclude_none=True).get("penalties") is None
    assert episode.ground_model.regions[0].model_dump(mode="json", exclude_none=True).get("penalty") is None
    assert episode.actors.robot.transform.location_m == [0.0, 0.0, 0.0]
    assert episode.actors.robot.route.goal_m == [10.0, 0.0, 0.0]
    assert episode.paths[0].path_id == "pedestrian_001_path"
    assert episode.paths[0].points_m == [[5.0, -2.0, 0.0], [5.0, 2.0, 0.0]]
    assert episode.actors.pedestrians[0].movement.speed_mps == 1.0
    assert episode.actors.static_obstacles[0].prop_id == "obstacle.road_barrier_01"
    assert episode.actors.static_obstacles[0].properties["semantic_type"] == "Kickboard"
    assert warnings
    assert "Kickboard mapped" in warnings[0].message


def test_converts_generic_obstacle_with_blocking_ratio_to_static_obstacle() -> None:
    world_config = _world_config()
    world_config["obstacles"] = [
        {
            "objectId": "obstacle_001",
            "type": "Obstacle",
            "position": {"x": 400, "y": 0, "z": 0},
            "blockingRatio": 0.6,
        }
    ]
    world_config["pedestrians"] = []

    episode, warnings = convert_world_config_to_episode_spec_with_warnings(world_config)

    obstacle = episode.actors.static_obstacles[0]
    assert obstacle.prop_id == "obstacle.box_01"
    assert obstacle.transform.location_m == [4.0, 0.0, 0.0]
    assert isinstance(obstacle.properties["semantic_type"], str)
    assert obstacle.properties["semantic_type"] == "Obstacle"
    assert isinstance(obstacle.properties["blocking_ratio"], float)
    assert obstacle.properties["blocking_ratio"] == 0.6
    assert episode.actors.pedestrians == []
    assert episode.paths == []
    assert warnings
