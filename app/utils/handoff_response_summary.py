from __future__ import annotations

from typing import Any

from app.services.route_geometry_utils import compute_midpoint, distance_between_points


def _as_dict(value: Any) -> dict[str, Any]:
    return value if isinstance(value, dict) else {}


def _as_list(value: Any) -> list[Any]:
    return value if isinstance(value, list) else []


def _first_dict(value: Any) -> dict[str, Any]:
    values = _as_list(value)
    return values[0] if values and isinstance(values[0], dict) else {}


def _bool_or_none(value: Any) -> bool | None:
    return value if isinstance(value, bool) else None


def _validation_passed(value: Any) -> bool | None:
    payload = _as_dict(value)
    if "passed" in payload:
        return _bool_or_none(payload.get("passed"))
    if "valid" in payload:
        return _bool_or_none(payload.get("valid"))
    return None


def _effective_response_format(response: dict[str, Any]) -> str | None:
    diagnostics = _as_dict(response.get("diagnostics"))
    if diagnostics.get("effectiveResponseFormat"):
        return str(diagnostics["effectiveResponseFormat"])
    if response.get("episodeSpec") is not None and response.get("worldConfig") is not None:
        return "both"
    if response.get("episodeSpec") is not None:
        return "episode_spec"
    if response.get("worldConfig") is not None:
        return "world_config"
    return None


def _applied_patches(response: dict[str, Any]) -> list[str]:
    patch_types: list[str] = []
    post_processing = _as_dict(response.get("postProcessing"))
    for patch in _as_list(post_processing.get("patches")):
        if isinstance(patch, dict) and patch.get("patchType"):
            patch_types.append(str(patch["patchType"]))

    diagnostics = _as_dict(response.get("diagnostics"))
    for attempt in _as_list(diagnostics.get("attempts")):
        if not isinstance(attempt, dict):
            continue
        for patch in _as_list(attempt.get("scenarioPostProcessingPatches")):
            if isinstance(patch, dict) and patch.get("patchType"):
                patch_types.append(str(patch["patchType"]))
            elif isinstance(patch, str):
                patch_types.append(patch)

    return list(dict.fromkeys(patch_types))


def _warnings(response: dict[str, Any]) -> list[str]:
    values: list[str] = []
    for warning in _as_list(response.get("warnings")):
        if isinstance(warning, dict):
            message = warning.get("message") or warning.get("code")
            if message:
                values.append(str(message))
        elif warning is not None:
            values.append(str(warning))
    return values


