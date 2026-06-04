from __future__ import annotations

from typing import Any

from pydantic import ValidationError

from app.models.delivery_bot_setup import DeliveryBotSetup
from app.models.episode_setup import SetupValidationError, SetupValidationResult, SetupValidationWarning


FORBIDDEN_ROOT_FIELDS = {"run", "actors"}
FORBIDDEN_ROBOT_FIELDS = {
    "instance_id",
    "asset_id",
    "spawn_only",
    "route",
    "location",
    "transform",
    "xy_m",
    "yaw_deg",
}


def _error(code: str, message: str, path: str | None = None) -> SetupValidationError:
    return SetupValidationError(code=code, message=message, path=path)


def _is_number(value: Any) -> bool:
    return isinstance(value, (int, float)) and not isinstance(value, bool)


def _range(
    value: Any,
    path: str,
    errors: list[SetupValidationError],
    minimum: float | None = None,
    maximum: float | None = None,
) -> None:
    if value is None:
        return
    if not _is_number(value):
        errors.append(_error("value_out_of_range", f"{path} must be numeric.", path))
        return
    if minimum is not None and float(value) < minimum:
        errors.append(_error("value_out_of_range", f"{path} must be >= {minimum}.", path))
    if maximum is not None and float(value) > maximum:
        errors.append(_error("value_out_of_range", f"{path} must be <= {maximum}.", path))


def _validate_raw(payload: dict[str, Any], errors: list[SetupValidationError]) -> None:
    def reject_nulls(value: Any, path: str) -> None:
        if isinstance(value, dict):
            for key, item in value.items():
                child_path = f"{path}.{key}" if path else str(key)
                if item is None:
                    errors.append(_error("explicit_null_field", f"{child_path} must be omitted instead of null.", child_path))
                else:
                    reject_nulls(item, child_path)
        elif isinstance(value, list):
            for index, item in enumerate(value):
                reject_nulls(item, f"{path}[{index}]")

    reject_nulls(payload, "")
    for field in FORBIDDEN_ROOT_FIELDS:
        if field in payload:
            errors.append(_error("forbidden_root_field", f"{field} must not be output in DeliveryBotSetup.", field))
    robot = payload.get("robot") if isinstance(payload.get("robot"), dict) else {}
    for field in FORBIDDEN_ROBOT_FIELDS:
        if field in robot:
            errors.append(_error("forbidden_robot_field", f"robot.{field} must not be output in DeliveryBotSetup.", f"robot.{field}"))

    drive = robot.get("drive") if isinstance(robot.get("drive"), dict) else {}
    _range(drive.get("max_speed_kmh"), "robot.drive.max_speed_kmh", errors, minimum=0)
    _range(drive.get("slowdown_speed_range_kmh"), "robot.drive.slowdown_speed_range_kmh", errors, minimum=0.1)
    _range(drive.get("speed_limit_brake"), "robot.drive.speed_limit_brake", errors, minimum=0, maximum=1)
    _range(drive.get("stop_brake_input"), "robot.drive.stop_brake_input", errors, minimum=0, maximum=1)

    path_follow = robot.get("path_follow") if isinstance(robot.get("path_follow"), dict) else {}
    _range(path_follow.get("target_speed_kmh"), "robot.path_follow.target_speed_kmh", errors, minimum=0)
    _range(path_follow.get("look_ahead_distance_m"), "robot.path_follow.look_ahead_distance_m", errors, minimum=0.1)
    _range(path_follow.get("obstacle_slow_speed_kmh"), "robot.path_follow.obstacle_slow_speed_kmh", errors, minimum=0)

    lidar = robot.get("lidar") if isinstance(robot.get("lidar"), dict) else {}
    _range(lidar.get("scan_range_m"), "robot.lidar.scan_range_m", errors, minimum=0)
    _range(lidar.get("angle_step_degree"), "robot.lidar.angle_step_degree", errors, minimum=1.0)
    _range(lidar.get("stop_distance_m"), "robot.lidar.stop_distance_m", errors, minimum=0)
    _range(lidar.get("front_half_angle_degree"), "robot.lidar.front_half_angle_degree", errors, minimum=0, maximum=180)
    stop_distance = lidar.get("stop_distance_m")
    slow_down_distance = lidar.get("slow_down_distance_m")
    _range(slow_down_distance, "robot.lidar.slow_down_distance_m", errors, minimum=0)
    if _is_number(stop_distance) and _is_number(slow_down_distance):
        if float(slow_down_distance) < float(stop_distance) + 0.1:
            errors.append(
                _error(
                    "invalid_slow_down_distance",
                    "robot.lidar.slow_down_distance_m must be at least stop_distance_m + 0.1.",
                    "robot.lidar.slow_down_distance_m",
                )
            )


def validate_delivery_bot_setup(delivery_bot_setup: DeliveryBotSetup | dict[str, Any]) -> SetupValidationResult:
    errors: list[SetupValidationError] = []
    warnings: list[SetupValidationWarning] = []
    if isinstance(delivery_bot_setup, dict):
        _validate_raw(delivery_bot_setup, errors)
    try:
        setup = delivery_bot_setup if isinstance(delivery_bot_setup, DeliveryBotSetup) else DeliveryBotSetup.model_validate(delivery_bot_setup)
    except ValidationError as exc:
        errors.extend(
            _error("model_validation_error", error["msg"], ".".join(str(part) for part in error["loc"]))
            for error in exc.errors()
        )
        return SetupValidationResult(valid=False, errors=errors, warnings=warnings)

    if setup.robot.lidar.slow_down_distance_m < setup.robot.lidar.stop_distance_m + 0.1:
        errors.append(
            _error(
                "invalid_slow_down_distance",
                "robot.lidar.slow_down_distance_m must be at least stop_distance_m + 0.1.",
                "robot.lidar.slow_down_distance_m",
            )
        )
    return SetupValidationResult(valid=not errors, errors=errors, warnings=warnings)
