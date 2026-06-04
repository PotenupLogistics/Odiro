from __future__ import annotations

from app.services.delivery_bot_setup_validator import validate_delivery_bot_setup
from app.services.world_config_to_delivery_bot_setup_adapter import convert_world_config_to_delivery_bot_setup
from app.utils.json_sanitizer import remove_json_nulls


def test_delivery_bot_setup_adapter_returns_defaults_without_placement_fields() -> None:
    setup = convert_world_config_to_delivery_bot_setup({})
    payload = setup.model_dump(mode="json", by_alias=True, exclude_none=True)

    assert payload["robot"]["drive"]["max_speed_kmh"] == 10.0
    assert payload["robot"]["drive"]["slowdown_speed_range_kmh"] == 2.0
    assert payload["robot"]["path_follow"]["target_speed_kmh"] == 10.0
    assert payload["robot"]["path_follow"]["look_ahead_distance_m"] == 1.0
    assert payload["robot"]["path_follow"]["obstacle_slow_speed_kmh"] == 2.0
    assert payload["robot"]["lidar"]["scan_range_m"] == 5.0
    assert payload["robot"]["lidar"]["angle_step_degree"] == 5.0
    assert payload["robot"]["lidar"]["stop_distance_m"] == 1.2
    assert payload["robot"]["lidar"]["slow_down_distance_m"] == 3.5
    assert "route" not in payload["robot"]
    assert "instance_id" not in payload["robot"]
    assert "xy_m" not in payload["robot"]
    assert validate_delivery_bot_setup(setup).valid is True


def test_delivery_bot_setup_adapter_can_apply_policy_params() -> None:
    setup = convert_world_config_to_delivery_bot_setup(
        {},
        policy_params={
            "maxSpeedKmh": 8.0,
            "stopDistanceM": 1.5,
            "slowDownDistanceM": 4.0,
        },
    )

    assert setup.robot.drive.max_speed_kmh == 8.0
    assert setup.robot.path_follow.target_speed_kmh == 8.0
    assert setup.robot.lidar.stop_distance_m == 1.5
    assert setup.robot.lidar.slow_down_distance_m == 4.0


def test_delivery_bot_setup_export_payload_is_null_free_and_omits_unspecified_optional_fields() -> None:
    setup = convert_world_config_to_delivery_bot_setup({})

    payload = remove_json_nulls(setup.model_dump(mode="json", by_alias=True))

    assert "speed_limit_brake" not in payload["robot"]["drive"]
    assert "draw_debug" not in payload["robot"]["path_follow"]
    assert "ignore_tags" not in payload["robot"]["lidar"]
    assert "null" not in __import__("json").dumps(payload)


def test_delivery_bot_setup_adapter_can_output_optional_values_when_policy_selects_them() -> None:
    setup = convert_world_config_to_delivery_bot_setup(
        {},
        policy_params={
            "frontHalfAngleDegree": 25.0,
            "minTurnSpeedKmh": 1.0,
            "ignoreTags": ["NoCollision"],
        },
    )
    payload = remove_json_nulls(setup.model_dump(mode="json", by_alias=True))

    assert payload["robot"]["lidar"]["front_half_angle_degree"] == 25.0
    assert payload["robot"]["lidar"]["ignore_tags"] == ["NoCollision"]
    assert payload["robot"]["path_follow"]["min_turn_speed_kmh"] == 1.0
