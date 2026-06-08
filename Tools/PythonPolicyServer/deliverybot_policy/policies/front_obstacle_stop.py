from __future__ import annotations

from typing import Any

from deliverybot_policy.actions import make_policy_candidate, make_stop_action
from deliverybot_policy.context import (
    get_float_field,
    get_lidar_spec,
    get_nearest_observed_object,
    get_policy_priority,
)


POLICY_ID = "front_obstacle_stop"


def evaluate(context: dict[str, Any]) -> dict[str, Any] | None:
    observation = context.get("observation", {})
    if not isinstance(observation, dict):
        return None

    nearest_object = get_nearest_observed_object(observation, require_in_front=True)
    if nearest_object is None:
        return None

    lidar_spec = get_lidar_spec(context)
    stop_distance_m = get_float_field(lidar_spec, "stopDistanceM", 1.5)
    distance_m = float(nearest_object.get("closestDistanceM", 0.0) or 0.0)

    if distance_m > stop_distance_m:
        return None

    actor_tags = nearest_object.get("actorTags", [])
    safe_actor_tags = actor_tags if isinstance(actor_tags, list) else []

    return make_policy_candidate(
        POLICY_ID,
        make_stop_action(),
        "front_object_inside_stop_distance",
        get_policy_priority(context, 10),
        {
            "nearestObjectActor": str(nearest_object.get("actorName", "")),
            "nearestObjectTags": [str(tag) for tag in safe_actor_tags],
            "nearestObjectDistanceM": distance_m,
            "stopDistanceM": stop_distance_m,
        },
    )
