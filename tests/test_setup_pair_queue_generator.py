from __future__ import annotations

from app.services.setup_pair_queue_generator import generate_setup_pair_queue


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
        },
        "obstacles": [
            {"objectId": "obstacle_01", "type": "Obstacle", "position": {"x": 400, "y": 0, "z": 0}, "blockingRatio": 0.6}
        ],
        "pedestrians": [],
        "runtime": {"maxDurationSec": 60},
    }


def test_generate_setup_pair_queue_creates_default_five_valid_pairs() -> None:
    result = generate_setup_pair_queue(_world_config())

    assert result.run_count == 5
    assert result.run_queue_validation.valid is True
    assert all(item.episode_setup_validation.valid for item in result.items)
    assert all(item.delivery_bot_setup_validation.valid for item in result.items)
    assert [run.pair_id for run in result.run_queue.runs] == [
        "obstacle_ahead_000",
        "obstacle_ahead_001",
        "obstacle_ahead_002",
        "obstacle_ahead_003",
        "obstacle_ahead_004",
    ]


def test_generate_setup_pair_queue_uses_ue_relative_paths_and_no_wrapper_fields() -> None:
    result = generate_setup_pair_queue(_world_config(), episode_count=2)
    payload = result.run_queue.model_dump(mode="json", by_alias=True)

    assert set(payload) == {"schema", "version", "runs"}
    assert payload["runs"][0]["episode_setup"] == "Json/Input/EpisodeSetup_obstacle_ahead_000.json"
    assert payload["runs"][0]["delivery_bot_setup"] == "Json/Input/DeliveryBotSetup_obstacle_ahead_000.json"
    assert "diagnostics" not in payload
    assert "setupPairs" not in payload


def test_generate_setup_pair_queue_preserves_no_pedestrians_across_episodes() -> None:
    result = generate_setup_pair_queue(_world_config(), episode_count=5)

    assert all(item.episode_setup.paths == [] for item in result.items)
    assert all(item.episode_setup.actors.pedestrians == [] for item in result.items)
    assert all(len(item.episode_setup.actors.static_obstacles) == 1 for item in result.items)
    assert [item.episode_setup.run.iteration_index for item in result.items] == [0, 1, 2, 3, 4]


def test_generate_setup_pair_queue_applies_delivery_bot_variation_policy() -> None:
    result = generate_setup_pair_queue(_world_config(), episode_count=5)

    assert result.items[3].delivery_bot_setup.robot.lidar.stop_distance_m == 1.4
    assert result.items[3].delivery_bot_setup.robot.lidar.front_half_angle_degree == 25.0
    assert result.items[4].delivery_bot_setup.robot.path_follow.target_speed_kmh == 8.0
    assert "deliveryBotSetup.robot.lidar.front_half_angle_degree" in result.items[3].variant.changed_parameters
    assert "deliveryBotSetup.robot.path_follow.target_speed_kmh" in result.items[4].variant.changed_parameters


def test_generate_setup_pair_queue_rejects_count_above_maximum() -> None:
    result = generate_setup_pair_queue(_world_config(), episode_count=21)

    assert result.run_queue_validation.valid is False
    assert result.items == []
    assert {error.code for error in result.run_queue_validation.errors} == {"episode_count_too_large"}