def summarize_handoff_response(response_json: dict[str, Any], http_status: int | None = None) -> dict[str, Any]:
    """Return a JSON-safe handoff summary without full WorldConfig or EpisodeSpec payloads."""
    response = _as_dict(response_json)
    metadata = _as_dict(response.get("metadata"))
    validation = _as_dict(response.get("validation"))
    world_config = _as_dict(response.get("worldConfig"))
    episode_spec = _as_dict(response.get("episodeSpec"))
    scenario_reflection = _as_dict(response.get("scenarioReflection"))
    post_processing = _as_dict(response.get("postProcessing"))
    episode_reflection = _as_dict(response.get("episodeScenarioReflection"))
    diagnostics = _as_dict(response.get("diagnostics"))
    environment_sampling = _as_dict(diagnostics.get("environmentSampling"))
    environment_parameters = _as_dict(environment_sampling.get("parameters"))

    map_config = _as_dict(world_config.get("map"))
    obstacles = _as_list(world_config.get("obstacles"))
    obstacle = _first_dict(obstacles)
    pedestrians = _as_list(world_config.get("pedestrians"))

    actors = _as_dict(episode_spec.get("actors"))
    static_obstacles = _as_list(actors.get("static_obstacles"))
    static_obstacle = _first_dict(static_obstacles)
    static_obstacle_properties = _as_dict(static_obstacle.get("properties"))
    episode_pedestrians = _as_list(actors.get("pedestrians"))
    episode_paths = _as_list(episode_spec.get("paths"))
    checked_requirements = _as_list(scenario_reflection.get("checkedRequirements"))
    route_midpoint_expected = any(
        isinstance(requirement, dict) and requirement.get("requirementId") == "obstacle_route_midpoint"
        for requirement in checked_requirements
    )
    obstacle_distance_from_midpoint = None
    obstacle_near_route_midpoint = None
    if world_config:
        robot = _as_dict(world_config.get("robot"))
        midpoint = compute_midpoint(_as_dict(robot.get("spawn")), _as_dict(robot.get("goal")))
        if isinstance(obstacle.get("position"), dict):
            obstacle_distance_from_midpoint = round(distance_between_points(obstacle["position"], midpoint), 3)
            obstacle_near_route_midpoint = obstacle_distance_from_midpoint <= 50.0
    elif episode_spec:
        robot = _as_dict(actors.get("robot"))
        transform = _as_dict(robot.get("transform"))
        route = _as_dict(robot.get("route"))
        spawn_m = transform.get("location_m")
        goal_m = route.get("goal_m")
        obstacle_transform = _as_dict(static_obstacle.get("transform"))
        obstacle_location = obstacle_transform.get("location_m")
        if (
            isinstance(spawn_m, list)
            and len(spawn_m) >= 3
            and isinstance(goal_m, list)
            and len(goal_m) >= 3
            and isinstance(obstacle_location, list)
            and len(obstacle_location) >= 3
        ):
            midpoint = compute_midpoint(
                {"x": spawn_m[0], "y": spawn_m[1], "z": spawn_m[2]},
                {"x": goal_m[0], "y": goal_m[1], "z": goal_m[2]},
            )
            obstacle_distance_from_midpoint = round(
                distance_between_points(
                    {"x": obstacle_location[0], "y": obstacle_location[1], "z": obstacle_location[2]},
                    midpoint,
                ),
                3,
            )
            obstacle_near_route_midpoint = obstacle_distance_from_midpoint <= 0.5

    summary = {
        "httpStatus": http_status,
        "success": _bool_or_none(response.get("success")),
        "handoffTarget": response.get("handoffTarget"),
        "effectiveResponseFormat": _effective_response_format(response),
        "providerUsed": metadata.get("provider"),
        "model": metadata.get("model"),
        "worldConfigExists": bool(world_config),
        "episodeSpecExists": bool(episode_spec),
        "schemaValidationPassed": _bool_or_none(validation.get("schemaValidationPassed")),
        "scenarioReflectionPassed": _bool_or_none(validation.get("scenarioReflectionPassed")),
        "contractValidationPassed": _bool_or_none(validation.get("contractValidationPassed")),
        "episodeValidationPassed": _validation_passed(response.get("episodeValidation")),
        "episodeScenarioReflectionPassed": _bool_or_none(episode_reflection.get("passed")),
        "sidewalkWidthCm": map_config.get("sidewalkWidthCm"),
        "obstacleExists": bool(obstacle),
        "obstacleType": obstacle.get("type"),
        "obstaclePosition": obstacle.get("position"),
        "blockingRatio": obstacle.get("blockingRatio"),
        "pedestriansEmpty": bool(world_config) and not pedestrians,
        "pathsEmpty": bool(episode_spec) and not episode_paths,
        "staticObstacleCount": len(static_obstacles),
        "staticObstacleSemanticType": static_obstacle_properties.get("semantic_type"),
        "staticObstacleBlockingRatio": static_obstacle_properties.get("blocking_ratio"),
        "pedestrianCount": len(episode_pedestrians),
        "pathCount": len(episode_paths),
        "checkedRequirementsCount": len(checked_requirements),
        "postProcessingApplied": _bool_or_none(post_processing.get("applied")),
        "appliedPatches": _applied_patches(response),
        "routeMidpointExpected": route_midpoint_expected,
        "obstacleNearRouteMidpoint": obstacle_near_route_midpoint,
        "obstacleDistanceFromMidpoint": obstacle_distance_from_midpoint,
        "warnings": _warnings(response),
        "errorCode": _as_dict(response.get("error")).get("code"),
        "environmentSamplingEnabled": environment_sampling.get("enabled"),
        "environmentSamplingSeed": environment_sampling.get("seed"),
        "environmentSamplingScenarioType": environment_sampling.get("scenarioType"),
        "sampledSidewalkWidthCm": environment_parameters.get("sidewalkWidthCm"),
        "sampledPedestrianCount": environment_parameters.get("pedestrianCount"),
        "sampledObstacleBlockingRatio": environment_parameters.get("obstacleBlockingRatio"),
        "sampledTimeLimitSec": environment_parameters.get("timeLimitSec"),
        "apiKeyStored": False,
        "fullPayloadStored": False,
        "fullEpisodeSpecStored": False,
    }
    return summary
