from __future__ import annotations

from typing import Any

from app.models.episode_spec import (
    EpisodeScenarioReflectionIssue,
    EpisodeScenarioReflectionResult,
)
from app.services.route_geometry_utils import compute_midpoint, distance_between_points


def _contains_any(text: str, values: list[str]) -> bool:
    lowered = text.lower()
    return any(value.lower() in lowered for value in values)


def _issue(
    issue_type: str,
    expected_path: str,
    expected_value_hint: str,
    actual_value_summary: str,
    message: str,
) -> EpisodeScenarioReflectionIssue:
    return EpisodeScenarioReflectionIssue(
        issueType=issue_type,
        expectedPath=expected_path,
        expectedValueHint=expected_value_hint,
        actualValueSummary=actual_value_summary,
        message=message,
    )


def _scenario_flags(prompt: str) -> dict[str, bool]:
    no_pedestrian = _contains_any(
        prompt,
        ["보행자는 없는", "보행자 없는", "보행자 없음", "보행자는 없음", "보행자는 없", "보행자가 없는"],
    )
    return {
        "expectsKickboard": _contains_any(prompt, ["킥보드", "kickboard", "공유 킥보드"]),
        "expectsGenericObstacle": _contains_any(prompt, ["장애물", "정적 장애물", "static obstacle", "obstacle"]),
        "expectsBlocking": _contains_any(prompt, ["막", "차단", "blocking", "blocked", "경로를 막"]),
        "expectsPedestrian": _contains_any(prompt, ["보행자", "pedestrian"]) and not no_pedestrian,
        "expectsNoPedestrian": no_pedestrian,
        "expectsCrossing": _contains_any(prompt, ["횡단", "crossing", "cross"]) and not no_pedestrian,
        "expectsNarrowSidewalk": _contains_any(prompt, ["좁은 보도", "narrow sidewalk", "좁은"]),
        "expectsRouteMidpoint": _contains_any(
            prompt,
            ["경로 중앙", "경로 중간", "로봇 경로 중앙", "로봇 경로 중간", "중앙 근처", "경로 중앙 근처", "path center", "middle of the path"],
        ),
    }


def _environment_parameters(environment_sampling: dict[str, Any] | None) -> dict[str, Any]:
    if not isinstance(environment_sampling, dict):
        return {}
    parameters = environment_sampling.get("parameters")
    if isinstance(parameters, dict):
        return parameters
    return environment_sampling


def _close(left: float | None, right: float | None, tolerance: float = 0.001) -> bool:
    if left is None or right is None:
        return False
    return abs(float(left) - float(right)) <= tolerance


