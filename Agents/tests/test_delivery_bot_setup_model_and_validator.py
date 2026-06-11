from __future__ import annotations

from app.models.delivery_bot_setup import DeliveryBotSetup
from app.services.delivery_bot_setup_validator import validate_delivery_bot_setup


def _valid_delivery_bot_setup() -> dict:
    return {
        "schema": "delivery_bot_setup",
        "version": 1,
        "robot": {
            "drive": {"max_speed_kmh": 10.0, "slowdown_speed_range_kmh": 2.0},
            "path_follow": {
                "target_speed_kmh": 10.0,
                "look_ahead_distance_m": 1.0,
                "obstacle_slow_speed_kmh": 2.0,
            },
            "lidar": {
                "scan_range_m": 5.0,
                "angle_step_degree": 5.0,
                "stop_distance_m": 1.2,
                "slow_down_distance_m": 3.5,
            },
        },
    }


def test_delivery_bot_setup_model_accepts_tuning_only_shape() -> None:
    setup = DeliveryBotSetup.model_validate(_valid_delivery_bot_setup())

    assert setup.robot.drive.max_speed_kmh == 10.0
    assert setup.robot.path_follow.look_ahead_distance_m == 1.0
    assert setup.robot.lidar.stop_distance_m == 1.2


def test_delivery_bot_setup_validator_rejects_placement_and_route_fields() -> None:
    payload = _valid_delivery_bot_setup()
    payload["run"] = {}
    payload["actors"] = {}
    payload["robot"]["instance_id"] = "robot_01"
    payload["robot"]["route"] = {"goal_xy_m": [1, 0]}
    payload["robot"]["xy_m"] = [0, 0]

    result = validate_delivery_bot_setup(payload)

    assert result.valid is False
    codes = {error.code for error in result.errors}
    assert "forbidden_root_field" in codes
    assert "forbidden_robot_field" in codes


def test_delivery_bot_setup_validator_rejects_explicit_null_values() -> None:
    payload = _valid_delivery_bot_setup()
    payload["robot"]["drive"]["speed_limit_brake"] = None
    payload["robot"]["path_follow"]["draw_debug"] = None
    payload["robot"]["lidar"]["ignore_tags"] = None

    result = validate_delivery_bot_setup(payload)

    assert result.valid is False
    assert "explicit_null_field" in {error.code for error in result.errors}


def test_delivery_bot_setup_validator_checks_numeric_ranges() -> None:
    payload = _valid_delivery_bot_setup()
    payload["robot"]["drive"]["speed_limit_brake"] = 2.0
    payload["robot"]["path_follow"]["look_ahead_distance_m"] = 0.0
    payload["robot"]["lidar"]["stop_distance_m"] = 1.2
    payload["robot"]["lidar"]["slow_down_distance_m"] = 1.25
    payload["robot"]["lidar"]["front_half_angle_degree"] = 220

    result = validate_delivery_bot_setup(payload)

    assert result.valid is False
    codes = {error.code for error in result.errors}
    assert "value_out_of_range" in codes
    assert "invalid_slow_down_distance" in codes
