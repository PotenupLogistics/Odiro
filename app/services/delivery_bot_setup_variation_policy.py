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
    values: dict[str, Any] = {}
    changed: list[str] = []
    if episode_index == 3:
        _set_if_not_fixed(values, changed, fixed, "stopDistanceM", 1.4, "deliveryBotSetup.robot.lidar.stop_distance_m")
        _set_if_not_fixed(values, changed, fixed, "slowDownDistanceM", 4.0, "deliveryBotSetup.robot.lidar.slow_down_distance_m")
        _set_if_not_fixed(values, changed, fixed, "frontHalfAngleDegree", 25.0, "deliveryBotSetup.robot.lidar.front_half_angle_degree")
        return DeliveryBotTuningVariation("conservative_lidar", values, changed)
    if episode_index == 4:
        _set_if_not_fixed(values, changed, fixed, "maxSpeedKmh", 8.0, "deliveryBotSetup.robot.drive.max_speed_kmh")
        _set_if_not_fixed(values, changed, fixed, "targetSpeedKmh", 8.0, "deliveryBotSetup.robot.path_follow.target_speed_kmh")
        _set_if_not_fixed(values, changed, fixed, "obstacleSlowSpeedKmh", 1.2, "deliveryBotSetup.robot.path_follow.obstacle_slow_speed_kmh")
        _set_if_not_fixed(values, changed, fixed, "minTurnSpeedKmh", 1.0, "deliveryBotSetup.robot.path_follow.min_turn_speed_kmh")
        return DeliveryBotTuningVariation("slower_path_follow", values, changed)
    return DeliveryBotTuningVariation("baseline", {}, [])
