from __future__ import annotations

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
            {
                "objectId": "obstacle_01",
                "type": "Obstacle",
                "position": {"x": 400, "y": 0, "z": 0},
                "blockingRatio": 0.6,
            }
        ],
        "pedestrians": [],
        "runtime": {"maxDurationSec": 60},
    }


def test_consistency_checker_does_not_require_unspecified_numeric_constraints() -> None:
    result = generate_setup_pair_queue(_world_config(), episode_count=1)

    consistency = check_setup_pair_queue_consistency(
        result,
        fixed_constraints={"pedestrianCount": 0, "obstacleType": "static_obstacle"},
        expected_episode_count=1,
    )

    assert consistency.passed is True
    assert consistency.issues == []


def test_consistency_checker_reports_critical_artifact_path_mismatch() -> None:
    result = generate_setup_pair_queue(_world_config(), episode_count=1)
    result.run_queue.runs[0].episode_setup = "Json/Input/EpisodeSetup_wrong.json"

    consistency = check_setup_pair_queue_consistency(result, fixed_constraints={}, expected_episode_count=1)

    assert consistency.passed is False
    assert {issue.code for issue in consistency.issues} == {"artifact_path_mismatch"}


def test_consistency_checker_reports_fixed_constraint_mismatch() -> None:
    result = generate_setup_pair_queue(_world_config(), episode_count=1)

    consistency = check_setup_pair_queue_consistency(
        result,
        fixed_constraints={"goalDistanceM": 10.0, "obstacleCount": 2, "pedestrianCount": 0},
        expected_episode_count=1,
    )

    assert consistency.passed is False
    assert {"goal_distance_mismatch", "obstacle_count_mismatch"} <= {
        issue.code for issue in consistency.issues
    }


def test_consistency_checker_rejects_narrow_gap_marked_passable() -> None:
    result = generate_setup_pair_queue(_world_config(), episode_count=1)
    obstacle = result.items[0].episode_setup.actors.static_obstacles[0]
    obstacle.properties["passability"] = "passable"

    consistency = check_setup_pair_queue_consistency(result, fixed_constraints={}, expected_episode_count=1)

    assert consistency.passed is False
    assert any(issue.code == "narrow_gap_marked_passable" for issue in consistency.issues)


def test_consistency_checker_accepts_narrow_gap_marked_blocked_path() -> None:
    result = generate_setup_pair_queue(_world_config(), episode_count=1)
    obstacle = result.items[0].episode_setup.actors.static_obstacles[0]
    obstacle.properties["passability"] = "blocked_path"

    consistency = check_setup_pair_queue_consistency(result, fixed_constraints={}, expected_episode_count=1)

    assert consistency.passed is True


def test_consistency_checker_accepts_obstacle_without_passability_property() -> None:
    result = generate_setup_pair_queue(_world_config(), episode_count=1)
    obstacle = result.items[0].episode_setup.actors.static_obstacles[0]
    obstacle.properties.pop("passability", None)

    consistency = check_setup_pair_queue_consistency(result, fixed_constraints={}, expected_episode_count=1)

    assert consistency.passed is True
