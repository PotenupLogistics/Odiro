from __future__ import annotations

from app.models.episode_setup import EpisodeSetup
from app.services.episode_setup_validator import validate_episode_setup


def _valid_episode_setup() -> dict:
    return {
        "schema": "episode_actor_spawn_mvp",
        "version": 1,
        "scenario_id": "obstacle_ahead",
        "map_id": "EpisodeSandbox",
        "run": {"base_seed": 1001, "iteration_index": 0, "time_limit_s": 60},
        "evaluation": {"goal_acceptance_radius_m": 1.0},
        "ground_model": {
            "default_region_type": "walkable",
            "regions": [
                {
                    "region_id": "sidewalk_main",
                    "region_type": "walkable",
                    "shape": {"type": "rectangle", "center_xy_m": [4.0, 0.0], "size_m": [8.0, 1.2], "yaw_deg": 0},
                    "traversability_score": 1.0,
                }
            ],
        },
        "paths": [],
        "actors": {
            "robot": {
                "instance_id": "robot_01",
                "asset_id": "delivery_bot",
                "spawn_only": False,
                "xy_m": [0.0, 0.0],
                "yaw_deg": 0.0,
                "route": {"goal_xy_m": [8.0, 0.0], "auto_start": True},
            },
            "static_obstacles": [
                {
                    "instance_id": "obstacle_01",
                    "prop_id": "obstacle.box_01",
                    "xy_m": [4.0, -0.4],
                    "yaw_deg": 0.0,
                    "properties": {"blocking_ratio": 0.6, "semantic_type": "Obstacle"},
                }
            ],
            "pedestrians": [],
        },
    }


def test_episode_setup_model_accepts_valid_contract_shape() -> None:
    episode = EpisodeSetup.model_validate(_valid_episode_setup())

    assert episode.schema == "episode_actor_spawn_mvp"
    assert episode.actors.robot.xy_m == [0.0, 0.0]
    assert episode.actors.robot.route is not None
    assert episode.actors.robot.route.goal_xy_m == [8.0, 0.0]
    assert episode.ground_model.regions[0].shape.center_xy_m == [4.0, 0.0]


def test_episode_setup_validator_rejects_legacy_transform_and_units_fields() -> None:
    payload = _valid_episode_setup()
    payload["units"] = {"distance": "m"}
    payload["actors"]["robot"]["transform"] = {"location_m": [0, 0, 0]}
    payload["actors"]["static_obstacles"][0]["location_m"] = [4, 0, 0]
    payload["actors"]["static_obstacles"][0]["rotation_deg"] = {"yaw": 0}
    payload["actors"]["static_obstacles"][0]["scale"] = [1, 1, 1]

    result = validate_episode_setup(payload)

    assert result.valid is False
    joined = " ".join(error.code for error in result.errors)
    assert "forbidden_root_field" in joined
    assert "forbidden_actor_field" in joined


def test_episode_setup_validator_requires_route_goal_when_robot_auto_runs() -> None:
    payload = _valid_episode_setup()
    payload["actors"]["robot"]["route"] = None

    result = validate_episode_setup(payload)

    assert result.valid is False
    assert any(error.code == "missing_robot_goal" for error in result.errors)


def test_episode_setup_validator_checks_ids_paths_and_prop_catalog() -> None:
    payload = _valid_episode_setup()
    payload["paths"] = [{"path_id": "ped_path", "points_xy_m": [[0, 0], [1, 1]], "role": "legacy"}]
    payload["actors"]["pedestrians"] = [
        {"instance_id": "obstacle_01", "path_id": "missing_path", "xy_m": [0, 0], "movement": {"speed_mps": 1.0}}
    ]
    payload["actors"]["static_obstacles"][0]["prop_id"] = "obstacle.unknown"

    result = validate_episode_setup(payload)

    assert result.valid is False
    codes = {error.code for error in result.errors}
    assert "duplicate_instance_id" in codes
    assert "forbidden_path_field" in codes
    assert "missing_pedestrian_path" in codes
    assert "unknown_prop_id" in codes
