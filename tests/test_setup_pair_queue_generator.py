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
        "narrow_sidewalk_policy_000_baseline",
        "narrow_sidewalk_policy_001_short_stop",
        "narrow_sidewalk_policy_002_long_stop",
        "narrow_sidewalk_policy_003_early_slowdown",
        "narrow_sidewalk_policy_004_low_speed",
    ]


def test_generate_setup_pair_queue_uses_ue_relative_paths_and_no_wrapper_fields() -> None:
    result = generate_setup_pair_queue(_world_config(), episode_count=2)
    payload = result.run_queue.model_dump(mode="json", by_alias=True)

    assert set(payload) == {"schema", "version", "runs"}
    assert payload["runs"][0]["episode_setup"] == "Json/Input/EpisodeSetup_narrow_sidewalk_fixed_center_block.json"
    assert payload["runs"][0]["delivery_bot_setup"] == "Json/Input/DeliveryBotSetup_policy_000_baseline.json"
    assert payload["runs"][1]["episode_setup"] == payload["runs"][0]["episode_setup"]
    assert payload["runs"][1]["delivery_bot_setup"] == "Json/Input/DeliveryBotSetup_policy_001_short_stop.json"
    assert "diagnostics" not in payload
    assert "setupPairs" not in payload


def test_generate_setup_pair_queue_uses_one_fixed_episode_setup_for_policy_comparison() -> None:
    result = generate_setup_pair_queue(_world_config(), episode_count=5)

    assert all(item.episode_setup.paths == [] for item in result.items)
    assert all(item.episode_setup.actors.pedestrians == [] for item in result.items)
    assert all(len(item.episode_setup.actors.static_obstacles) == 1 for item in result.items)
    assert len({item.episode_setup_path for item in result.items}) == 1
    assert len({tuple(item.episode_setup.actors.robot.xy_m) for item in result.items}) == 1
    assert len({tuple(item.episode_setup.actors.static_obstacles[0].xy_m) for item in result.items}) == 1
    episode = result.items[0].episode_setup
    region = episode.ground_model.regions[0]
    assert region.shape.center_xy_m == [5.0, 0.0]
    assert region.shape.size_m == [14.0, 1.5]
    assert episode.actors.robot.xy_m == [0.0, 0.0]
    assert episode.actors.static_obstacles[0].xy_m == [5.5, 0.0]
    assert episode.actors.robot.route is not None
    assert episode.actors.robot.route.goal_xy_m == [10.5, 0.0]


def test_generate_setup_pair_queue_applies_policy_comparison_delivery_bot_setups() -> None:
    result = generate_setup_pair_queue(_world_config(), episode_count=5)

    assert result.items[0].delivery_bot_setup.robot.lidar.stop_distance_m == 1.2
    assert result.items[1].delivery_bot_setup.robot.lidar.stop_distance_m == 0.8
    assert result.items[2].delivery_bot_setup.robot.lidar.stop_distance_m == 1.6
    assert result.items[3].delivery_bot_setup.robot.lidar.slow_down_distance_m == 4.5
    assert result.items[4].delivery_bot_setup.robot.path_follow.target_speed_kmh == 8.0
    assert result.items[4].delivery_bot_setup.robot.path_follow.obstacle_slow_speed_kmh == 1.5
    assert "deliveryBotSetup.robot.lidar.slow_down_distance_m" in result.items[3].variant.changed_parameters
    assert "deliveryBotSetup.robot.path_follow.target_speed_kmh" in result.items[4].variant.changed_parameters


def test_policy_comparison_delivery_bot_setups_share_identical_field_sets_and_valid_ranges() -> None:
    result = generate_setup_pair_queue(_world_config(), episode_count=5)
    payloads = [item.delivery_bot_setup.model_dump(mode="json", by_alias=True, exclude_none=True) for item in result.items]
    first_field_set = payloads[0]["robot"]

    for payload in payloads:
        robot = payload["robot"]
        assert robot.keys() == first_field_set.keys()
        assert robot["drive"].keys() == first_field_set["drive"].keys()
        assert robot["path_follow"].keys() == first_field_set["path_follow"].keys()
        assert robot["lidar"].keys() == first_field_set["lidar"].keys()
        assert robot["path_follow"]["target_speed_kmh"] <= robot["drive"]["max_speed_kmh"]
        assert robot["path_follow"]["obstacle_slow_speed_kmh"] <= robot["path_follow"]["target_speed_kmh"]
        assert robot["lidar"]["stop_distance_m"] < robot["lidar"]["slow_down_distance_m"]


def test_generate_setup_pair_queue_rejects_count_above_maximum() -> None:
    result = generate_setup_pair_queue(_world_config(), episode_count=21)

    assert result.run_queue_validation.valid is False
    assert result.items == []
    assert {error.code for error in result.run_queue_validation.errors} == {"episode_count_too_large"}
