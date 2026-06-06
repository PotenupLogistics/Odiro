from __future__ import annotations

import pytest

from app.core.settings import Settings
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
        "obstacle_ahead_000_baseline",
        "obstacle_ahead_001_baseline",
        "obstacle_ahead_002_baseline",
        "obstacle_ahead_003_baseline",
        "obstacle_ahead_004_baseline",
    ]


def test_generate_setup_pair_queue_uses_ue_relative_paths_and_no_wrapper_fields() -> None:
    result = generate_setup_pair_queue(_world_config(), episode_count=2)
    payload = result.run_queue.model_dump(mode="json", by_alias=True)

    assert set(payload) == {"schema", "version", "runs"}
    assert payload["runs"][0]["episode_setup"] == "Json/Input/EpisodeSetup_obstacle_ahead_000.json"
    assert payload["runs"][0]["delivery_bot_setup"] == "Json/Input/DeliveryBotSetup_obstacle_ahead_000_baseline.json"
    assert payload["runs"][1]["episode_setup"] == "Json/Input/EpisodeSetup_obstacle_ahead_001.json"
    assert payload["runs"][1]["delivery_bot_setup"] == "Json/Input/DeliveryBotSetup_obstacle_ahead_001_baseline.json"
    assert "diagnostics" not in payload
    assert "setupPairs" not in payload


def test_generate_setup_pair_queue_uses_distinct_episode_setups() -> None:
    result = generate_setup_pair_queue(_world_config(), episode_count=3)

    assert len({item.episode_setup_path for item in result.items}) == 3
    assert len({tuple(item.episode_setup.actors.robot.xy_m) for item in result.items}) == 1
    summaries = [
        (
            len(item.episode_setup.actors.static_obstacles),
            tuple(tuple(obstacle.xy_m) for obstacle in item.episode_setup.actors.static_obstacles),
            tuple(obstacle.properties["blocking_ratio"] for obstacle in item.episode_setup.actors.static_obstacles),
            len(item.episode_setup.actors.pedestrians),
        )
        for item in result.items
    ]
    assert len(set(summaries)) == 3
    assert summaries[0] != summaries[2]
    episode = result.items[0].episode_setup
    region = episode.ground_model.regions[0]
    assert region.shape.center_xy_m == [4.0, 0.0]
    assert region.shape.size_m == [8.0, 1.2]
    assert episode.actors.robot.xy_m == [0.0, 0.0]
    assert episode.actors.static_obstacles[0].xy_m[0] == 4.0
    assert episode.actors.robot.route is not None
    assert episode.actors.robot.route.goal_xy_m == [8.0, 0.0]


def test_generate_setup_pair_queue_keeps_pedestrians_inside_walkable_sidewalk_region() -> None:
    result = generate_setup_pair_queue(_world_config(), episode_count=3)

    for item in result.items:
        episode = item.episode_setup
        region = episode.ground_model.regions[0]
        center_x, center_y = region.shape.center_xy_m
        size_x, size_y = region.shape.size_m
        min_x = center_x - size_x / 2.0
        max_x = center_x + size_x / 2.0
        min_y = center_y - size_y / 2.0
        max_y = center_y + size_y / 2.0

        assert episode.ground_model.default_region_type == "blocked"
        assert region.region_type == "walkable"
        for pedestrian in episode.actors.pedestrians:
            assert min_x <= pedestrian.xy_m[0] <= max_x
            assert min_y <= pedestrian.xy_m[1] <= max_y
        for path in episode.paths:
            for point in path.points_xy_m:
                assert min_x <= point[0] <= max_x
                assert min_y <= point[1] <= max_y


def test_generate_setup_pair_queue_defaults_to_repeated_baseline_delivery_bot_setups() -> None:
    result = generate_setup_pair_queue(_world_config(), episode_count=3)

    payloads = [item.delivery_bot_setup.model_dump(mode="json", by_alias=True, exclude_none=True) for item in result.items]
    assert payloads == [payloads[0], payloads[0], payloads[0]]
    assert [run.delivery_bot_setup for run in result.run_queue.runs] == [
        "Json/Input/DeliveryBotSetup_obstacle_ahead_000_baseline.json",
        "Json/Input/DeliveryBotSetup_obstacle_ahead_001_baseline.json",
        "Json/Input/DeliveryBotSetup_obstacle_ahead_002_baseline.json",
    ]


def test_generate_setup_pair_queue_applies_policy_comparison_delivery_bot_setups_when_requested() -> None:
    result = generate_setup_pair_queue(_world_config(), episode_count=5, comparison_mode="policy_comparison")

    assert result.items[0].delivery_bot_setup.robot.lidar.stop_distance_m == 1.2
    assert result.items[1].delivery_bot_setup.robot.lidar.stop_distance_m == 1.35
    assert result.items[2].delivery_bot_setup.robot.lidar.stop_distance_m == 1.25
    assert result.items[3].delivery_bot_setup.robot.lidar.stop_distance_m == 1.4
    assert result.items[3].delivery_bot_setup.robot.lidar.slow_down_distance_m == 4.5
    assert result.items[4].delivery_bot_setup.robot.path_follow.target_speed_kmh == 8.0
    assert result.items[4].delivery_bot_setup.robot.path_follow.obstacle_slow_speed_kmh == 1.5
    assert "deliveryBotSetup.robot.lidar.slow_down_distance_m" in result.items[3].variant.changed_parameters
    assert "deliveryBotSetup.robot.path_follow.target_speed_kmh" in result.items[4].variant.changed_parameters


def test_generate_setup_pair_queue_falls_back_to_baseline_after_defined_policy_profiles() -> None:
    max_count = Settings().scenarioEpisodeMaxCount
    if max_count < 6:
        pytest.skip("fallback profile test requires SCENARIO_EPISODE_MAX_COUNT >= 6")

    result = generate_setup_pair_queue(_world_config(), episode_count=max_count, comparison_mode="policy_comparison")

    assert result.run_queue.runs[5].pair_id == "obstacle_ahead_005_baseline"
    assert result.run_queue.runs[5].episode_setup == "Json/Input/EpisodeSetup_obstacle_ahead_005.json"
    assert result.run_queue.runs[5].delivery_bot_setup == "Json/Input/DeliveryBotSetup_obstacle_ahead_005_baseline.json"


def test_policy_comparison_delivery_bot_setups_share_identical_field_sets_and_valid_ranges() -> None:
    result = generate_setup_pair_queue(_world_config(), episode_count=5, comparison_mode="policy_comparison")
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
    result = generate_setup_pair_queue(_world_config(), episode_count=Settings().scenarioEpisodeMaxCount + 1)

    assert result.run_queue_validation.valid is False
    assert result.items == []
    assert {error.code for error in result.run_queue_validation.errors} == {"episode_count_too_large"}
