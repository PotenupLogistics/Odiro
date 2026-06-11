from __future__ import annotations

import pytest

from app.core.settings import Settings
from app.catalogs.static_obstacle_catalog import find_static_obstacle_by_prop_id
from app.services.episode_setup_validator import ALLOWED_STATIC_PROP_IDS
from app.services.scenario_consistency_checker import check_setup_pair_queue_consistency
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


def _explicit_fixed_world_config() -> dict:
    config = _world_config()
    config["map"]["lengthCm"] = 800
    config["environmentSampling"] = {
        "fixedParameters": {
            "sidewalkWidthCm": 120,
            "goalDistanceM": 10.0,
            "obstacleCount": 2,
            "obstacleType": "box",
            "obstaclePositionsFromStartM": [3.0, 6.0],
            "obstacleLateralPosition": "center",
            "pedestrianCount": 3,
            "pedestrianDirection": "opposite_direction",
        }
    }
    return config


def _simple_blocking_world_config() -> dict:
    config = _world_config()
    config["environmentSampling"] = {
        "fixedParameters": {
            "sidewalkWidthCm": 120,
            "obstacleType": "static_obstacle",
            "pedestrianCount": 0,
            "expectedRobotBehavior": ["SlowDown", "ReplanPath"],
        }
    }
    config["pedestrians"] = [
        {
            "objectId": "pedestrian_01",
            "spawn": {"x": 400, "y": 40, "z": 0},
            "goal": {"x": 600, "y": 40, "z": 0},
            "behavior": "same_direction",
            "speedKmh": 3.6,
        }
    ]
    return config


def _crossing_140_world_config() -> dict:
    config = _world_config()
    config["semanticFixedConstraints"] = {
        "sidewalkWidthCm": 140.0,
        "goalDistanceM": 10.0,
        "obstacleCount": 1,
        "obstacleType": "box",
        "obstaclePositionsFromStartM": [5.0],
        "obstacleLateralPosition": "center",
        "pedestrianCount": 1,
        "pedestrianDirection": "crossing",
    }
    return config


def _mixed_obstacle_180_world_config() -> dict:
    config = _world_config()
    config["semanticFixedConstraints"] = {
        "sidewalkWidthCm": 180.0,
        "goalDistanceM": 12.0,
        "obstacleCount": 2,
        "obstacleTypes": ["box", "kickboard"],
        "obstaclePositionsFromStartM": [4.0, 8.0],
        "pedestrianCount": 2,
        "pedestrianDirection": "opposite_direction",
    }
    return config


def _unsupported_obstacle_world_config() -> dict:
    config = _world_config()
    config["semanticFixedConstraints"] = {
        "sidewalkWidthCm": 140.0,
        "obstacleCount": 1,
        "obstacleTypes": ["hover_cart"],
        "obstaclePositionsFromStartM": [4.0],
        "pedestrianCount": 0,
    }
    return config


def _assert_episode_points_inside_sidewalk(item) -> None:
    episode = item.episode_setup
    region = episode.ground_model.regions[0]
    center_x, center_y = region.shape.center_xy_m
    size_x, size_y = region.shape.size_m
    min_x = center_x - size_x / 2.0
    max_x = center_x + size_x / 2.0
    min_y = center_y - size_y / 2.0
    max_y = center_y + size_y / 2.0
    points = [episode.actors.robot.xy_m]
    if episode.actors.robot.route is not None:
        points.append(episode.actors.robot.route.goal_xy_m)
    points.extend(obstacle.xy_m for obstacle in episode.actors.static_obstacles)
    points.extend(pedestrian.xy_m for pedestrian in episode.actors.pedestrians)
    for path in episode.paths:
        points.extend(path.points_xy_m)

    for point in points:
        assert min_x <= point[0] <= max_x
        assert min_y <= point[1] <= max_y


def _segment_intersects_rect(start: list[float], end: list[float], rect: tuple[float, float, float, float]) -> bool:
    min_x, max_x, min_y, max_y = rect
    if min_x <= start[0] <= max_x and min_y <= start[1] <= max_y:
        return True
    if min_x <= end[0] <= max_x and min_y <= end[1] <= max_y:
        return True
    x1, y1 = start
    x2, y2 = end
    dx = x2 - x1
    dy = y2 - y1
    t_min = 0.0
    t_max = 1.0
    for edge, distance in ((-dx, x1 - min_x), (dx, max_x - x1), (-dy, y1 - min_y), (dy, max_y - y1)):
        if abs(edge) < 1e-12:
            if distance < 0:
                return False
            continue
        ratio = distance / edge
        if edge < 0:
            t_min = max(t_min, ratio)
        else:
            t_max = min(t_max, ratio)
        if t_min > t_max:
            return False
    return True