def validate_episode_spec_scenario_reflection(
    prompt: str,
    episode_spec: dict[str, Any],
    environment_sampling: dict[str, Any] | None = None,
) -> EpisodeScenarioReflectionResult:
    flags = _scenario_flags(prompt)
    sampled_parameters = _environment_parameters(environment_sampling)
    sampled_width_cm = sampled_parameters.get("sidewalkWidthCm")
    sampled_blocking_ratio = sampled_parameters.get("obstacleBlockingRatio")
    if isinstance(sampled_blocking_ratio, (int, float)) and float(sampled_blocking_ratio) > 0:
        flags["expectsGenericObstacle"] = True
        flags["expectsBlocking"] = True
    actors = episode_spec.get("actors") or {}
    static_obstacles = actors.get("static_obstacles") or []
    pedestrians = actors.get("pedestrians") or []
    paths = episode_spec.get("paths") or []
    path_by_id = {path.get("path_id"): path for path in paths}
    regions = (episode_spec.get("ground_model") or {}).get("regions") or []
    sidewalk_width_m = None
    if regions:
        size_m = ((regions[0].get("shape") or {}).get("size_m") or [])
        if len(size_m) >= 2:
            sidewalk_width_m = float(size_m[1])

    has_kickboard = any(
        obstacle.get("prop_id") == "obstacle.kickboard"
        or (
            obstacle.get("prop_id") == "obstacle.road_barrier_01"
            and (obstacle.get("properties") or {}).get("semantic_type") == "Kickboard"
        )
        or (obstacle.get("properties") or {}).get("semantic_type") == "Kickboard"
        for obstacle in static_obstacles
    )
    has_generic_obstacle = any(
        (obstacle.get("properties") or {}).get("semantic_type") in {"Obstacle", "Kickboard"}
        or bool(obstacle.get("prop_id"))
        for obstacle in static_obstacles
    )
    has_blocking_ratio = any(
        isinstance((obstacle.get("properties") or {}).get("blocking_ratio"), (int, float))
        and float((obstacle.get("properties") or {}).get("blocking_ratio")) > 0
        for obstacle in static_obstacles
    )
    pedestrian_path_linked = all(
        pedestrian.get("path_id") in path_by_id
        and len(path_by_id[pedestrian.get("path_id")].get("points_m") or []) >= 2
        for pedestrian in pedestrians
    ) if pedestrians else flags["expectsNoPedestrian"]
    has_crossing = any(
        (pedestrian.get("properties") or {}).get("semantic_behavior") == "Crossing"
        or (path_by_id.get(pedestrian.get("path_id")) or {}).get("role") == "pedestrian_crossing"
        for pedestrian in pedestrians
    )
    route_midpoint_m = None
    obstacle_distance_from_midpoint_m = None
    robot = actors.get("robot") or {}
    robot_transform = robot.get("transform") or {}
    route = robot.get("route") or {}
    spawn_m = robot_transform.get("location_m")
    goal_m = route.get("goal_m")
    if isinstance(spawn_m, list) and len(spawn_m) >= 3 and isinstance(goal_m, list) and len(goal_m) >= 3:
        route_midpoint_m = compute_midpoint(
            {"x": spawn_m[0], "y": spawn_m[1], "z": spawn_m[2]},
            {"x": goal_m[0], "y": goal_m[1], "z": goal_m[2]},
        )
    if route_midpoint_m is not None and static_obstacles:
        obstacle_location = ((static_obstacles[0].get("transform") or {}).get("location_m") or [])
        if isinstance(obstacle_location, list) and len(obstacle_location) >= 3:
            obstacle_distance_from_midpoint_m = distance_between_points(
                {"x": obstacle_location[0], "y": obstacle_location[1], "z": obstacle_location[2]},
                route_midpoint_m,
            )

    issues: list[EpisodeScenarioReflectionIssue] = []
    if flags["expectsKickboard"] and not static_obstacles:
        issues.append(_issue("missing_static_obstacle", "actors.static_obstacles", "at least one obstacle", "0", "Prompt expects a kickboard/obstacle but EpisodeSpec has no static obstacles."))
    if flags["expectsGenericObstacle"] and not static_obstacles:
        issues.append(_issue("missing_static_obstacle", "actors.static_obstacles", "at least one obstacle", "0", "Prompt expects an obstacle but EpisodeSpec has no static obstacles."))
    if flags["expectsGenericObstacle"] and static_obstacles and not has_generic_obstacle:
        issues.append(_issue("missing_obstacle_semantic", "actors.static_obstacles[].properties.semantic_type", "Obstacle or Kickboard", str([obs.get("properties") for obs in static_obstacles]), "Prompt expects an obstacle but no obstacle semantic marker is present."))
    if flags["expectsKickboard"] and static_obstacles and not has_kickboard:
        issues.append(_issue("missing_kickboard_semantic", "actors.static_obstacles[].properties.semantic_type", "Kickboard", str([obs.get("properties") for obs in static_obstacles]), "Prompt expects a kickboard but no Kickboard semantic marker is present."))
    if flags["expectsBlocking"] and not has_blocking_ratio:
        issues.append(_issue("missing_blocking_ratio", "actors.static_obstacles[].properties.blocking_ratio", "number > 0", str([obs.get("properties") for obs in static_obstacles]), "Prompt expects path blocking but no positive blocking_ratio is present."))
    if flags["expectsRouteMidpoint"] and route_midpoint_m is not None:
        if obstacle_distance_from_midpoint_m is None or obstacle_distance_from_midpoint_m > 0.5:
            issues.append(_issue("obstacle_not_near_route_midpoint", "actors.static_obstacles[].transform.location_m", f"near {route_midpoint_m}", str(obstacle_distance_from_midpoint_m), "Prompt expects the obstacle near the route midpoint."))
    if isinstance(sampled_blocking_ratio, (int, float)) and static_obstacles:
        expected_ratio = float(sampled_blocking_ratio)
        has_expected_ratio = any(
            _close(float((obstacle.get("properties") or {}).get("blocking_ratio", -1)), expected_ratio)
            for obstacle in static_obstacles
            if isinstance((obstacle.get("properties") or {}).get("blocking_ratio"), (int, float))
        )
        if not has_expected_ratio:
            issues.append(_issue("environment_blocking_ratio_mismatch", "actors.static_obstacles[].properties.blocking_ratio", f"{expected_ratio:g}", str([obs.get("properties") for obs in static_obstacles]), "Environment sampler blockingRatio must be preserved in EpisodeSpec."))
    if flags["expectsPedestrian"] and not pedestrians:
        issues.append(_issue("missing_pedestrian_actor", "actors.pedestrians", "at least one pedestrian", "0", "Prompt expects a pedestrian but EpisodeSpec has no pedestrian actors."))
    if flags["expectsNoPedestrian"] and pedestrians:
        issues.append(_issue("unexpected_pedestrian_actor", "actors.pedestrians", "empty list", str(len(pedestrians)), "Prompt explicitly says there should be no pedestrians."))
    if flags["expectsCrossing"] and not paths:
        issues.append(_issue("missing_pedestrian_path", "paths", "at least one path", "0", "Prompt expects crossing but EpisodeSpec has no pedestrian paths."))
    if pedestrians and not pedestrian_path_linked:
        issues.append(_issue("pedestrian_path_not_linked", "actors.pedestrians[].path_id", "existing paths[].path_id", str([ped.get("path_id") for ped in pedestrians]), "Pedestrian path_id must reference an existing path with at least two points."))
    if flags["expectsCrossing"] and pedestrians and not has_crossing:
        issues.append(_issue("missing_pedestrian_crossing_semantic", "actors.pedestrians[].properties.semantic_behavior or paths[].role", "Crossing or pedestrian_crossing", str([ped.get("properties") for ped in pedestrians]), "Prompt expects crossing but no crossing semantic/path role is present."))
    if flags["expectsNarrowSidewalk"] and sidewalk_width_m is not None and sidewalk_width_m > 2.0:
        issues.append(_issue("weak_episode_scenario_binding", "ground_model.regions[].shape.size_m[1]", "<= 2.0", str(sidewalk_width_m), "Prompt expects a narrow sidewalk but EpisodeSpec sidewalk width is not narrow."))
    if isinstance(sampled_width_cm, (int, float)):
        expected_width_m = float(sampled_width_cm) / 100.0
        if not _close(sidewalk_width_m, expected_width_m):
            issues.append(_issue("environment_sidewalk_width_mismatch", "ground_model.regions[].shape.size_m[1]", f"{expected_width_m:g}", str(sidewalk_width_m), "Environment sampler sidewalkWidthCm must be preserved in EpisodeSpec ground model."))

    ue_ready = bool(
        episode_spec.get("schema")
        and episode_spec.get("units", {}).get("distance") == "m"
        and episode_spec.get("units", {}).get("angle") == "deg"
        and regions
        and actors.get("robot")
        and not issues
    )
    return EpisodeScenarioReflectionResult(
        passed=not issues,
        issues=issues,
        staticObstacleCount=len(static_obstacles),
        hasKickboardSemantic=has_kickboard,
        hasBlockingRatio=has_blocking_ratio,
        pedestrianCount=len(pedestrians),
        pathCount=len(paths),
        pedestrianPathLinked=pedestrian_path_linked,
        hasCrossingPedestrian=has_crossing,
        sidewalkWidthM=sidewalk_width_m,
        ueCompilerReadiness=ue_ready,
    )
