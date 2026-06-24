from __future__ import annotations

from copy import deepcopy

from app.agents.scenario_generation_v2.repair_handler import ROBOT_ANCHOR_EXCLUSION_RADIUS_M, RepairHandler
from app.agents.scenario_generation_v2.template_validator import TemplateValidator


def _base_quality_scenario() -> dict:
    """Return a valid corridor scenario fixture for quality repair tests."""
    return {
        "schema": "scenario",
        "version": 1,
        "scenario_id": "quality_repair_fixture",
        "intent": "Validate safety repair for generated scenario obstacles.",
        "corridor": {
            "axis": {"type": "polyline", "points_m": [[0.0, 0.0], [10.0, 0.0]]},
            "walkway_width_m": 1.4,
            "building_side": [{"surface": "wall", "width_m": 0.3}],
            "curb_side": [{"surface": "road", "width_m": 4.0}],
            "segments": [{"id": "main", "type": "straight", "along_range_m": [0.0, 10.0]}],
        },
        "obstacles": {"min_clear_width_m": 0.9, "placements": []},
        "pedestrians": {"background": {"count": 0, "speed_mps": 1.0}, "encounters": []},
        "robot": {
            "start": {
                "type": "corridor_pose",
                "segment": "main",
                "along_m": 1.0,
                "offset_m": 0.0,
                "lane": "walkway",
                "heading": "forward",
            },
            "goal": {
                "type": "corridor_pose",
                "segment": "main",
                "along_m": 9.0,
                "offset_m": 0.0,
                "lane": "walkway",
                "heading": "forward",
            },
        },
    }


def _cone(placement_id: str, *, along_m: object, offset_m: object) -> dict:
    """Return one catalog-safe cone placement anchored in the main segment."""
    return {
        "kind": "fixed",
        "id": placement_id,
        "prop": "obstacle.road_cone_01",
        "at": {"segment": "main", "along_m": along_m, "offset_m": offset_m, "lane": "walkway"},
        "yaw_deg": 0,
    }


def _bounds(value: object) -> tuple[float, float]:
    """Return numeric bounds for scalar or min/max values used in tests."""
    if isinstance(value, dict):
        return float(value["min"]), float(value["max"])
    numeric = float(value)
    return numeric, numeric


def _overlaps(left: tuple[float, float], right: tuple[float, float]) -> bool:
    """Return whether two closed numeric intervals overlap."""
    return left[0] <= right[1] and right[0] <= left[1]


def _max_clear_width(walkway_width_m: float, offset_m: object) -> float:
    """Return the largest left/right passable width around one obstacle footprint."""
    offset_min, offset_max = _bounds(offset_m)
    half_width = walkway_width_m / 2.0
    left_clear = offset_min + half_width
    right_clear = half_width - offset_max
    return max(left_clear, right_clear)


def _assert_no_anchor_conflicts(scenario: dict, *, radius_m: float = ROBOT_ANCHOR_EXCLUSION_RADIUS_M) -> None:
    """Assert fixed obstacles stay outside start/goal along safety bands."""
    anchors = [scenario["robot"]["start"], scenario["robot"]["goal"]]
    for placement in scenario["obstacles"]["placements"]:
        at = placement["at"]
        along_bounds = _bounds(at["along_m"])
        for anchor in anchors:
            if at["segment"] != anchor["segment"]:
                continue
            anchor_along = float(anchor["along_m"])
            forbidden = (anchor_along - radius_m, anchor_along + radius_m)
            assert not _overlaps(along_bounds, forbidden)


def _assert_no_placement_overlaps(scenario: dict) -> None:
    """Assert no two fixed obstacles overlap in both along and offset ranges."""
    placements = scenario["obstacles"]["placements"]
    for left_index, left in enumerate(placements):
        for right in placements[left_index + 1 :]:
            if left["at"]["segment"] != right["at"]["segment"]:
                continue
            along_overlap = _overlaps(_bounds(left["at"]["along_m"]), _bounds(right["at"]["along_m"]))
            offset_overlap = _overlaps(_bounds(left["at"]["offset_m"]), _bounds(right["at"]["offset_m"]))
            assert not (along_overlap and offset_overlap)


def test_repair_moves_obstacles_outside_robot_start_and_goal_safety_radius() -> None:
    scenario = _base_quality_scenario()
    scenario["obstacles"]["placements"] = [
        _cone("start_cone", along_m=1.2, offset_m=0.0),
        _cone("goal_cone", along_m=8.8, offset_m=0.0),
    ]

    repaired = RepairHandler().repair(deepcopy(scenario))

    assert len(repaired["obstacles"]["placements"]) == 2
    _assert_no_anchor_conflicts(repaired)
    assert TemplateValidator().validate(repaired).valid is True


def test_repair_distributes_overlapping_obstacles_without_losing_requested_count() -> None:
    scenario = _base_quality_scenario()
    scenario["obstacles"]["placements"] = [
        _cone("cone_01", along_m={"min": 4.0, "max": 4.4}, offset_m={"min": -0.2, "max": 0.1}),
        _cone("cone_02", along_m={"min": 4.1, "max": 4.3}, offset_m={"min": -0.1, "max": 0.2}),
        _cone("cone_03", along_m={"min": 4.2, "max": 4.5}, offset_m={"min": -0.15, "max": 0.15}),
    ]

    repaired = RepairHandler().repair(deepcopy(scenario))

    assert len(repaired["obstacles"]["placements"]) == 3
    _assert_no_placement_overlaps(repaired)
    assert TemplateValidator().validate(repaired).valid is True


def test_repair_preserves_minimum_clear_width_for_center_blocking_obstacle() -> None:
    scenario = _base_quality_scenario()
    scenario["corridor"]["walkway_width_m"] = 1.2
    scenario["obstacles"]["min_clear_width_m"] = 0.9
    scenario["obstacles"]["placements"] = [
        _cone("center_blocker", along_m={"min": 4.0, "max": 4.4}, offset_m={"min": -0.25, "max": 0.25})
    ]

    repaired = RepairHandler().repair(deepcopy(scenario))
    placement = repaired["obstacles"]["placements"][0]

    assert _max_clear_width(1.2, placement["at"]["offset_m"]) >= 0.9
    assert TemplateValidator().validate(repaired).valid is True


def test_template_validator_rejects_obstacle_along_outside_segment_range() -> None:
    scenario = _base_quality_scenario()
    scenario["obstacles"]["placements"] = [_cone("outside_cone", along_m=12.0, offset_m=0.0)]

    validation = TemplateValidator().validate(scenario)

    assert validation.valid is False
    assert any(issue.field == "obstacles.placements[0].at.along_m" for issue in validation.errors)


def test_template_validator_rejects_unknown_lane_values() -> None:
    scenario = _base_quality_scenario()
    scenario["obstacles"]["placements"] = [_cone("bad_lane_cone", along_m=4.0, offset_m=0.0)]
    scenario["obstacles"]["placements"][0]["at"]["lane"] = "hover_lane"
    scenario["robot"]["start"]["lane"] = "hover_lane"

    validation = TemplateValidator().validate(scenario)

    assert validation.valid is False
    assert any(issue.field == "obstacles.placements[0].at.lane" for issue in validation.errors)
    assert any(issue.field == "robot.start.lane" for issue in validation.errors)