def _obstacle_rect_with_buffer(obstacle, buffer_m: float = 0.3) -> tuple[float, float, float, float]:
    catalog_item = find_static_obstacle_by_prop_id(obstacle.prop_id)
    bbox_m = catalog_item["bbox_m"] if catalog_item else [0.0, 0.0, 0.0]
    half_x = bbox_m[0] / 2.0 + buffer_m
    half_y = bbox_m[1] / 2.0 + buffer_m
    return (
        obstacle.xy_m[0] - half_x,
        obstacle.xy_m[0] + half_x,
        obstacle.xy_m[1] - half_y,
        obstacle.xy_m[1] + half_y,
    )


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
    assert region.shape.size_m == [12.0, 1.5]
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


@pytest.mark.parametrize("episode_count", [1, 2, 3])
def test_generate_setup_pair_queue_preserves_explicit_fixed_constraints_in_episode_setups(
    episode_count: int,
) -> None:
    result = generate_setup_pair_queue(_explicit_fixed_world_config(), episode_count=episode_count)
    fixed = _explicit_fixed_world_config()["environmentSampling"]["fixedParameters"]
    consistency = check_setup_pair_queue_consistency(result, fixed_constraints=fixed, expected_episode_count=episode_count)

    assert result.run_count == episode_count
    assert result.run_queue_validation.valid is True
    assert consistency.passed is True
    assert consistency.issues == []
    assert len(result.run_queue.runs) == episode_count
    for item in result.items:
        episode = item.episode_setup
        region = episode.ground_model.regions[0]
        robot = episode.actors.robot
        assert region.shape.size_m == [14.0, 1.5]
        assert robot.route is not None
        assert robot.route.goal_xy_m[0] - robot.xy_m[0] == 10.0
        assert [obstacle.prop_id for obstacle in episode.actors.static_obstacles] == [
            "obstacle.box_01",
            "obstacle.box_01",
        ]
        assert [obstacle.xy_m[0] for obstacle in episode.actors.static_obstacles] == [3.0, 6.0]
        assert [obstacle.xy_m[1] for obstacle in episode.actors.static_obstacles] == [0.0, 0.0]
        assert len(episode.actors.pedestrians) == 3
        assert {pedestrian.properties["semantic_behavior"] for pedestrian in episode.actors.pedestrians} == {
            "opposite_direction"
        }


def test_generate_setup_pair_queue_uses_defaults_for_simple_blocking_prompt_constraints() -> None:
    result = generate_setup_pair_queue(_simple_blocking_world_config(), episode_count=1)
    fixed = _simple_blocking_world_config()["environmentSampling"]["fixedParameters"]
    consistency = check_setup_pair_queue_consistency(result, fixed_constraints=fixed, expected_episode_count=1)

    assert result.run_count == 1
    assert len(result.run_queue.runs) == 1
    assert consistency.passed is True
    item = result.items[0]
    episode = item.episode_setup
    region = episode.ground_model.regions[0]
    robot = episode.actors.robot
    assert episode.ground_model.default_region_type == "blocked"
    assert region.region_type == "walkable"
    assert episode.actors.pedestrians == []
    assert len(episode.actors.static_obstacles) >= 1
    assert all(obstacle.prop_id == "obstacle.box_01" for obstacle in episode.actors.static_obstacles)
    assert robot.route is not None
    for obstacle in episode.actors.static_obstacles:
        assert abs(obstacle.xy_m[1] - robot.xy_m[1]) <= region.shape.size_m[1] / 2.0
        assert robot.xy_m[0] <= obstacle.xy_m[0] <= robot.route.goal_xy_m[0]
    assert item.pair_id.endswith("_baseline")
    _assert_episode_points_inside_sidewalk(item)


def test_generate_setup_pair_queue_applies_140cm_crossing_semantic_constraints() -> None:
    result = generate_setup_pair_queue(_crossing_140_world_config(), episode_count=3)
    fixed = _crossing_140_world_config()["semanticFixedConstraints"]
    consistency = check_setup_pair_queue_consistency(result, fixed_constraints=fixed, expected_episode_count=3)

    assert consistency.passed is True
    for item in result.items:
        episode = item.episode_setup
        region = episode.ground_model.regions[0]
        robot = episode.actors.robot
        assert region.shape.size_m == [14.0, 1.5]
        assert robot.route is not None
        assert robot.route.goal_xy_m[0] - robot.xy_m[0] == 10.0
        assert [obstacle.prop_id for obstacle in episode.actors.static_obstacles] == ["obstacle.box_01"]
        assert [obstacle.xy_m[0] for obstacle in episode.actors.static_obstacles] == [5.0]
        assert len(episode.actors.pedestrians) == 1
        assert episode.actors.pedestrians[0].properties["semantic_behavior"] == "crossing"
        _assert_episode_points_inside_sidewalk(item)


