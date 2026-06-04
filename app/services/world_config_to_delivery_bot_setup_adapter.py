from __future__ import annotations

from typing import Any

from app.models.delivery_bot_setup import (
    DeliveryBotDriveConfig,
    DeliveryBotLidarConfig,
    DeliveryBotPathFollowConfig,
    DeliveryBotSetup,
    DeliveryBotSetupRobot,
)


def _first_dict(*values: Any) -> dict[str, Any]:
    for value in values:
        if isinstance(value, dict):
            return value
    return {}


def _value(params: dict[str, Any], *keys: str, default: Any) -> Any:
    for key in keys:
        if key in params and params[key] is not None:
            return params[key]
    return default


def convert_world_config_to_delivery_bot_setup(
    world_config: dict[str, Any],
    policy_params: dict[str, Any] | None = None,
) -> DeliveryBotSetup:
    constraints = world_config.get("constraints") if isinstance(world_config, dict) else None
    environment_sampling = world_config.get("environmentSampling") if isinstance(world_config, dict) else None
    params = _first_dict(policy_params, environment_sampling, constraints)

    max_speed = float(_value(params, "maxSpeedKmh", "targetSpeedKmh", default=10.0))
    target_speed = float(_value(params, "targetSpeedKmh", "maxSpeedKmh", default=max_speed))
    stop_distance = float(_value(params, "stopDistanceM", default=1.2))
    slow_down_distance = float(_value(params, "slowDownDistanceM", default=3.5))

    return DeliveryBotSetup(
        robot=DeliveryBotSetupRobot(
            drive=DeliveryBotDriveConfig(
                max_speed_kmh=max_speed,
                slowdown_speed_range_kmh=float(params.get("slowdownSpeedRangeKmh", 2.0)),
                speed_limit_tolerance_kmh=params.get("speedLimitToleranceKmh"),
                speed_limit_brake=params.get("speedLimitBrake"),
                stop_brake_input=params.get("stopBrakeInput"),
                throttle_input_rate_per_second=params.get("throttleInputRatePerSecond"),
                brake_input_rate_per_second=params.get("brakeInputRatePerSecond"),
                steering_input_rate_per_second=params.get("steeringInputRatePerSecond"),
                acceleration_rate_kmh_per_second=params.get("accelerationRateKmhPerSecond"),
                deceleration_rate_kmh_per_second=params.get("decelerationRateKmhPerSecond"),
                use_handbrake_when_brake=params.get("useHandbrakeWhenBrake"),
                max_torque=params.get("maxTorque"),
                max_rpm=params.get("maxRpm"),
                engine_idle_rpm=params.get("engineIdleRpm"),
                engine_brake_effect=params.get("engineBrakeEffect"),
                engine_rev_up_moi=params.get("engineRevUpMoi"),
                engine_rev_down_rate=params.get("engineRevDownRate"),
            ),
            path_follow=DeliveryBotPathFollowConfig(
                target_speed_kmh=target_speed,
                look_ahead_distance_m=float(params.get("lookAheadDistanceM", 1.0)),
                obstacle_slow_speed_kmh=float(params.get("obstacleSlowSpeedKmh", 2.0)),
                draw_debug=params.get("pathFollowDrawDebug"),
                path_point_acceptance_distance_m=params.get("pathPointAcceptanceDistanceM"),
                goal_acceptance_distance_m=params.get("goalAcceptanceDistanceM"),
                steering_sensitivity=params.get("steeringSensitivity"),
                min_turn_speed_kmh=params.get("minTurnSpeedKmh"),
            ),
            lidar=DeliveryBotLidarConfig(
                scan_range_m=float(params.get("scanRangeM", 5.0)),
                angle_step_degree=float(params.get("angleStepDegree", 5.0)),
                stop_distance_m=stop_distance,
                slow_down_distance_m=slow_down_distance,
                draw_debug=params.get("lidarDrawDebug"),
                sensor_height_m=params.get("sensorHeightM"),
                front_half_angle_degree=params.get("frontHalfAngleDegree"),
                store_missed_rays=params.get("storeMissedRays"),
                trace_channel=params.get("traceChannel"),
                ignore_tags=params.get("ignoreTags"),
            ),
        )
    )
