from __future__ import annotations

from typing import Any

from pydantic import ValidationError

from app.models.episode_setup import EpisodeSetup, SetupValidationError, SetupValidationResult, SetupValidationWarning


ALLOWED_STATIC_PROP_IDS = {
    "obstacle.box_01",
    "obstacle.road_barrier_01",
    "obstacle.road_cone_01",
}

FORBIDDEN_ACTOR_FIELDS = {"transform", "location_m", "rotation_deg", "scale"}


def _error(code: str, message: str, path: str | None = None) -> SetupValidationError:
    return SetupValidationError(code=code, message=message, path=path)


def _warning(code: str, message: str, path: str | None = None) -> SetupValidationWarning:
    return SetupValidationWarning(code=code, message=message, path=path)


def _is_number(value: Any) -> bool:
    return isinstance(value, (int, float)) and not isinstance(value, bool)


def _validate_xy(value: Any, path: str, errors: list[SetupValidationError]) -> None:
    if not isinstance(value, list) or len(value) != 2 or not all(_is_number(item) for item in value):
        errors.append(_error("invalid_xy_m", f"{path} must contain exactly two numeric values.", path))


def _validate_yaw(value: Any, path: str, errors: list[SetupValidationError]) -> None:
    if value is not None and not _is_number(value):
        errors.append(_error("invalid_yaw_deg", f"{path} must be numeric.", path))


def _validate_forbidden_fields(value: dict[str, Any], path: str, errors: list[SetupValidationError]) -> None:
    for field in FORBIDDEN_ACTOR_FIELDS:
        if field in value:
            errors.append(_error("forbidden_actor_field", f"{field} must not be output in EpisodeSetup.", f"{path}.{field}"))


def _validate_unique(values: list[str], code: str, label: str, errors: list[SetupValidationError]) -> None:
    seen: set[str] = set()
    for value in values:
        if value in seen:
            errors.append(_error(code, f"Duplicate {label}: {value}"))
        seen.add(value)


