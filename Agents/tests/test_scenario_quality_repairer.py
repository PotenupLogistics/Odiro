from __future__ import annotations

from copy import deepcopy

from app.agents.scenario_generation_v2.repair_diagnostics import RepairDiagnosticCode, RepairDiagnosticCollector
from app.agents.scenario_generation_v2.repair_handler import (
    ROBOT_GOAL_EXCLUSION_RADIUS_M,
    ROBOT_START_EXCLUSION_RADIUS_M,
    RepairHandler,
)
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


def _assert_no_anchor_conflicts(
    scenario: dict,
    *,
    start_radius_m: float = ROBOT_START_EXCLUSION_RADIUS_M,
    goal_radius_m: float = ROBOT_GOAL_EXCLUSION_RADIUS_M,
) -> None:
    """Assert fixed obstacles stay outside start/goal along safety bands."""
    anchors = [
        (scenario["robot"]["start"], start_radius_m),
        (scenario["robot"]["goal"], goal_radius_m),
    ]
    for placement in scenario["obstacles"]["placements"]:
        at = placement["at"]
        along_bounds = _bounds(at["along_m"])
        for anchor, radius_m in anchors:
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


def _repair_with_events(
    scenario: dict,
    *,
    stage: str = "unit.repair",
    repair_quality: bool = True,
) -> tuple[dict, list[dict]]:
    """Run repair with an event collector and return JSON-friendly events."""
    collector = RepairDiagnosticCollector()
    repaired = RepairHandler().repair(deepcopy(scenario), repair_quality=repair_quality, diagnostics=collector, stage=stage)
    return repaired, collector.as_dicts()


def _event_by_code(events: list[dict], code: RepairDiagnosticCode) -> dict:
    """Return the first diagnostic event with the requested code."""
    return next(event for event in events if event["code"] == code.value)


def _event_codes(events: list[dict]) -> set[str]:
    """Return repair diagnostic codes from JSON-friendly event objects."""
    return {event["code"] for event in events}


def test_repair_records_legacy_pose_prop_and_lane_diagnostics() -> None:
    scenario = _base_quality_scenario()
    existing_at = _cone("existing_at", along_m=5.0, offset_m=-0.65)
    existing_at.update({"segment": "main", "along_m": 5.2, "offset_m": 0.2, "lane": "center"})
    scenario["obstacles"]["placements"] = [
        {
            "kind": "fixed",
            "id": "legacy_pose",
            "prop": "traffic_cone_01",
            "segment": "main",
            "along_m": 4.0,
            "offset_m": -0.65,
            "lane": "hover_lane",
            "yaw_deg": 0,
        },
        existing_at,
    ]

    repaired, events = _repair_with_events(scenario, repair_quality=False)

    assert repaired["obstacles"]["placements"][0]["prop"] == "obstacle.road_cone_01"
    assert repaired["obstacles"]["placements"][0]["at"]["lane"] == "walkway"
    assert "segment" not in repaired["obstacles"]["placements"][1]
    assert repaired["obstacles"]["placements"][1]["at"]["along_m"] == 5.0
    assert {
        RepairDiagnosticCode.LEGACY_POSE_MIGRATED.value,
        RepairDiagnosticCode.LEGACY_POSE_REMOVED_DUE_TO_EXISTING_AT.value,
        RepairDiagnosticCode.PROP_NORMALIZED.value,
        RepairDiagnosticCode.LANE_HINT_NORMALIZED.value,
    } <= _event_codes(events)
    prop_event = _event_by_code(events, RepairDiagnosticCode.PROP_NORMALIZED)
    assert prop_event["stage"] == "unit.repair"
    assert prop_event["path"] == "obstacles.placements[0].prop"
    assert prop_event["target_id"] == "legacy_pose"
    assert prop_event["before"] == "traffic_cone_01"
    assert prop_event["after"] == "obstacle.road_cone_01"


def test_repair_records_robot_anchor_and_range_diagnostics() -> None:
    scenario = _base_quality_scenario()
    scenario["robot"]["start"] = {
        "type": "entry",
        "segment": "main",
        "along_m": 1.0,
        "offset_m": 0.0,
        "lane": "walkway",
    }
    scenario["obstacles"]["placements"] = [
        _cone("range_cone", along_m={"min": 5.0, "max": 4.0}, offset_m=-0.65)
    ]

    repaired, events = _repair_with_events(scenario, repair_quality=False)

    assert repaired["robot"]["start"]["type"] == "corridor_pose"
    assert repaired["obstacles"]["placements"][0]["at"]["along_m"] == {"min": 4.0, "max": 5.0}
    assert RepairDiagnosticCode.ROBOT_ANCHOR_NORMALIZED.value in _event_codes(events)
    range_event = _event_by_code(events, RepairDiagnosticCode.RANGE_SWAPPED)
    assert range_event["path"] == "obstacles.placements[0].at.along_m"
    assert range_event["before"] == "min=5.0,max=4.0"
    assert range_event["after"] == "min=4.0,max=5.0"


