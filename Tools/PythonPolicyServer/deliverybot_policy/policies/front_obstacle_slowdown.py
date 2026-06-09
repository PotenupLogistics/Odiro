from __future__ import annotations

from typing import Any

from deliverybot_policy.context import (
    get_float_field,
    get_lidar_spec,
    get_motion_control_spec,
    get_nearest_observed_object,
    get_policy_priority,
)
from deliverybot_policy.dynamic_obstacles import build_dynamic_obstacle_reroute_context
from deliverybot_policy.policies.normal_path_follow import build_path_follow_candidate


POLICY_ID = "front_obstacle_slowdown"


def evaluate(context: dict[str, Any]) -> dict[str, Any] | None:
    observation = context.get("observation", {})
    if not isinstance(observation, dict):
        return None

    lidar_spec = get_lidar_spec(context)
    slow_down_distance_m = get_float_field(lidar_spec, "slowDownDistanceM", 5.0)
    nearest_object = get_nearest_observed_object(observation, require_in_front=True)
    nearest_lidar_hit = get_nearest_lidar_hit(observation, slow_down_distance_m)
    distance_m = float(nearest_object.get("closestDistanceM", 0.0) or 0.0) if nearest_object is not None else 0.0

    if nearest_object is None and nearest_lidar_hit is None:
        return None

    if nearest_object is not None and distance_m > slow_down_distance_m and nearest_lidar_hit is None:
        return None

    motion_spec = get_motion_control_spec(context)
    slow_speed_kmh = get_float_field(motion_spec, "obstacleSlowSpeedKmh", 0.5)
    reroute_context = build_dynamic_obstacle_reroute_context(context, slow_down_distance_m)
    dynamic_debug = reroute_context.pop("dynamicObstacleDebug", {})
    reason = "front_object_dynamic_obstacle_reroute"
    if isinstance(dynamic_debug, dict) and int(dynamic_debug.get("dynamicObstacleBlockedCellCount", 0) or 0) <= 0:
        reason = "front_object_inside_slowdown_distance"

    candidate = build_path_follow_candidate(
        reroute_context,
        POLICY_ID,
        reason,
        speed_limit_kmh=slow_speed_kmh,
    )
    candidate["priority"] = get_policy_priority(context, 30)
    candidate_debug = candidate.setdefault("debug", {})
    if isinstance(candidate_debug, dict):
        actor_tags = nearest_object.get("actorTags", []) if nearest_object is not None else []
        safe_actor_tags = actor_tags if isinstance(actor_tags, list) else []
        candidate_debug.update(
            {
                "nearestObjectActor": str(nearest_object.get("actorName", "")) if nearest_object is not None else "",
                "nearestObjectTags": [str(tag) for tag in safe_actor_tags],
                "nearestObjectDistanceM": distance_m,
                "nearestLidarHitDistanceM": nearest_lidar_hit["distanceM"] if nearest_lidar_hit else None,
                "nearestLidarHitYawDegree": nearest_lidar_hit["rayYawDegree"] if nearest_lidar_hit else None,
                "slowDownDistanceM": slow_down_distance_m,
                "obstacleSlowSpeedKmh": slow_speed_kmh,
            }
        )
        if isinstance(dynamic_debug, dict):
            candidate_debug.update(dynamic_debug)

    return candidate


def get_nearest_lidar_hit(observation: dict[str, Any], max_distance_m: float) -> dict[str, float] | None:
    rays = observation.get("lidarRays", [])
    if not isinstance(rays, list):
        return None

    nearest_hit: dict[str, float] | None = None
    nearest_distance_m = max_distance_m
    for ray in rays:
        if not isinstance(ray, dict) or not bool(ray.get("hit", False)):
            continue

        distance_m = get_float_field(ray, "distanceM", 0.0)
        if distance_m <= 0.0 or distance_m > nearest_distance_m:
            continue

        nearest_distance_m = distance_m
        nearest_hit = {
            "distanceM": distance_m,
            "rayYawDegree": get_float_field(ray, "rayYawDegree", 0.0),
        }

    return nearest_hit