def _validate_raw_episode(payload: dict[str, Any], errors: list[SetupValidationError]) -> None:
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
    if "units" in payload:
        errors.append(_error("forbidden_root_field", "units must not be output in EpisodeSetup.", "units"))

    ground_model = payload.get("ground_model") if isinstance(payload.get("ground_model"), dict) else {}
    regions = ground_model.get("regions") if isinstance(ground_model.get("regions"), list) else []
    region_ids: list[str] = []
    for index, region in enumerate(regions):
        if not isinstance(region, dict):
            continue
        if region.get("region_id"):
            region_ids.append(str(region["region_id"]))
        shape = region.get("shape") if isinstance(region.get("shape"), dict) else {}
        if "center_m" in shape:
            errors.append(_error("legacy_region_field", "shape.center_m must not be used; use center_xy_m.", f"ground_model.regions[{index}].shape.center_m"))
        if "center_xy_m" not in shape:
            errors.append(_error("missing_center_xy_m", "region shape must use center_xy_m.", f"ground_model.regions[{index}].shape.center_xy_m"))
        else:
            _validate_xy(shape["center_xy_m"], f"ground_model.regions[{index}].shape.center_xy_m", errors)
        if "size_m" not in shape:
            errors.append(_error("missing_size_m", "region shape must use size_m.", f"ground_model.regions[{index}].shape.size_m"))
        else:
            _validate_xy(shape["size_m"], f"ground_model.regions[{index}].shape.size_m", errors)
        _validate_yaw(shape.get("yaw_deg"), f"ground_model.regions[{index}].shape.yaw_deg", errors)
    _validate_unique(region_ids, "duplicate_region_id", "region_id", errors)

    paths = payload.get("paths") if isinstance(payload.get("paths"), list) else []
    path_ids: list[str] = []
    for index, path in enumerate(paths):
        if not isinstance(path, dict):
            continue
        if path.get("path_id"):
            path_ids.append(str(path["path_id"]))
        for field in ["role", "type"]:
            if field in path:
                errors.append(_error("forbidden_path_field", f"paths.{field} must not be output.", f"paths[{index}].{field}"))
        if "points_m" in path:
            errors.append(_error("legacy_path_field", "points_m must not be used; use points_xy_m.", f"paths[{index}].points_m"))
        if "points_xy_m" not in path:
            errors.append(_error("missing_points_xy_m", "path must use points_xy_m.", f"paths[{index}].points_xy_m"))
        else:
            for point_index, point in enumerate(path["points_xy_m"]):
                _validate_xy(point, f"paths[{index}].points_xy_m[{point_index}]", errors)
    _validate_unique(path_ids, "duplicate_path_id", "path_id", errors)
    path_id_set = set(path_ids)

    actors = payload.get("actors") if isinstance(payload.get("actors"), dict) else {}
    instance_ids: list[str] = []
    robot = actors.get("robot") if isinstance(actors.get("robot"), dict) else {}
    if robot:
        _validate_forbidden_fields(robot, "actors.robot", errors)
        if robot.get("instance_id"):
            instance_ids.append(str(robot["instance_id"]))
        _validate_xy(robot.get("xy_m"), "actors.robot.xy_m", errors)
        _validate_yaw(robot.get("yaw_deg"), "actors.robot.yaw_deg", errors)
        if robot.get("spawn_only") is False:
            route = robot.get("route") if isinstance(robot.get("route"), dict) else None
            if route is None or "goal_xy_m" not in route:
                errors.append(_error("missing_robot_goal", "route.goal_xy_m is required when spawn_only is false.", "actors.robot.route.goal_xy_m"))
            elif route is not None:
                _validate_xy(route.get("goal_xy_m"), "actors.robot.route.goal_xy_m", errors)

    obstacles = actors.get("static_obstacles") if isinstance(actors.get("static_obstacles"), list) else []
    for index, obstacle in enumerate(obstacles):
        if not isinstance(obstacle, dict):
            continue
        _validate_forbidden_fields(obstacle, f"actors.static_obstacles[{index}]", errors)
        if obstacle.get("instance_id"):
            instance_ids.append(str(obstacle["instance_id"]))
        _validate_xy(obstacle.get("xy_m"), f"actors.static_obstacles[{index}].xy_m", errors)
        _validate_yaw(obstacle.get("yaw_deg"), f"actors.static_obstacles[{index}].yaw_deg", errors)
        if obstacle.get("prop_id") not in ALLOWED_STATIC_PROP_IDS:
            errors.append(_error("unknown_prop_id", f"static obstacle prop_id is not allowed: {obstacle.get('prop_id')}", f"actors.static_obstacles[{index}].prop_id"))

    pedestrians = actors.get("pedestrians") if isinstance(actors.get("pedestrians"), list) else []
    for index, pedestrian in enumerate(pedestrians):
        if not isinstance(pedestrian, dict):
            continue
        _validate_forbidden_fields(pedestrian, f"actors.pedestrians[{index}]", errors)
        if pedestrian.get("instance_id"):
            instance_ids.append(str(pedestrian["instance_id"]))
        _validate_xy(pedestrian.get("xy_m"), f"actors.pedestrians[{index}].xy_m", errors)
        _validate_yaw(pedestrian.get("yaw_deg"), f"actors.pedestrians[{index}].yaw_deg", errors)
        if pedestrian.get("path_id") not in path_id_set:
            errors.append(_error("missing_pedestrian_path", f"pedestrian path_id does not exist: {pedestrian.get('path_id')}", f"actors.pedestrians[{index}].path_id"))
    _validate_unique(instance_ids, "duplicate_instance_id", "instance_id", errors)


def validate_episode_setup(episode_setup: EpisodeSetup | dict[str, Any]) -> SetupValidationResult:
    errors: list[SetupValidationError] = []
    warnings: list[SetupValidationWarning] = []
    if isinstance(episode_setup, dict):
        _validate_raw_episode(episode_setup, errors)
    try:
        episode = episode_setup if isinstance(episode_setup, EpisodeSetup) else EpisodeSetup.model_validate(episode_setup)
    except ValidationError as exc:
        errors.extend(
            _error("model_validation_error", error["msg"], ".".join(str(part) for part in error["loc"]))
            for error in exc.errors()
        )
        return SetupValidationResult(valid=False, errors=errors, warnings=warnings)

    if episode.actors.robot.spawn_only is False and episode.actors.robot.route is None:
        errors.append(_error("missing_robot_goal", "route.goal_xy_m is required when spawn_only is false.", "actors.robot.route.goal_xy_m"))
    return SetupValidationResult(valid=not errors, errors=errors, warnings=warnings)
