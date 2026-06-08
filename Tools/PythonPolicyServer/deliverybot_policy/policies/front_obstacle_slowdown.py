from __future__ import annotations

from typing import Any

from deliverybot_policy.context import (
    get_float_field,
    get_lidar_spec,
    get_motion_control_spec,
    get_nearest_observed_object,
    get_policy_priority,
)
from deliverybot_policy.policies.normal_path_follow import build_path_follow_candidate


POLICY_ID = "front_obstacle_slowdown"


def evaluate(context: dict[str, Any]) -> dict[str, Any] | None:
    observation = context.get("observation", {})
    if not isinstance(observation, dict):
        return None

    nearest_object = get_nearest_observed_object(observation, require_in_front=True)
    if nearest_object is None:
        return None

    lidar_spec = get_lidar_spec(context)
    slow_down_distance_m = get_float_field(lidar_spec, "slowDownDistanceM", 5.0)
    distance_m = float(nearest_object.get("closestDistanceM", 0.0) or 0.0)

    if distance_m > slow_down_distance_m:
        return None

    motion_spec = get_motion_control_spec(context)
    slow_speed_kmh = get_float_field(motion_spec, "obstacleSlowSpeedKmh", 0.5)
    candidate = build_path_follow_candidate(
        context,
        POLICY_ID,
        "front_object_inside_slowdown_distance",
        speed_limit_kmh=slow_speed_kmh,
    )
    candidate["priority"] = get_policy_priority(context, 30)
    candidate_debug = candidate.setdefault("debug", {})
    if isinstance(candidate_debug, dict):
        actor_tags = nearest_object.get("actorTags", [])
        safe_actor_tags = actor_tags if isinstance(actor_tags, list) else []
        candidate_debug.update(
            {
                "nearestObjectActor": str(nearest_object.get("actorName", "")),
                "nearestObjectTags": [str(tag) for tag in safe_actor_tags],
                "nearestObjectDistanceM": distance_m,
                "slowDownDistanceM": slow_down_distance_m,
                "obstacleSlowSpeedKmh": slow_speed_kmh,
            }
        )

    return candidate
