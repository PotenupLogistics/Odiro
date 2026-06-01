from __future__ import annotations

from typing import Any

from app.models.episode_spec import (
    EpisodeScenarioReflectionIssue,
    EpisodeScenarioReflectionResult,
)


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
    return {
        "expectsKickboard": _contains_any(prompt, ["킥보드", "kickboard", "공유 킥보드"]),
        "expectsBlocking": _contains_any(prompt, ["막", "차단", "blocking", "blocked", "경로를 막"]),
        "expectsPedestrian": _contains_any(prompt, ["보행자", "pedestrian"]),
        "expectsCrossing": _contains_any(prompt, ["횡단", "crossing", "cross"]),
        "expectsNarrowSidewalk": _contains_any(prompt, ["좁은 보도", "narrow sidewalk", "좁은"]),
    }


def validate_episode_spec_scenario_reflection(
    prompt: str,
    episode_spec: dict[str, Any],
) -> EpisodeScenarioReflectionResult:
    flags = _scenario_flags(prompt)
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
    has_blocking_ratio = any(
        isinstance((obstacle.get("properties") or {}).get("blocking_ratio"), (int, float))
        and float((obstacle.get("properties") or {}).get("blocking_ratio")) > 0
        for obstacle in static_obstacles
    )
    pedestrian_path_linked = all(
        pedestrian.get("path_id") in path_by_id
        and len(path_by_id[pedestrian.get("path_id")].get("points_m") or []) >= 2
        for pedestrian in pedestrians
    ) if pedestrians else False
    has_crossing = any(
        (pedestrian.get("properties") or {}).get("semantic_behavior") == "Crossing"
        or (path_by_id.get(pedestrian.get("path_id")) or {}).get("role") == "pedestrian_crossing"
        for pedestrian in pedestrians
    )

    issues: list[EpisodeScenarioReflectionIssue] = []
    if flags["expectsKickboard"] and not static_obstacles:
        issues.append(_issue("missing_static_obstacle", "actors.static_obstacles", "at least one obstacle", "0", "Prompt expects a kickboard/obstacle but EpisodeSpec has no static obstacles."))
    if flags["expectsKickboard"] and static_obstacles and not has_kickboard:
        issues.append(_issue("missing_kickboard_semantic", "actors.static_obstacles[].properties.semantic_type", "Kickboard", str([obs.get("properties") for obs in static_obstacles]), "Prompt expects a kickboard but no Kickboard semantic marker is present."))
    if flags["expectsBlocking"] and not has_blocking_ratio:
        issues.append(_issue("missing_blocking_ratio", "actors.static_obstacles[].properties.blocking_ratio", "number > 0", str([obs.get("properties") for obs in static_obstacles]), "Prompt expects path blocking but no positive blocking_ratio is present."))
    if flags["expectsPedestrian"] and not pedestrians:
        issues.append(_issue("missing_pedestrian_actor", "actors.pedestrians", "at least one pedestrian", "0", "Prompt expects a pedestrian but EpisodeSpec has no pedestrian actors."))
    if flags["expectsCrossing"] and not paths:
        issues.append(_issue("missing_pedestrian_path", "paths", "at least one path", "0", "Prompt expects crossing but EpisodeSpec has no pedestrian paths."))
    if pedestrians and not pedestrian_path_linked:
        issues.append(_issue("pedestrian_path_not_linked", "actors.pedestrians[].path_id", "existing paths[].path_id", str([ped.get("path_id") for ped in pedestrians]), "Pedestrian path_id must reference an existing path with at least two points."))
    if flags["expectsCrossing"] and pedestrians and not has_crossing:
        issues.append(_issue("missing_pedestrian_crossing_semantic", "actors.pedestrians[].properties.semantic_behavior or paths[].role", "Crossing or pedestrian_crossing", str([ped.get("properties") for ped in pedestrians]), "Prompt expects crossing but no crossing semantic/path role is present."))
    if flags["expectsNarrowSidewalk"] and sidewalk_width_m is not None and sidewalk_width_m > 2.0:
        issues.append(_issue("weak_episode_scenario_binding", "ground_model.regions[].shape.size_m[1]", "<= 2.0", str(sidewalk_width_m), "Prompt expects a narrow sidewalk but EpisodeSpec sidewalk width is not narrow."))

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
