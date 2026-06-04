from __future__ import annotations

from typing import Any

from app.services.episode_spec_validator import ALLOWED_STATIC_PROP_IDS
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
    if response.get("episodeSetup") is not None and response.get("deliveryBotSetup") is not None:
        return "setup_pair"
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


def _episode_spec_missing_reason(response: dict[str, Any], diagnostics: dict[str, Any]) -> str | None:
    error_code = _as_dict(response.get("error")).get("code")
    if error_code:
        return str(error_code)
    failure_stage = diagnostics.get("failureStage")
    if failure_stage:
        return str(failure_stage)
    validation = _as_dict(diagnostics.get("validation"))
    if validation.get("status") == "failed":
        return "world_config_validation"
    return None


def _trace_source_types(generation_trace: dict[str, Any]) -> list[str]:
    source_types: list[str] = []
    for item in _as_list(generation_trace.get("evidenceItems")):
        if isinstance(item, dict) and item.get("sourceType"):
            source_types.append(str(item["sourceType"]))
    return list(dict.fromkeys(source_types))


def _coordinate_source(source_types: list[str]) -> str:
    has_user = "user_prompt" in source_types or "scenario_intent" in source_types
    has_sampling = "environment_sampling" in source_types
    has_placement = "placement_rule" in source_types
    active = [value for value, flag in [("explicit_user_coordinates", has_user), ("environment_sampling", has_sampling), ("placement_rule", has_placement)] if flag]
    if len(active) > 1:
        return "mixed"
    return active[0] if active else "unknown"


def _policy_rag_used_for(generation_trace: dict[str, Any]) -> str:
    trace_summary = _as_dict(generation_trace.get("summary"))
    if trace_summary.get("policyRagUsedFor"):
        return str(trace_summary["policyRagUsedFor"])
    items = _as_list(generation_trace.get("evidenceItems"))
    policy_items = [
        item for item in items if isinstance(item, dict) and item.get("sourceType") == "policy_rag"
    ]
    if not policy_items:
        return "unknown"
    if any("safety context" in str(item.get("reason", "")).lower() for item in policy_items):
        return "safety_context"
    if any(item.get("valueSummary") == "not_used" for item in policy_items):
        return "not_used"
    return "unknown"


def _contains_key(value: Any, key: str) -> bool:
    if isinstance(value, dict):
        return key in value or any(_contains_key(child, key) for child in value.values())
    if isinstance(value, list):
        return any(_contains_key(child, key) for child in value)
    return False


def _properties_are_shallow(properties: Any) -> bool | None:
    if not isinstance(properties, dict):
        return None
    for value in properties.values():
        if isinstance(value, dict):
            return False
        if isinstance(value, list):
            if len(value) != 3 or not all(isinstance(item, (int, float)) for item in value):
                return False
            continue
        if value is not None and not isinstance(value, (bool, int, float, str)):
            return False
    return True