def test_generate_setup_pair_queue_applies_180cm_mixed_obstacle_semantic_constraints() -> None:
    result = generate_setup_pair_queue(_mixed_obstacle_180_world_config(), episode_count=3)
    fixed = _mixed_obstacle_180_world_config()["semanticFixedConstraints"]
    consistency = check_setup_pair_queue_consistency(result, fixed_constraints=fixed, expected_episode_count=3)

    assert consistency.passed is True
    for item in result.items:
        episode = item.episode_setup
        region = episode.ground_model.regions[0]
        robot = episode.actors.robot
        assert region.shape.size_m == [16.0, 1.8]
        assert robot.route is not None
        assert robot.route.goal_xy_m[0] - robot.xy_m[0] == 12.0
        assert [obstacle.properties["semantic_type"] for obstacle in episode.actors.static_obstacles] == [
            "Obstacle",
            "Obstacle",
        ]
        assert [obstacle.xy_m[0] for obstacle in episode.actors.static_obstacles] == [4.0, 8.0]
        assert len(episode.actors.pedestrians) == 2
        assert {pedestrian.properties["semantic_behavior"] for pedestrian in episode.actors.pedestrians} == {
            "opposite_direction"
        }
        _assert_episode_points_inside_sidewalk(item)


def test_semantic_constraints_do_not_leak_into_ue_episode_setup_payload() -> None:
    result = generate_setup_pair_queue(_mixed_obstacle_180_world_config(), episode_count=1)
    payload = result.items[0].episode_setup.model_dump(mode="json", by_alias=True)

    assert "semanticFixedConstraints" not in payload
    assert "environmentSampling" not in payload
    assert "constraints" not in payload
    assert result.items[0].episode_setup_validation.valid is True
    assert result.items[0].delivery_bot_setup_validation.valid is True
    assert result.run_queue_validation.valid is True


def test_semantic_obstacle_types_use_only_allowed_ue_prop_ids() -> None:
    result = generate_setup_pair_queue(_mixed_obstacle_180_world_config(), episode_count=1)
    obstacles = result.items[0].episode_setup.actors.static_obstacles

    assert [obstacle.prop_id for obstacle in obstacles] == [
        "obstacle.box_01",
        "obstacle.road_barrier_01",
    ]
    assert all(obstacle.prop_id in ALLOWED_STATIC_PROP_IDS for obstacle in obstacles)
    assert [obstacle.properties["semantic_type"] for obstacle in obstacles] == ["Obstacle", "Obstacle"]
    assert result.items[0].episode_setup_validation.valid is True


def test_unsupported_obstacle_type_falls_back_to_allowed_prop_id_without_new_asset() -> None:
    result = generate_setup_pair_queue(_unsupported_obstacle_world_config(), episode_count=1)
    obstacle = result.items[0].episode_setup.actors.static_obstacles[0]

    assert obstacle.prop_id in ALLOWED_STATIC_PROP_IDS
    assert obstacle.prop_id == "obstacle.box_01"
    assert result.items[0].episode_setup_validation.valid is True


def test_crossing_pedestrian_paths_stay_inside_region_margin_and_avoid_obstacle_bbox() -> None:
    config = _world_config()
    config["semanticFixedConstraints"] = {
        "sidewalkWidthCm": 150.0,
        "goalDistanceM": 10.0,
        "obstacleCount": 1,
        "obstacleType": "road_barrier",
        "obstaclePositionsFromStartM": [5.0],
        "obstacleLateralPosition": "center",
        "pedestrianCount": 2,
        "pedestrianDirection": "crossing",
    }

    result = generate_setup_pair_queue(config, episode_count=2)
    consistency = check_setup_pair_queue_consistency(
        result,
        fixed_constraints=config["semanticFixedConstraints"],
        expected_episode_count=2,
    )

    assert consistency.passed is True
    for item in result.items:
        episode = item.episode_setup
        region = episode.ground_model.regions[0]
        center_x, center_y = region.shape.center_xy_m
        size_x, size_y = region.shape.size_m
        min_y = center_y - size_y / 2.0
        max_y = center_y + size_y / 2.0
        path_by_id = {path.path_id: path for path in episode.paths}
        obstacle_rects = [_obstacle_rect_with_buffer(obstacle) for obstacle in episode.actors.static_obstacles]

        assert len(episode.actors.pedestrians) == 2
        assert len({tuple(pedestrian.xy_m) for pedestrian in episode.actors.pedestrians}) == 2
        for pedestrian in episode.actors.pedestrians:
            path = path_by_id[pedestrian.path_id]
            assert path.points_xy_m[0][1] > min_y + 0.19
            assert path.points_xy_m[-1][1] < max_y - 0.19
            for rect in obstacle_rects:
                assert not _segment_intersects_rect(path.points_xy_m[0], path.points_xy_m[-1], rect)