def test_repair_records_no_events_when_nothing_changes() -> None:
    scenario = _base_quality_scenario()
    scenario["obstacles"]["placements"] = [_cone("safe_cone", along_m=5.0, offset_m=-0.65)]

    _, events = _repair_with_events(scenario)

    assert events == []


def test_repair_records_anchor_clearance_relocation_and_removal() -> None:
    scenario = _base_quality_scenario()
    scenario["obstacles"]["placements"] = [_cone("start_cone", along_m=1.2, offset_m=-0.65)]

    repaired, events = _repair_with_events(scenario)

    assert len(repaired["obstacles"]["placements"]) == 1
    _assert_no_anchor_conflicts(repaired)
    assert RepairDiagnosticCode.OBSTACLE_RELOCATED_ANCHOR_CLEARANCE.value in _event_codes(events)

    blocked = _base_quality_scenario()
    blocked["corridor"]["axis"]["points_m"] = [[0.0, 0.0], [2.0, 0.0]]
    blocked["corridor"]["segments"] = [{"id": "main", "type": "straight", "along_range_m": [0.0, 2.0]}]
    blocked["robot"]["start"]["along_m"] = 0.5
    blocked["robot"]["goal"]["along_m"] = 1.5
    blocked["obstacles"]["placements"] = [_cone("trapped_cone", along_m=1.0, offset_m=-0.65)]

    removed, removal_events = _repair_with_events(blocked)

    assert removed["obstacles"]["placements"] == []
    assert RepairDiagnosticCode.OBSTACLE_REMOVED_ANCHOR_CLEARANCE.value in _event_codes(removal_events)


def test_repair_applies_two_meter_start_and_one_meter_goal_clearance() -> None:
    """Relocate only obstacles inside the start 2m band or goal 1m band."""
    start_blocked = _base_quality_scenario()
    start_blocked["obstacles"]["placements"] = [_cone("start_outer_cone", along_m=2.5, offset_m=-0.65)]

    start_repaired, start_events = _repair_with_events(start_blocked)

    assert len(start_repaired["obstacles"]["placements"]) == 1
    _assert_no_anchor_conflicts(start_repaired)
    assert RepairDiagnosticCode.OBSTACLE_RELOCATED_ANCHOR_CLEARANCE.value in _event_codes(start_events)

    goal_allowed = _base_quality_scenario()
    goal_allowed["obstacles"]["placements"] = [_cone("goal_outer_cone", along_m=7.5, offset_m=-0.65)]

    goal_allowed_repaired, goal_allowed_events = _repair_with_events(goal_allowed)

    assert goal_allowed_repaired["obstacles"]["placements"][0]["at"]["along_m"] == 7.5
    _assert_no_anchor_conflicts(goal_allowed_repaired)
    assert goal_allowed_events == []

    goal_blocked = _base_quality_scenario()
    goal_blocked["obstacles"]["placements"] = [_cone("goal_inner_cone", along_m=8.5, offset_m=-0.65)]

    goal_repaired, goal_events = _repair_with_events(goal_blocked)

    assert len(goal_repaired["obstacles"]["placements"]) == 1
    _assert_no_anchor_conflicts(goal_repaired)
    assert RepairDiagnosticCode.OBSTACLE_RELOCATED_ANCHOR_CLEARANCE.value in _event_codes(goal_events)


def test_repair_treats_start_safety_boundary_as_overlap() -> None:
    """Relocate obstacles that touch the closed start safety boundary."""
    scenario = _base_quality_scenario()
    scenario["obstacles"]["placements"] = [_cone("start_boundary_cone", along_m=3.0, offset_m=-0.65)]

    repaired, events = _repair_with_events(scenario)

    assert len(repaired["obstacles"]["placements"]) == 1
    _assert_no_anchor_conflicts(repaired)
    assert RepairDiagnosticCode.OBSTACLE_RELOCATED_ANCHOR_CLEARANCE.value in _event_codes(events)


def test_repair_removes_obstacles_when_start_goal_bands_fill_short_corridor() -> None:
    """Remove obstacles when start 2m and goal 1m bands leave no usable along interval."""
    scenario = _base_quality_scenario()
    scenario["corridor"]["axis"]["points_m"] = [[0.0, 0.0], [4.0, 0.0]]
    scenario["corridor"]["segments"] = [{"id": "main", "type": "straight", "along_range_m": [0.0, 4.0]}]
    scenario["robot"]["start"]["along_m"] = 1.0
    scenario["robot"]["goal"]["along_m"] = 3.5
    scenario["obstacles"]["placements"] = [_cone("trapped_cone", along_m=2.8, offset_m=-0.65)]

    removed, removal_events = _repair_with_events(scenario)

    assert removed["obstacles"]["placements"] == []
    assert TemplateValidator().validate(removed).valid is True
    assert RepairDiagnosticCode.OBSTACLE_REMOVED_ANCHOR_CLEARANCE.value in _event_codes(removal_events)


