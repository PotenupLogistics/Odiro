from __future__ import annotations

from dataclasses import dataclass, field
from typing import Any


@dataclass(frozen=True)
class DeliveryBotTuningVariation:
    profile: str
    values: dict[str, Any] = field(default_factory=dict)
    changed_parameters: list[str] = field(default_factory=list)


def _set_if_not_fixed(
    values: dict[str, Any],
    changed: list[str],
    fixed: dict[str, Any],
    key: str,
    value: Any,
    target: str,
) -> None:
    if key in fixed:
        return
    values[key] = value
    changed.append(target)


def delivery_bot_tuning_for_episode(
    episode_index: int,
    fixed_parameters: dict[str, Any] | None = None,
    scenario_intent: str | None = None,
) -> DeliveryBotTuningVariation:
    fixed = fixed_parameters or {}
    baseline = {
        "maxSpeedKmh": 10.0,
        "targetSpeedKmh": 10.0,
        "obstacleSlowSpeedKmh": 2.0,
        "stopDistanceM": 1.2,
        "slowDownDistanceM": 3.5,
        "frontHalfAngleDegree": 30.0,
        "minTurnSpeedKmh": 1.0,
    }
    profiles: list[tuple[str, dict[str, Any]]] = [
        ("baseline", baseline),
        ("baseline", baseline),
        ("baseline", baseline),
        (
            "conservative_lidar",
            {
                **baseline,
                "stopDistanceM": 1.4,
                "slowDownDistanceM": 4.5,
                "frontHalfAngleDegree": 45.0,
            },
        ),
        (
            "slower_path_follow",
            {
                **baseline,
                "maxSpeedKmh": 8.0,
                "targetSpeedKmh": 8.0,
                "obstacleSlowSpeedKmh": 1.5,
                "minTurnSpeedKmh": 0.8,
            },
        ),
    ]
    profile, profile_values = profiles[episode_index] if episode_index < len(profiles) else profiles[0]
    values: dict[str, Any] = {}
    changed: list[str] = []
    targets = {
        "maxSpeedKmh": "deliveryBotSetup.robot.drive.max_speed_kmh",
        "targetSpeedKmh": "deliveryBotSetup.robot.path_follow.target_speed_kmh",
        "obstacleSlowSpeedKmh": "deliveryBotSetup.robot.path_follow.obstacle_slow_speed_kmh",
        "stopDistanceM": "deliveryBotSetup.robot.lidar.stop_distance_m",
        "slowDownDistanceM": "deliveryBotSetup.robot.lidar.slow_down_distance_m",
        "frontHalfAngleDegree": "deliveryBotSetup.robot.lidar.front_half_angle_degree",
        "minTurnSpeedKmh": "deliveryBotSetup.robot.path_follow.min_turn_speed_kmh",
    }
    for key, value in profile_values.items():
        _set_if_not_fixed(values, changed, fixed, key, value, targets[key])
    return DeliveryBotTuningVariation(profile, values, changed)
