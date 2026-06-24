from __future__ import annotations

from copy import deepcopy
import re
from typing import Any

from app.agents.scenario_generation_v2.prop_normalizer import normalize_legacy_static_obstacle_prop_id
from app.agents.scenario_generation_v2.template_validator import ALLOWED_LANES


ROBOT_ANCHOR_EXCLUSION_RADIUS_M = 1.0
"""Default along-distance safety radius around robot start and goal anchors."""

DEFAULT_OBSTACLE_FOOTPRINT_WIDTH_M = 0.1
"""Conservative footprint width used for AI-side passable-width repair."""


class RepairHandler:
    """Applies deterministic, local-only repairs before validation."""

    def repair(self, template: dict[str, Any], *, repair_quality: bool = True) -> dict[str, Any]:
        """Normalize schema shape and optionally repair final obstacle quality."""
        repaired = deepcopy(template)
        if repaired.get("schema") == "scenario_template":
            repaired["schema"] = "scenario"
        scenario_id = str(repaired.get("scenario_id") or repaired.get("template_id") or "project_scenario").strip()
        repaired["scenario_id"] = re.sub(r"[^a-zA-Z0-9]+", "_", scenario_id).strip("_").lower() or "project_scenario"
        for legacy_field in ("template_id", "scenario_template"):
            repaired.pop(legacy_field, None)
        self._remove_null_fields(repaired)
        self._repair_obstacle_placements(repaired)
        self._repair_robot_anchors(repaired)
        self._swap_inverted_ranges(repaired)
        self._repair_lane_hints(repaired)
        if repair_quality:
            self._repair_obstacle_quality(repaired)
        return repaired

    def _repair_obstacle_placements(self, template: dict[str, Any]) -> None:
        """Move legacy direct obstacle pose fields under the contract-owned at object."""
        obstacles = template.get("obstacles")
        if not isinstance(obstacles, dict):
            return
        placements = obstacles.get("placements")
        if not isinstance(placements, list):
            return
        for placement in placements:
            if not isinstance(placement, dict):
                continue
            at = placement.get("at")
            direct_pose = {
                key: placement.pop(key)
                for key in ("segment", "along_m", "offset_m", "lane")
                if key in placement
            }
            if direct_pose and not isinstance(at, dict):
                placement["at"] = direct_pose
            if "prop" in placement:
                placement["prop"] = normalize_legacy_static_obstacle_prop_id(placement["prop"])

    def _repair_robot_anchors(self, template: dict[str, Any]) -> None:
        """Normalize abstract and concrete robot anchors without inventing positions."""
        robot = template.get("robot")
        if not isinstance(robot, dict):
            return
        for key in ("start", "goal"):
            anchor = robot.get(key)
            if not isinstance(anchor, dict):
                continue
            anchor_type = anchor.get("type")
            if anchor_type not in {"entry", "exit"}:
                continue
            has_pose_identity = anchor.get("segment") is not None and anchor.get("along_m") is not None
            if has_pose_identity:
                anchor["type"] = "corridor_pose"
                continue
            for field in ("segment", "along_m", "offset_m", "lane", "heading"):
                anchor.pop(field, None)

    def _repair_lane_hints(self, template: dict[str, Any]) -> None:
        """Normalize unknown lane hints to the UE-friendly walkway lane."""
        robot = template.get("robot")
        if isinstance(robot, dict):
            for key in ("start", "goal"):
                anchor = robot.get(key)
                if isinstance(anchor, dict) and anchor.get("lane") not in {None, *ALLOWED_LANES}:
                    anchor["lane"] = "walkway"
        obstacles = template.get("obstacles")
        if not isinstance(obstacles, dict):
            return
        placements = obstacles.get("placements")
        if not isinstance(placements, list):
            return
        for placement in placements:
            if not isinstance(placement, dict):
                continue
            at = placement.get("at")
            if isinstance(at, dict) and at.get("lane") not in {None, *ALLOWED_LANES}:
                at["lane"] = "walkway"
            zone = placement.get("zone")
            if not isinstance(zone, dict):
                continue
            lanes = zone.get("lanes")
            if isinstance(lanes, list):
                zone["lanes"] = [lane if isinstance(lane, str) and lane in ALLOWED_LANES else "walkway" for lane in lanes]

    def _repair_obstacle_quality(self, template: dict[str, Any]) -> None:
        """Reposition fixed obstacles that threaten anchors, overlap, or block clearance."""
        obstacles = template.get("obstacles")
        if not isinstance(obstacles, dict):
            return
        placements = obstacles.get("placements")
        if not isinstance(placements, list) or not placements:
            return
        segment_ranges = self._segment_ranges(template.get("corridor"))
        if not segment_ranges:
            return
        forbidden_ranges = self._robot_forbidden_ranges(template.get("robot"), segment_ranges)
        walkway_width_m = self._walkway_width_m(template.get("corridor"))
        min_clear_width_m = self._min_clear_width_m(obstacles)

        grouped_indices: dict[str, list[int]] = {}
        repaired = [deepcopy(placement) for placement in placements]
        for index, placement in enumerate(repaired):
            segment_id = self._placement_segment(placement)
            if segment_id in segment_ranges:
                grouped_indices.setdefault(segment_id, []).append(index)

        remove_indices: set[int] = set()
        for segment_id, indices in grouped_indices.items():
            group = [repaired[index] for index in indices]
            if not self._group_needs_quality_repair(
                group,
                segment_ranges[segment_id],
                forbidden_ranges.get(segment_id, []),
                walkway_width_m,
                min_clear_width_m,
            ):
                continue
            available_intervals = self._available_intervals(segment_ranges[segment_id], forbidden_ranges.get(segment_id, []))
            if not available_intervals:
                remove_indices.update(indices)
                continue
            for group_index, placement_index in enumerate(indices):
                interval = self._distributed_interval(available_intervals, group_index, len(indices))
                if interval is None:
                    remove_indices.add(placement_index)
                    continue
                at = repaired[placement_index].setdefault("at", {})
                if not isinstance(at, dict):
                    remove_indices.add(placement_index)
                    continue
                at["segment"] = segment_id
                at["along_m"] = self._range_around(interval, self._placement_along_width(repaired[placement_index]))
                safe_offset = self._safe_offset_range(group_index, walkway_width_m, min_clear_width_m)
                if safe_offset is None:
                    remove_indices.add(placement_index)
                    continue
                at["offset_m"] = safe_offset
                at["lane"] = "walkway"

        obstacles["placements"] = [placement for index, placement in enumerate(repaired) if index not in remove_indices]

    def _group_needs_quality_repair(
        self,
        placements: list[dict[str, Any]],
        segment_range: tuple[float, float],
        forbidden_ranges: list[tuple[float, float]],
        walkway_width_m: float | None,
        min_clear_width_m: float | None,
    ) -> bool:
        """Return whether a segment placement group violates quality guardrails."""
        for placement in placements:
            at = placement.get("at")
            if not isinstance(at, dict):
                return True
            along_bounds = self._numeric_bounds(at.get("along_m"))
            if along_bounds is None or along_bounds[0] < segment_range[0] or along_bounds[1] > segment_range[1]:
                return True
            if any(self._ranges_overlap(along_bounds, forbidden) for forbidden in forbidden_ranges):
                return True
            if self._placement_blocks_clearance(placement, walkway_width_m, min_clear_width_m):
                return True
        return self._placements_overlap(placements)

    def _placements_overlap(self, placements: list[dict[str, Any]]) -> bool:
        """Return whether any two placements overlap in both along and offset bands."""
        for left_index, left in enumerate(placements):
            for right in placements[left_index + 1 :]:
                left_at = left.get("at")
                right_at = right.get("at")
                if not isinstance(left_at, dict) or not isinstance(right_at, dict):
                    return True
                left_along = self._numeric_bounds(left_at.get("along_m"))
                right_along = self._numeric_bounds(right_at.get("along_m"))
                left_offset = self._numeric_bounds(left_at.get("offset_m"))
                right_offset = self._numeric_bounds(right_at.get("offset_m"))
                if None in {left_along, right_along, left_offset, right_offset}:
                    return True
                if self._ranges_overlap(left_along, right_along) and self._ranges_overlap(left_offset, right_offset):
                    return True
        return False

    def _placement_blocks_clearance(
        self,
        placement: dict[str, Any],
        walkway_width_m: float | None,
        min_clear_width_m: float | None,
    ) -> bool:
        """Return whether one obstacle leaves too little clear width on both sides."""
        if walkway_width_m is None or min_clear_width_m is None:
            return False
        at = placement.get("at")
        if not isinstance(at, dict):
            return True
        offset_bounds = self._numeric_bounds(at.get("offset_m"))
        if offset_bounds is None:
            return True
        return self._max_clear_width(walkway_width_m, offset_bounds) < min_clear_width_m

    def _robot_forbidden_ranges(
        self,
        robot: Any,
        segment_ranges: dict[str, tuple[float, float]],
    ) -> dict[str, list[tuple[float, float]]]:
        """Return start/goal along bands where obstacles should not be placed."""
        forbidden: dict[str, list[tuple[float, float]]] = {}
        if not isinstance(robot, dict):
            return forbidden
        for key in ("start", "goal"):
            anchor = robot.get(key)
            if not isinstance(anchor, dict) or anchor.get("type") != "corridor_pose":
                continue
            segment = anchor.get("segment")
            along_bounds = self._numeric_bounds(anchor.get("along_m"))
            if not isinstance(segment, str) or segment not in segment_ranges or along_bounds is None:
                continue
            anchor_center = (along_bounds[0] + along_bounds[1]) / 2.0
            forbidden.setdefault(segment, []).append(
                (
                    anchor_center - ROBOT_ANCHOR_EXCLUSION_RADIUS_M,
                    anchor_center + ROBOT_ANCHOR_EXCLUSION_RADIUS_M,
                )
            )
        return forbidden

    def _available_intervals(
        self,
        segment_range: tuple[float, float],
        forbidden_ranges: list[tuple[float, float]],
    ) -> list[tuple[float, float]]:
        """Return usable along intervals after removing robot anchor safety bands."""
        intervals = [segment_range]
        for forbidden in sorted(forbidden_ranges):
            next_intervals: list[tuple[float, float]] = []
            for start_m, end_m in intervals:
                forbidden_start = max(start_m, forbidden[0])
                forbidden_end = min(end_m, forbidden[1])
                if forbidden_start > start_m:
                    next_intervals.append((start_m, forbidden_start))
                if forbidden_end < end_m:
                    next_intervals.append((forbidden_end, end_m))
            intervals = next_intervals
        return [(start_m, end_m) for start_m, end_m in intervals if end_m - start_m >= 0.2]

    def _distributed_interval(
        self,
        intervals: list[tuple[float, float]],
        index: int,
        count: int,
    ) -> tuple[float, float] | None:
        """Return the source interval and point slot for a placement index."""
        total_span = sum(end_m - start_m for start_m, end_m in intervals)
        if total_span <= 0:
            return None
        target = total_span * ((index + 1) / (count + 1))
        cursor = 0.0
        for start_m, end_m in intervals:
            span = end_m - start_m
            if cursor + span >= target:
                center = start_m + (target - cursor)
                return (center, center)
            cursor += span
        start_m, end_m = intervals[-1]
        return ((start_m + end_m) / 2.0, (start_m + end_m) / 2.0)

    def _range_around(self, center_interval: tuple[float, float], width_m: float) -> dict[str, float]:
        """Return a small along range around a distributed center point."""
        center = (center_interval[0] + center_interval[1]) / 2.0
        half_width = min(0.12, max(0.05, width_m / 2.0))
        return {"min": self._rounded(center - half_width), "max": self._rounded(center + half_width)}

    def _placement_along_width(self, placement: dict[str, Any]) -> float:
        """Return the current along footprint width or a compact default."""
        at = placement.get("at")
        if not isinstance(at, dict):
            return 0.24
        along_bounds = self._numeric_bounds(at.get("along_m"))
        if along_bounds is None:
            return 0.24
        return max(0.1, min(0.24, along_bounds[1] - along_bounds[0]))

    def _safe_offset_range(
        self,
        index: int,
        walkway_width_m: float | None,
        min_clear_width_m: float | None,
    ) -> dict[str, float] | None:
        """Return a zigzag offset range that preserves the requested clear width."""
        pattern = [-0.35, 0.25, -0.15, 0.35, -0.25, 0.15]
        center = pattern[index % len(pattern)]
        candidate = (center - 0.05, center + 0.05)
        if walkway_width_m is None or min_clear_width_m is None:
            return {"min": self._rounded(candidate[0]), "max": self._rounded(candidate[1])}
        half_width = walkway_width_m / 2.0
        if self._max_clear_width(walkway_width_m, candidate) >= min_clear_width_m:
            clamped = (max(-half_width, candidate[0]), min(half_width, candidate[1]))
            return {"min": self._rounded(clamped[0]), "max": self._rounded(clamped[1])}
        footprint = DEFAULT_OBSTACLE_FOOTPRINT_WIDTH_M
        if walkway_width_m - footprint < min_clear_width_m:
            return None
        place_left = index % 2 == 0
        if place_left:
            return {"min": self._rounded(-half_width), "max": self._rounded(-half_width + footprint)}
        return {"min": self._rounded(half_width - footprint), "max": self._rounded(half_width)}

    def _max_clear_width(self, walkway_width_m: float, offset_bounds: tuple[float, float]) -> float:
        """Return the largest passable width to either side of an obstacle."""
        half_width = walkway_width_m / 2.0
        left_clear = offset_bounds[0] + half_width
        right_clear = half_width - offset_bounds[1]
        return max(left_clear, right_clear)

    def _segment_ranges(self, corridor: Any) -> dict[str, tuple[float, float]]:
        """Return valid segment ranges keyed by segment id."""
        if not isinstance(corridor, dict):
            return {}
        ranges: dict[str, tuple[float, float]] = {}
        segments = corridor.get("segments")
        if not isinstance(segments, list):
            return ranges
        for segment in segments:
            if not isinstance(segment, dict):
                continue
            segment_id = segment.get("id")
            along_range = segment.get("along_range_m")
            if isinstance(segment_id, str) and self._is_ordered_pair(along_range):
                ranges[segment_id] = (float(along_range[0]), float(along_range[1]))
        return ranges

    def _walkway_width_m(self, corridor: Any) -> float | None:
        """Return the conservative minimum walkway width from corridor data."""
        if not isinstance(corridor, dict):
            return None
        bounds = self._numeric_bounds(corridor.get("walkway_width_m"))
        return bounds[0] if bounds is not None else None

    def _min_clear_width_m(self, obstacles: dict[str, Any]) -> float | None:
        """Return the requested minimum clear width using the strictest bound."""
        bounds = self._numeric_bounds(obstacles.get("min_clear_width_m"))
        return bounds[1] if bounds is not None else None

    def _placement_segment(self, placement: object) -> str | None:
        """Return a fixed placement segment id when present."""
        if not isinstance(placement, dict):
            return None
        at = placement.get("at")
        if not isinstance(at, dict):
            return None
        segment = at.get("segment")
        return segment if isinstance(segment, str) else None

    def _numeric_bounds(self, value: Any) -> tuple[float, float] | None:
        """Return comparable numeric bounds for scalars and min/max ranges."""
        if isinstance(value, int | float):
            numeric = float(value)
            return numeric, numeric
        if isinstance(value, dict):
            minimum = value.get("min")
            maximum = value.get("max")
            if isinstance(minimum, int | float) and isinstance(maximum, int | float) and minimum <= maximum:
                return float(minimum), float(maximum)
        return None

    def _ranges_overlap(self, left: tuple[float, float], right: tuple[float, float]) -> bool:
        """Return whether two closed numeric ranges overlap."""
        return left[0] <= right[1] and right[0] <= left[1]

    def _is_ordered_pair(self, value: Any) -> bool:
        """Return whether a value is an increasing two-number pair."""
        return (
            isinstance(value, list)
            and len(value) == 2
            and isinstance(value[0], int | float)
            and isinstance(value[1], int | float)
            and float(value[0]) < float(value[1])
        )

    def _rounded(self, value: float) -> float:
        """Round generated repair coordinates to stable millimeter precision."""
        return round(float(value), 3)

    def _remove_null_fields(self, value: Any) -> None:
        """Remove optional nullable fields emitted by strict structured output schemas."""
        if isinstance(value, dict):
            for key in list(value):
                child = value[key]
                if child is None:
                    value.pop(key)
                else:
                    self._remove_null_fields(child)
        elif isinstance(value, list):
            for child in value:
                self._remove_null_fields(child)

    def _swap_inverted_ranges(self, value: Any) -> None:
        """Repair range objects whose min/max values are reversed."""
        if isinstance(value, dict):
            if set(value) >= {"min", "max"} and isinstance(value["min"], int | float) and isinstance(value["max"], int | float):
                if value["min"] > value["max"]:
                    value["min"], value["max"] = value["max"], value["min"]
            for child in value.values():
                self._swap_inverted_ranges(child)
        elif isinstance(value, list):
            for child in value:
                self._swap_inverted_ranges(child)
