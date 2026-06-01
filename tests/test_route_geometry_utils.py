from __future__ import annotations

from app.services.route_geometry_utils import compute_midpoint, interpolate_route_point, is_point_near_route_midpoint


def test_compute_midpoint_between_spawn_and_goal_in_cm() -> None:
    start = {"x": 0, "y": 0, "z": 0}
    goal = {"x": 800, "y": 0, "z": 0}

    assert compute_midpoint(start, goal) == {"x": 400.0, "y": 0.0, "z": 0.0}
    assert interpolate_route_point(start, goal, 0.5) == {"x": 400.0, "y": 0.0, "z": 0.0}
    assert is_point_near_route_midpoint({"x": 420, "y": 0, "z": 0}, start, goal)
    assert not is_point_near_route_midpoint({"x": 800, "y": 0, "z": 0}, start, goal)
