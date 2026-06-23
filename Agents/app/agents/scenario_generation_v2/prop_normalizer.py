from __future__ import annotations

from typing import Any


LEGACY_STATIC_OBSTACLE_PROP_ALIASES = {
    "obstacle.crate_01": "obstacle.box_01",
    "crate_01": "obstacle.box_01",
    "traffic_cone_01": "obstacle.road_cone_01",
    "obstacle.traffic_cone_01": "obstacle.road_cone_01",
}
"""Legacy scenario preset and LLM prop ids mapped to catalog-owned prop ids."""


def normalize_legacy_static_obstacle_prop_id(prop_id: Any) -> Any:
    """Return the canonical catalog prop id for known legacy values."""
    if not isinstance(prop_id, str):
        return prop_id
    return LEGACY_STATIC_OBSTACLE_PROP_ALIASES.get(prop_id, prop_id)
