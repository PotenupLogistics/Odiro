from __future__ import annotations

from app.services.delivery_bot_setup_defaults import DELIVERY_BOT_SETUP_DEFAULTS, DELIVERY_BOT_SETUP_LIMITS
from app.services.delivery_bot_setup_variation_policy import delivery_bot_tuning_for_episode


def test_delivery_bot_setup_default_catalog_matches_ue_document_values() -> None:
    assert DELIVERY_BOT_SETUP_DEFAULTS["drive"]["slowdown_speed_range_kmh"] == 4.0
    assert DELIVERY_BOT_SETUP_DEFAULTS["drive"]["speed_limit_brake"] == 0.08
    assert DELIVERY_BOT_SETUP_DEFAULTS["path_follow"]["obstacle_slow_speed_kmh"] == 1.5
    assert DELIVERY_BOT_SETUP_DEFAULTS["lidar"]["angle_step_degree"] == 2.0
    assert DELIVERY_BOT_SETUP_DEFAULTS["lidar"]["ignore_tags"] == ["NoCollision"]
    assert DELIVERY_BOT_SETUP_LIMITS["lidar.slow_down_distance_m"]["rule"] == ">= stop_distance_m + 0.1"


def test_delivery_bot_variation_policy_returns_baseline_for_first_three_episodes() -> None:
    for index in [0, 1, 2]:
        tuning = delivery_bot_tuning_for_episode(index, fixed_parameters={})
        assert tuning.values == {}
        assert tuning.changed_parameters == []


def test_conservative_lidar_profile_changes_safe_lidar_values() -> None:
    tuning = delivery_bot_tuning_for_episode(3, fixed_parameters={})

    assert tuning.values["stopDistanceM"] == 1.4
    assert tuning.values["slowDownDistanceM"] == 4.0
    assert tuning.values["frontHalfAngleDegree"] == 25.0
    assert tuning.values["slowDownDistanceM"] >= tuning.values["stopDistanceM"] + 0.1
    assert "deliveryBotSetup.robot.lidar.front_half_angle_degree" in tuning.changed_parameters


def test_slower_path_follow_profile_changes_speed_values_without_overwriting_fixed_values() -> None:
    tuning = delivery_bot_tuning_for_episode(4, fixed_parameters={"maxSpeedKmh": 10.0})

    assert "maxSpeedKmh" not in tuning.values
    assert tuning.values["targetSpeedKmh"] == 8.0
    assert tuning.values["obstacleSlowSpeedKmh"] == 1.2
    assert tuning.values["minTurnSpeedKmh"] == 1.0
    assert "deliveryBotSetup.robot.drive.max_speed_kmh" not in tuning.changed_parameters
