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


def test_delivery_bot_variation_policy_returns_named_policy_profiles() -> None:
    profiles = [delivery_bot_tuning_for_episode(index, fixed_parameters={}) for index in range(5)]

    assert [profile.profile for profile in profiles] == [
        "baseline",
        "short_stop",
        "long_stop",
        "early_slowdown",
        "low_speed",
    ]
    assert profiles[0].values["stopDistanceM"] == 1.2
    assert profiles[1].values["stopDistanceM"] == 0.8
    assert profiles[2].values["stopDistanceM"] == 1.6
    assert profiles[3].values["slowDownDistanceM"] == 4.5
    assert profiles[4].values["targetSpeedKmh"] == 8.0

def test_policy_profiles_keep_common_field_set_and_valid_ranges() -> None:
    profiles = [delivery_bot_tuning_for_episode(index, fixed_parameters={}) for index in range(5)]
    field_set = set(profiles[0].values)

    assert field_set == {
        "maxSpeedKmh",
        "targetSpeedKmh",
        "obstacleSlowSpeedKmh",
        "stopDistanceM",
        "slowDownDistanceM",
        "frontHalfAngleDegree",
        "minTurnSpeedKmh",
    }
    for profile in profiles:
        assert set(profile.values) == field_set
        assert profile.values["targetSpeedKmh"] <= profile.values["maxSpeedKmh"]
        assert profile.values["obstacleSlowSpeedKmh"] <= profile.values["targetSpeedKmh"]
        assert profile.values["stopDistanceM"] < profile.values["slowDownDistanceM"]


def test_early_slowdown_profile_changes_slowdown_distance() -> None:
    tuning = delivery_bot_tuning_for_episode(3, fixed_parameters={})

    assert tuning.profile == "early_slowdown"
    assert tuning.values["stopDistanceM"] == 1.2
    assert tuning.values["slowDownDistanceM"] == 4.5
    assert tuning.values["frontHalfAngleDegree"] == 30.0
    assert tuning.values["slowDownDistanceM"] >= tuning.values["stopDistanceM"] + 0.1
    assert "deliveryBotSetup.robot.lidar.slow_down_distance_m" in tuning.changed_parameters


def test_low_speed_profile_changes_speed_values_without_overwriting_fixed_values() -> None:
    tuning = delivery_bot_tuning_for_episode(4, fixed_parameters={"maxSpeedKmh": 10.0})

    assert tuning.profile == "low_speed"
    assert "maxSpeedKmh" not in tuning.values
    assert tuning.values["targetSpeedKmh"] == 8.0
    assert tuning.values["obstacleSlowSpeedKmh"] == 1.5
    assert tuning.values["minTurnSpeedKmh"] == 1.0
    assert "deliveryBotSetup.robot.drive.max_speed_kmh" not in tuning.changed_parameters
