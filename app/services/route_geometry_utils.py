from __future__ import annotations

import math
from typing import Any


def _coordinate(value: dict[str, Any], key: str) -> float:
    raw = value.get(key, 0.0) if isinstance(value, dict) else 0.0
    return float(raw or 0.0)


def interpolate_route_point(start: dict[str, Any], goal: dict[str, Any], ratio: float) -> dict[str, float]:
    clamped = max(0.0, min(1.0, float(ratio)))
    return {
        "x": round(_coordinate(start, "x") + (_coordinate(goal, "x") - _coordinate(start, "x")) * clamped, 3),
        "y": round(_coordinate(start, "y") + (_coordinate(goal, "y") - _coordinate(start, "y")) * clamped, 3),
        "z": round(_coordinate(start, "z") + (_coordinate(goal, "z") - _coordinate(start, "z")) * clamped, 3),
    }


def compute_midpoint(start: dict[str, Any], goal: dict[str, Any]) -> dict[str, float]:
    return interpolate_route_point(start, goal, 0.5)


def distance_between_points(left: dict[str, Any], right: dict[str, Any]) -> float:
    return math.sqrt(
        (_coordinate(left, "x") - _coordinate(right, "x")) ** 2
        + (_coordinate(left, "y") - _coordinate(right, "y")) ** 2
        + (_coordinate(left, "z") - _coordinate(right, "z")) ** 2
    )


def is_point_near_route_midpoint(
    point: dict[str, Any],
    start: dict[str, Any],
    goal: dict[str, Any],
    tolerance_cm: float = 50.0,
) -> bool:
    return distance_between_points(point, compute_midpoint(start, goal)) <= float(tolerance_cm)
