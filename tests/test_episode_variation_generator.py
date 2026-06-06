from __future__ import annotations

from app.services.episode_variation_generator import generate_episode_variants


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
        "environmentSampling": {
            "fixedParameters": {
                "sidewalkWidthCm": 120,
                "obstacleBlockingRatio": 0.6,
                "timeLimitSec": 60,
            }
        },
    }


def test_generate_episode_variants_defaults_to_five_and_keeps_baseline_first() -> None:
    variants = generate_episode_variants(_world_config())

    assert len(variants) == 5
    assert variants[0].world_config["seed"] == 1001
    assert variants[0].world_config["run"]["iteration_index"] == 0
    assert variants[0].world_config["environmentSampling"]["deliveryBotPolicyProfile"] == "baseline"
    assert "deliveryBotSetup.robot.lidar.stop_distance_m" in variants[0].changed_parameters


def test_generate_episode_variants_uses_base_seed_and_unique_iteration_index() -> None:
    variants = generate_episode_variants(_world_config(), episode_count=5, base_seed=2000)

    assert [variant.world_config["seed"] for variant in variants] == [2000, 2001, 2002, 2003, 2004]
    assert [variant.world_config["run"]["iteration_index"] for variant in variants] == [0, 1, 2, 3, 4]


def test_generate_episode_variants_preserves_no_pedestrian_and_single_obstacle_constraints() -> None:
    variants = generate_episode_variants(_world_config(), episode_count=3)

    assert [len(variant.world_config["obstacles"]) for variant in variants] == [1, 2, 2]
    assert [len(variant.world_config["pedestrians"]) for variant in variants] == [0, 2, 3]
    assert all(variant.world_config["map"]["sidewalkWidthCm"] == 120 for variant in variants)
    assert all(variant.world_config["obstacles"][0]["blockingRatio"] == 0.6 for variant in variants)
    assert all(variant.world_config["runtime"]["maxDurationSec"] == 60 for variant in variants)


def test_generate_episode_variants_defaults_to_scenario_variation_with_baseline_policy() -> None:
    variants = generate_episode_variants(_world_config(), episode_count=3)

    assert [variant.world_config["environmentSampling"]["deliveryBotPolicyProfile"] for variant in variants] == [
        "baseline",
        "baseline",
        "baseline",
    ]
    summaries = [
        (
            len(variant.world_config["obstacles"]),
            tuple((obstacle["position"]["x"], obstacle["position"]["y"]) for obstacle in variant.world_config["obstacles"]),
            tuple(obstacle["blockingRatio"] for obstacle in variant.world_config["obstacles"]),
            len(variant.world_config["pedestrians"]),
        )
        for variant in variants
    ]
    assert len(set(summaries)) == 3
    assert summaries[0] != summaries[2]
    changed = {change for variant in variants for change in variant.changed_parameters}
    assert "obstacles[0].position.y" in changed
    assert "obstacles" in changed
    assert "pedestrians" in changed
    assert "deliveryBotSetup.robot.lidar.stop_distance_m" in variants[0].changed_parameters


def test_generate_episode_variants_can_enable_policy_comparison_explicitly() -> None:
    variants = generate_episode_variants(_world_config(), episode_count=5, comparison_mode="policy_comparison")

    assert [variant.world_config["environmentSampling"]["deliveryBotPolicyProfile"] for variant in variants] == [
        "baseline",
        "cautious_lidar",
        "slow_safe",
        "conservative_lidar",
        "slower_path_follow",
    ]
    assert "deliveryBotSetup.robot.lidar.stop_distance_m" in variants[1].changed_parameters
    assert "deliveryBotSetup.robot.path_follow.target_speed_kmh" in variants[2].changed_parameters