def summarize_handoff_response(response_json: dict[str, Any], http_status: int | None = None) -> dict[str, Any]:
    """Return a JSON-safe handoff summary without full WorldConfig or EpisodeSpec payloads."""
    response = _as_dict(response_json)
    metadata = _as_dict(response.get("metadata"))
    validation = _as_dict(response.get("validation"))
    world_config = _as_dict(response.get("worldConfig"))
    episode_spec = _as_dict(response.get("episodeSpec"))
    episode_setup = _as_dict(response.get("episodeSetup"))
    delivery_bot_setup = _as_dict(response.get("deliveryBotSetup"))
    scenario_reflection = _as_dict(response.get("scenarioReflection"))
    post_processing = _as_dict(response.get("postProcessing"))
    episode_reflection = _as_dict(response.get("episodeScenarioReflection"))
    diagnostics = _as_dict(response.get("diagnostics"))
    environment_sampling = _as_dict(diagnostics.get("environmentSampling"))
    environment_parameters = _as_dict(environment_sampling.get("parameters"))
    generation_trace = _as_dict(diagnostics.get("generationTrace"))
    setup_pair_trace = _as_list(diagnostics.get("setupPairTrace"))
    trace_summary = _as_dict(generation_trace.get("summary"))
    trace_source_types = _trace_source_types(generation_trace)

    map_config = _as_dict(world_config.get("map"))
    obstacles = _as_list(world_config.get("obstacles"))
    obstacle = _first_dict(obstacles)
    pedestrians = _as_list(world_config.get("pedestrians"))

    actors = _as_dict(episode_spec.get("actors"))
    setup_actors = _as_dict(episode_setup.get("actors"))
    setup_static_obstacles = _as_list(setup_actors.get("static_obstacles"))
    setup_ground_model = _as_dict(episode_setup.get("ground_model"))
    setup_regions = _as_list(setup_ground_model.get("regions"))
    setup_first_region = _first_dict(setup_regions)
    setup_first_shape = _as_dict(setup_first_region.get("shape"))
    setup_size_m = setup_first_shape.get("size_m")
    setup_sidewalk_width_m = None
    if isinstance(setup_size_m, list) and len(setup_size_m) >= 2 and isinstance(setup_size_m[1], (int, float)):
        setup_sidewalk_width_m = float(setup_size_m[1])
    delivery_robot = _as_dict(delivery_bot_setup.get("robot"))
    delivery_lidar = _as_dict(delivery_robot.get("lidar"))
    static_obstacles = _as_list(actors.get("static_obstacles"))
    static_obstacle = _first_dict(static_obstacles)
    static_obstacle_properties = _as_dict(static_obstacle.get("properties"))
    static_obstacle_transform = _as_dict(static_obstacle.get("transform"))
    static_obstacle_location = static_obstacle_transform.get("location_m")
    ground_model = _as_dict(episode_spec.get("ground_model"))
    regions = _as_list(ground_model.get("regions"))
    first_region = _first_dict(regions)
    first_region_shape = _as_dict(first_region.get("shape"))
    size_m = first_region_shape.get("size_m")
    episode_pedestrians = _as_list(actors.get("pedestrians"))
    episode_paths = _as_list(episode_spec.get("paths"))
    checked_requirements = _as_list(scenario_reflection.get("checkedRequirements"))
    route_midpoint_expected = any(
        isinstance(requirement, dict) and requirement.get("requirementId") == "obstacle_route_midpoint"
        for requirement in checked_requirements
    )
    obstacle_distance_from_midpoint = None
    obstacle_near_route_midpoint = None
    if episode_spec:
        robot = _as_dict(actors.get("robot"))
        transform = _as_dict(robot.get("transform"))
        route = _as_dict(robot.get("route"))
        spawn_m = transform.get("location_m")
        goal_m = route.get("goal_m")
        obstacle_location = static_obstacle_location
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
    elif world_config:
        robot = _as_dict(world_config.get("robot"))
        midpoint = compute_midpoint(_as_dict(robot.get("spawn")), _as_dict(robot.get("goal")))
        if isinstance(obstacle.get("position"), dict):
            obstacle_distance_from_midpoint = round(distance_between_points(obstacle["position"], midpoint), 3)
            obstacle_near_route_midpoint = obstacle_distance_from_midpoint <= 50.0
    sidewalk_width_m = None
    if isinstance(size_m, list) and len(size_m) >= 2 and isinstance(size_m[1], (int, float)):
        sidewalk_width_m = float(size_m[1])

    summary = {
        "httpStatus": http_status,
        "success": _bool_or_none(response.get("success")),
        "handoffTarget": response.get("handoffTarget"),
        "effectiveResponseFormat": _effective_response_format(response),
        "providerUsed": metadata.get("provider"),
        "model": metadata.get("model"),
        "schema": episode_spec.get("schema"),
        "units": episode_spec.get("units"),
        "worldConfigExists": bool(world_config),
        "episodeSpecExists": bool(episode_spec),
        "episodeSetupExists": bool(episode_setup),
        "deliveryBotSetupExists": bool(delivery_bot_setup),
        "schemaValidationPassed": _bool_or_none(validation.get("schemaValidationPassed")),
        "scenarioReflectionPassed": _bool_or_none(validation.get("scenarioReflectionPassed")),
        "contractValidationPassed": _bool_or_none(validation.get("contractValidationPassed")),
        "episodeValidationPassed": _validation_passed(response.get("episodeValidation")),
        "episodeScenarioReflectionPassed": _bool_or_none(episode_reflection.get("passed")),
        "episodeSetupValidationPassed": _validation_passed(response.get("episodeSetupValidation")),
        "deliveryBotSetupValidationPassed": _validation_passed(response.get("deliveryBotSetupValidation")),
        "sidewalkWidthCm": map_config.get("sidewalkWidthCm"),
        "obstacleExists": bool(obstacle),
        "obstacleType": obstacle.get("type"),
        "obstaclePosition": obstacle.get("position"),
        "blockingRatio": obstacle.get("blockingRatio"),
        "pedestriansEmpty": bool(world_config) and not pedestrians,
        "pathsEmpty": bool(episode_spec) and not episode_paths,
        "staticObstacleCount": len(static_obstacles),
        "staticObstaclePropId": static_obstacle.get("prop_id"),
        "staticObstacleSemanticType": static_obstacle_properties.get("semantic_type"),
        "staticObstacleBlockingRatio": static_obstacle_properties.get("blocking_ratio"),
        "obstacleLocation": static_obstacle_location,
        "sidewalkWidthM": sidewalk_width_m,
        "episodeSetupSidewalkWidthM": setup_sidewalk_width_m,
        "episodeSetupStaticObstacleCount": len(setup_static_obstacles),
        "deliveryBotStopDistanceM": delivery_lidar.get("stop_distance_m"),
        "deliveryBotSlowDownDistanceM": delivery_lidar.get("slow_down_distance_m"),
        "runTimeLimitS": _as_dict(episode_spec.get("run")).get("time_limit_s"),
        "pedestrianCount": len(episode_pedestrians),
        "pathCount": len(episode_paths),
        "ueCompilerReadiness": _bool_or_none(episode_reflection.get("ueCompilerReadiness")),
        "checkedRequirementsCount": len(checked_requirements),
        "postProcessingApplied": _bool_or_none(post_processing.get("applied")),
        "appliedPatches": _applied_patches(response),
        "routeMidpointExpected": route_midpoint_expected,
        "obstacleNearRouteMidpoint": obstacle_near_route_midpoint,
        "obstacleDistanceFromMidpoint": obstacle_distance_from_midpoint,
        "penaltiesFieldAbsent": not _contains_key(episode_spec, "penalties"),
        "propertiesAreShallow": _properties_are_shallow(static_obstacle_properties),
        "propIdInCatalog": (
            static_obstacle.get("prop_id") in ALLOWED_STATIC_PROP_IDS
            if static_obstacle.get("prop_id") is not None
            else None
        ),
        "warnings": _warnings(response),
        "errorCode": _as_dict(response.get("error")).get("code"),
        "environmentSamplingEnabled": environment_sampling.get("enabled"),
        "environmentSamplingSeed": environment_sampling.get("seed"),
        "environmentSamplingScenarioType": environment_sampling.get("scenarioType"),
        "sampledSidewalkWidthCm": environment_parameters.get("sidewalkWidthCm"),
        "sampledPedestrianCount": environment_parameters.get("pedestrianCount"),
        "sampledObstacleBlockingRatio": environment_parameters.get("obstacleBlockingRatio"),
        "sampledTimeLimitSec": environment_parameters.get("timeLimitSec"),
        "generationTraceExists": bool(generation_trace),
        "setupPairTraceExists": bool(setup_pair_trace),
        "traceItemCount": len(_as_list(generation_trace.get("evidenceItems"))),
        "traceSourceTypes": trace_source_types,
        "coordinateSource": trace_summary.get("coordinateSource") or _coordinate_source(trace_source_types),
        "policyRagUsedFor": _policy_rag_used_for(generation_trace) if generation_trace else "unknown",
        "traceStatus": trace_summary.get("status") or ("unknown" if response.get("success") is False and generation_trace else None),
        "traceFailureStage": trace_summary.get("failureStage"),
        "generationTraceError": diagnostics.get("generationTraceError"),
        "missingFailureStage": bool(
            response.get("success") is False
            and bool(generation_trace)
            and not trace_summary.get("failureStage")
            and not diagnostics.get("failureStage")
        ),
        "episodeSpecMissingReason": (
            _episode_spec_missing_reason(response, diagnostics)
            if not bool(episode_spec)
            else None
        ),
        "apiKeyStored": False,
        "fullPayloadStored": False,
        "fullEpisodeSpecStored": False,
    }
    return summary