def test_repair_records_overlap_redistribution() -> None:
    scenario = _base_quality_scenario()
    scenario["obstacles"]["placements"] = [
        _cone("cone_01", along_m={"min": 4.0, "max": 4.4}, offset_m=-0.65),
        _cone("cone_02", along_m={"min": 4.1, "max": 4.3}, offset_m=-0.65),
    ]

    repaired, events = _repair_with_events(scenario)

    _assert_no_placement_overlaps(repaired)
    assert RepairDiagnosticCode.OBSTACLE_OVERLAP_REDISTRIBUTED.value in _event_codes(events)


def test_repair_records_min_clear_width_adjustment_and_removal() -> None:
    scenario = _base_quality_scenario()
    scenario["corridor"]["walkway_width_m"] = 1.2
    scenario["obstacles"]["min_clear_width_m"] = 0.9
    scenario["obstacles"]["placements"] = [
        _cone("center_blocker", along_m={"min": 4.0, "max": 4.4}, offset_m={"min": -0.25, "max": 0.25})
    ]

    repaired, events = _repair_with_events(scenario)

    assert _max_clear_width(1.2, repaired["obstacles"]["placements"][0]["at"]["offset_m"]) >= 0.9
    assert RepairDiagnosticCode.MIN_CLEAR_WIDTH_OFFSET_ADJUSTED.value in _event_codes(events)

    impossible = _base_quality_scenario()
    impossible["corridor"]["walkway_width_m"] = 0.9
    impossible["obstacles"]["min_clear_width_m"] = 0.85
    impossible["obstacles"]["placements"] = [
        _cone("impossible_blocker", along_m={"min": 4.0, "max": 4.4}, offset_m={"min": -0.1, "max": 0.1})
    ]

    removed, removal_events = _repair_with_events(impossible)

    assert removed["obstacles"]["placements"] == []
    assert RepairDiagnosticCode.OBSTACLE_REMOVED_MIN_CLEAR_WIDTH.value in _event_codes(removal_events)


def test_repair_records_invalid_anchor_removals() -> None:
    """Remove malformed obstacle anchors with an explicit invalid-anchor diagnostic."""
    scenario = _base_quality_scenario()
    non_object_at = _cone("non_object_at", along_m=4.0, offset_m=-0.65)
    non_object_at["at"] = "main:4.0"
    missing_segment = _cone("missing_segment", along_m=5.0, offset_m=-0.65)
    missing_segment["at"].pop("segment")
    unknown_segment = _cone("unknown_segment", along_m=6.0, offset_m=-0.65)
    unknown_segment["at"]["segment"] = "missing_segment"
    bad_along = _cone("bad_along", along_m=7.0, offset_m=-0.65)
    bad_along["at"]["along_m"] = "seven"
    scenario["obstacles"]["placements"] = [non_object_at, missing_segment, unknown_segment, bad_along]

    repaired, events = _repair_with_events(scenario)

    assert repaired["obstacles"]["placements"] == []
    invalid_events = [
        event for event in events if event["code"] == RepairDiagnosticCode.OBSTACLE_REMOVED_INVALID_ANCHOR.value
    ]
    assert len(invalid_events) == 4
    assert {event["target_id"] for event in invalid_events} == {
        "non_object_at",
        "missing_segment",
        "unknown_segment",
        "bad_along",
    }
    for event in invalid_events:
        assert event["stage"] == "unit.repair"
        assert event["after"] == "removed"
        assert event["severity"] == "warning"
        assert isinstance(event["before"], str)
        assert not event["before"].startswith("{")


def test_repair_records_no_valid_interval_removal_separately_from_invalid_anchor() -> None:
    """Keep exhausted-interval removal distinct from malformed-anchor removal."""
    scenario = _base_quality_scenario()
    scenario["corridor"]["axis"]["points_m"] = [[0.0, 0.0], [2.0, 0.0]]
    scenario["corridor"]["segments"] = [{"id": "main", "type": "straight", "along_range_m": [0.0, 2.0]}]
    scenario["robot"]["start"]["along_m"] = 0.5
    scenario["robot"]["goal"]["along_m"] = 1.5
    scenario["obstacles"]["placements"] = [_cone("outside_but_parseable", along_m=20.0, offset_m=-0.65)]

    repaired, events = _repair_with_events(scenario)

    assert repaired["obstacles"]["placements"] == []
    assert RepairDiagnosticCode.OBSTACLE_REMOVED_NO_VALID_INTERVAL.value in _event_codes(events)
    assert RepairDiagnosticCode.OBSTACLE_REMOVED_INVALID_ANCHOR.value not in _event_codes(events)


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
