from __future__ import annotations

from typing import Any

from pydantic import ValidationError

from app.models.episode_spec import (
    EpisodeSpec,
    EpisodeValidationError,
    EpisodeValidationResult,
    EpisodeValidationWarning,
)


ALLOWED_STATIC_PROP_IDS = {
    "obstacle.bin",
    "obstacle.box_01",
    "obstacle.box_02",
    "obstacle.box_03",
    "obstacle.fire_hydrant",
    "obstacle.mailbox",
    "obstacle.manhole_01",
    "obstacle.manhole_02",
    "obstacle.manhole_03",
    "obstacle.manhole_04",
    "obstacle.road_cone_01",
    "obstacle.road_cone_02",
    "obstacle.road_barrier_01",
    "obstacle.road_barrier_02",
    "obstacle.street_bank",
    "obstacle.trash_bin",
}


def _error(code: str, message: str, path: str | None = None) -> EpisodeValidationError:
    return EpisodeValidationError(code=code, message=message, path=path)


def _warning(code: str, message: str, path: str | None = None) -> EpisodeValidationWarning:
    return EpisodeValidationWarning(code=code, message=message, path=path)


def _validate_unique(values: list[str], code: str, label: str, errors: list[EpisodeValidationError]) -> None:
    seen: set[str] = set()
    for value in values:
        if value in seen:
            errors.append(_error(code, f"Duplicate {label}: {value}"))
        seen.add(value)


def _validate_vector(length: int, value: list[float], path: str, errors: list[EpisodeValidationError]) -> None:
    if len(value) != length:
        errors.append(_error("invalid_vector_length", f"{path} must contain {length} values.", path))


def _validate_properties(
    properties: dict[str, Any],
    path: str,
    errors: list[EpisodeValidationError],
) -> None:
    for key, value in properties.items():
        value_path = f"{path}.{key}"
        if isinstance(value, dict):
            errors.append(_error("invalid_property_value", "properties must be a shallow map; nested objects are not allowed.", value_path))
            continue
        if isinstance(value, list):
            if len(value) != 3 or not all(isinstance(item, (int, float)) for item in value):
                errors.append(_error("invalid_property_value", "property arrays must be numeric vectors with exactly 3 values.", value_path))
            continue
        if value is not None and not isinstance(value, (bool, int, float, str)):
            errors.append(_error("invalid_property_value", "properties values must be boolean, number, string, or numeric vector3.", value_path))


def _validate_raw_properties(value: Any, path: str, errors: list[EpisodeValidationError]) -> None:
    if not isinstance(value, dict):
        return
    properties = value.get("properties")
    if isinstance(properties, dict):
        _validate_properties(properties, f"{path}.properties", errors)


def _validate_raw_episode_dict(episode_spec: dict[str, Any], errors: list[EpisodeValidationError]) -> None:
    actors = episode_spec.get("actors") if isinstance(episode_spec.get("actors"), dict) else {}
    _validate_raw_properties(actors.get("robot"), "actors.robot", errors)
    for index, obstacle in enumerate(actors.get("static_obstacles") or []):
        _validate_raw_properties(obstacle, f"actors.static_obstacles[{index}]", errors)
    for index, pedestrian in enumerate(actors.get("pedestrians") or []):
        _validate_raw_properties(pedestrian, f"actors.pedestrians[{index}]", errors)


def validate_episode_spec(episode_spec: EpisodeSpec | dict[str, Any]) -> EpisodeValidationResult:
    errors: list[EpisodeValidationError] = []
    warnings: list[EpisodeValidationWarning] = []
    if isinstance(episode_spec, dict):
        _validate_raw_episode_dict(episode_spec, errors)
    try:
        episode = episode_spec if isinstance(episode_spec, EpisodeSpec) else EpisodeSpec.model_validate(episode_spec)
    except ValidationError as exc:
        return EpisodeValidationResult(
            valid=False,
            errors=[
                _error("model_validation_error", error["msg"], ".".join(str(part) for part in error["loc"]))
                for error in exc.errors()
            ] + errors,
            warnings=[],
        )

    if episode.units.distance != "m":
        errors.append(_error("invalid_distance_unit", "units.distance must be m.", "units.distance"))
    if episode.units.angle != "deg":
        errors.append(_error("invalid_angle_unit", "units.angle must be deg.", "units.angle"))

    instance_ids = [episode.actors.robot.instance_id]
    instance_ids.extend(actor.instance_id for actor in episode.actors.pedestrians)
    instance_ids.extend(actor.instance_id for actor in episode.actors.static_obstacles)
    _validate_unique(instance_ids, "duplicate_instance_id", "instance_id", errors)

    path_ids = [path.path_id for path in episode.paths]
    _validate_unique(path_ids, "duplicate_path_id", "path_id", errors)
    path_id_set = set(path_ids)
    for index, pedestrian in enumerate(episode.actors.pedestrians):
        if pedestrian.path_id not in path_id_set:
            errors.append(
                _error(
                    "missing_pedestrian_path",
                    f"pedestrian path_id does not exist: {pedestrian.path_id}",
                    f"actors.pedestrians[{index}].path_id",
                )
            )
        if not 0.8 <= pedestrian.movement.speed_mps <= 1.8:
            warnings.append(
                _warning(
                    "pedestrian_speed_outside_recommended_range",
                    "pedestrian speed_mps is outside the recommended 0.8~1.8 range.",
                    f"actors.pedestrians[{index}].movement.speed_mps",
                )
            )
        _validate_properties(pedestrian.properties, f"actors.pedestrians[{index}].properties", errors)

    for index, obstacle in enumerate(episode.actors.static_obstacles):
        if obstacle.prop_id not in ALLOWED_STATIC_PROP_IDS:
            errors.append(
                _error(
                    "unknown_prop_id",
                    f"static obstacle prop_id is not in the allowed catalog: {obstacle.prop_id}",
                    f"actors.static_obstacles[{index}].prop_id",
                )
            )
        _validate_vector(3, obstacle.transform.location_m, f"actors.static_obstacles[{index}].transform.location_m", errors)
        _validate_vector(3, obstacle.transform.scale, f"actors.static_obstacles[{index}].transform.scale", errors)
        _validate_properties(obstacle.properties, f"actors.static_obstacles[{index}].properties", errors)

    for index, region in enumerate(episode.ground_model.regions):
        if region.shape.type != "rectangle":
            errors.append(_error("invalid_ground_shape", "ground region shape.type must be rectangle.", f"ground_model.regions[{index}].shape.type"))
        _validate_vector(3, region.shape.center_m, f"ground_model.regions[{index}].shape.center_m", errors)
        _validate_vector(2, region.shape.size_m, f"ground_model.regions[{index}].shape.size_m", errors)

    _validate_vector(3, episode.actors.robot.transform.location_m, "actors.robot.transform.location_m", errors)
    _validate_vector(3, episode.actors.robot.transform.scale, "actors.robot.transform.scale", errors)
    _validate_properties(episode.actors.robot.properties, "actors.robot.properties", errors)
    if not episode.actors.robot.spawn_only and episode.actors.robot.route is None:
        errors.append(_error("missing_robot_route", "robot route.goal_m is required when spawn_only is false.", "actors.robot.route"))
    if episode.actors.robot.route is not None:
        _validate_vector(3, episode.actors.robot.route.goal_m, "actors.robot.route.goal_m", errors)

    for path_index, path in enumerate(episode.paths):
        for point_index, point in enumerate(path.points_m):
            _validate_vector(3, point, f"paths[{path_index}].points_m[{point_index}]", errors)

    return EpisodeValidationResult(valid=not errors, errors=errors, warnings=warnings)
