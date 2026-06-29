from __future__ import annotations

from typing import Any

from app.agents.scenario_generation_v2.prop_normalizer import normalize_legacy_static_obstacle_prop_id
from app.agents.scenario_generation_v2.template_planner import TemplatePlan
from app.catalogs.static_obstacle_catalog import get_allowed_static_obstacle_prop_ids


class TemplateJsonWriter:
    """Builds current project scenario v1 JSON objects without file or run ownership."""

    def write(self, plan: TemplatePlan) -> dict[str, Any]:
        """Return a deterministic scenario object for the selected alpha pattern."""
        corridor = self._corridor(plan)
        obstacle_count = self._obstacle_count(plan)
        obstacle_props = self._obstacle_props(plan, obstacle_count)
        placements = []
        target_segments = self._target_obstacle_segments(corridor, plan)
        segment_id, segment_range = target_segments[0]
        if plan.requested_gate_obstacle_count == 2 and obstacle_count == 2 and len(target_segments) == 1:
            placements.extend(self._gate_pair_placements(segment_id, segment_range, props=obstacle_props))
        elif obstacle_count > 0:
            placements.extend(
                self._fixed_obstacle_placements_across_segments(
                    obstacle_count,
                    target_segments,
                    props=obstacle_props,
                    explicit_blocking=plan.explicit_blocking,
                    requested_counts=plan.requested_obstacle_counts,
                )
            )

        pedestrians = self._pedestrians(plan)
        robot = self._robot(corridor)
        if plan.robot_anchor_only and plan.robot_start_anchor is not None and plan.robot_goal_anchor is not None:
            robot = {"start": plan.robot_start_anchor, "goal": plan.robot_goal_anchor}

        return {
            "schema": "scenario",
            "version": 1,
            "scenario_id": self._scenario_id(plan, obstacle_count),
            "intent": self._intent(plan, obstacle_count),
            "corridor": corridor,
            "obstacles": {"min_clear_width_m": 0.9, "placements": placements},
            "pedestrians": pedestrians,
            "robot": robot,
        }

    def _corridor(self, plan: TemplatePlan) -> dict[str, Any]:
        """Return fallback corridor geometry derived from the parsed intent."""
        length_m = self._corridor_length(plan)
        if plan.corridor_profile == "complex":
            return self._complex_corridor(length_m)
        if plan.corridor_profile == "curved":
            return self._curved_corridor(length_m)
        if plan.corridor_profile == "s-curve":
            return self._s_curve_corridor(length_m)
        if plan.corridor_profile in {"g-shape", "l-shape"}:
            return self._g_shape_corridor(length_m)
        if plan.corridor_profile == "construction":
            return self._construction_corridor(length_m)
        return self._straight_corridor(length_m, single_segment=plan.requested_length_m is not None)

    def _corridor_length(self, plan: TemplatePlan) -> float:
        """Return the positive corridor length requested by the user or a profile default."""
        if plan.requested_length_m is not None and plan.requested_length_m > 0:
            return float(plan.requested_length_m)
        if plan.corridor_profile == "complex":
            return 35.0
        if plan.corridor_profile == "s-curve":
            return 16.5
        if plan.corridor_profile == "construction":
            return 12.0
        if plan.corridor_profile == "curved":
            return 10.0
        if plan.corridor_profile in {"g-shape", "l-shape"}:
            return 20.0
        return 18.0

    def _straight_corridor(self, length_m: float, *, single_segment: bool) -> dict[str, Any]:
        """Return a straight fallback corridor with optional single main segment."""
        if single_segment:
            segments = [{"id": "main", "type": "straight", "along_range_m": [0.0, self._rounded(length_m)]}]
        else:
            approach_end = self._rounded(length_m * 0.28)
            conflict_end = self._rounded(length_m * 0.62)
            segments = [
                {"id": "approach", "type": "straight", "along_range_m": [0.0, approach_end]},
                {"id": "conflict", "type": "narrowing", "along_range_m": [approach_end, conflict_end]},
                {"id": "exit", "type": "straight", "along_range_m": [conflict_end, self._rounded(length_m)]},
            ]
        return {
            "axis": {"type": "polyline", "points_m": [[0.0, 0.0], [self._rounded(length_m), 0.0]]},
            "walkway_width_m": {"min": 1.4, "max": 1.8},
            "building_side": [{"surface": "wall", "width_m": 0.3}],
            "curb_side": [{"surface": "road", "width_m": 4.0}],
            "segments": segments,
        }

    def _curved_corridor(self, length_m: float) -> dict[str, Any]:
        """Return a simple curved fallback corridor independent of preset files."""
        length_m = self._rounded(length_m)
        return {
            "axis": {
                "type": "polyline",
                "points_m": [
                    [0.0, 0.0],
                    [self._rounded(length_m * 0.33), 0.8],
                    [self._rounded(length_m * 0.66), 0.8],
                    [length_m, 0.0],
                ],
            },
            "walkway_width_m": {"min": 2.4, "max": 3.0},
            "building_side": [{"surface": "wall", "width_m": 0.4}],
            "curb_side": [{"surface": "road", "width_m": 4.0}],
            "segments": [
                {"id": "entry_straight", "type": "straight", "along_range_m": [0.0, self._rounded(length_m * 0.25)]},
                {
                    "id": "road_curve",
                    "type": "straight",
                    "along_range_m": [self._rounded(length_m * 0.25), self._rounded(length_m * 0.75)],
                },
                {"id": "exit_straight", "type": "straight", "along_range_m": [self._rounded(length_m * 0.75), length_m]},
            ],
        }

    def _s_curve_corridor(self, length_m: float) -> dict[str, Any]:
        """Return a multi-curve fallback corridor independent of preset files."""
        length_m = self._rounded(length_m)
        points = [
            [0.0, 0.0],
            [self._rounded(length_m * 0.16), -0.7],
            [self._rounded(length_m * 0.32), -1.4],
            [self._rounded(length_m * 0.49), 0.8],
            [self._rounded(length_m * 0.66), 1.5],
            [self._rounded(length_m * 0.83), -0.9],
            [self._rounded(length_m), -0.1],
        ]
        return {
            "axis": {"type": "polyline", "points_m": points},
            "walkway_width_m": {"min": 2.8, "max": 3.2},
            "building_side": [{"surface": "wall", "width_m": 0.4}],
            "curb_side": [{"surface": "road", "width_m": 4.0}],
            "segments": [
                {"id": "entry", "type": "straight", "along_range_m": [0.0, self._rounded(length_m * 0.13)]},
                {
                    "id": "curve_left_1",
                    "type": "straight",
                    "along_range_m": [self._rounded(length_m * 0.13), self._rounded(length_m * 0.33)],
                },
                {
                    "id": "curve_right_1",
                    "type": "narrowing",
                    "along_range_m": [self._rounded(length_m * 0.33), self._rounded(length_m * 0.55)],
                },
                {
                    "id": "curve_left_2",
                    "type": "narrowing",
                    "along_range_m": [self._rounded(length_m * 0.55), self._rounded(length_m * 0.76)],
                },
                {
                    "id": "curve_right_2",
                    "type": "straight",
                    "along_range_m": [self._rounded(length_m * 0.76), self._rounded(length_m * 0.91)],
                },
                {"id": "exit", "type": "straight", "along_range_m": [self._rounded(length_m * 0.91), length_m]},
            ],
        }

    def _g_shape_corridor(self, length_m: float) -> dict[str, Any]:
        """Return a right-angle corridor with a pre-corner construction segment."""
        length_m = self._rounded(max(6.0, length_m))
        corner_along_m = self._rounded(length_m * 0.5)
        exit_leg_m = self._rounded(length_m - corner_along_m)
        pre_corner_width_m = self._rounded(min(3.0, max(1.5, length_m * 0.15)))
        pre_corner_start_m = self._rounded(max(0.0, corner_along_m - pre_corner_width_m))
        return {
            "axis": {
                "type": "polyline",
                "points_m": [
                    [0.0, 0.0],
                    [corner_along_m, 0.0],
                    [corner_along_m, exit_leg_m],
                ],
            },
            "walkway_width_m": {"min": 1.6, "max": 2.2},
            "building_side": [{"surface": "wall", "width_m": 0.4}],
            "curb_side": [{"surface": "road", "width_m": 4.0}],
            "segments": [
                {"id": "approach", "type": "straight", "along_range_m": [0.0, pre_corner_start_m]},
                {
                    "id": "pre_corner_construction",
                    "type": "narrowing",
                    "along_range_m": [pre_corner_start_m, corner_along_m],
                },
                {
                    "id": "turn_and_exit",
                    "type": "straight",
                    "along_range_m": [corner_along_m, length_m],
                },
            ],
        }

    def _complex_corridor(self, length_m: float) -> dict[str, Any]:
        """Return a long straight/S-curve/right-angle corridor for compound prompts."""
        length_m = self._rounded(max(12.0, length_m))
        base_points = [
            [0.0, 0.0],
            [10.0, 0.0],
            [14.0, -2.0],
            [18.0, 2.0],
            [22.0, -1.5],
            [26.0, 0.0],
            [30.0, 0.0],
            [30.0, 5.0],
        ]
        scale = length_m / self._axis_path_length(base_points)
        points = [[self._rounded(point[0] * scale), self._rounded(point[1] * scale)] for point in base_points]
        return {
            "axis": {"type": "polyline", "points_m": points},
            "walkway_width_m": {"min": 2.4, "max": 3.0},
            "building_side": [{"surface": "wall", "width_m": 0.4}],
            "curb_side": [{"surface": "road", "width_m": 4.0}],
            "segments": [
                {"id": "entry_straight", "type": "straight", "along_range_m": [0.0, self._rounded(length_m * 0.25)]},
                {
                    "id": "s_curve_entry",
                    "type": "straight",
                    "along_range_m": [self._rounded(length_m * 0.25), self._rounded(length_m * 0.42)],
                },
                {
                    "id": "middle_construction",
                    "type": "narrowing",
                    "along_range_m": [self._rounded(length_m * 0.42), self._rounded(length_m * 0.58)],
                },
                {
                    "id": "s_curve_exit",
                    "type": "straight",
                    "along_range_m": [self._rounded(length_m * 0.58), self._rounded(length_m * 0.68)],
                },
                {
                    "id": "pre_corner_conflict",
                    "type": "narrowing",
                    "along_range_m": [self._rounded(length_m * 0.68), self._rounded(length_m * 0.84)],
                },
                {"id": "turn_and_exit", "type": "straight", "along_range_m": [self._rounded(length_m * 0.84), length_m]},
            ],
        }

    def _construction_corridor(self, length_m: float) -> dict[str, Any]:
        """Return a construction-style straight corridor for fallback generation."""
        corridor = self._straight_corridor(length_m, single_segment=False)
        corridor["walkway_width_m"] = {"min": 1.2, "max": 1.8}
        return corridor

    def _obstacle_count(self, plan: TemplatePlan) -> int:
        """Return the final fallback obstacle count after user overrides."""
        requested_count = self._requested_obstacle_count(plan)
        if plan.explicit_no_obstacles:
            return 0
        if requested_count is not None:
            return requested_count
        if plan.requested_props:
            return len(plan.requested_props)
        return 1 if plan.include_obstacle else 0

    def _requested_obstacle_count(self, plan: TemplatePlan) -> int | None:
        """Return the explicit obstacle count carried by the parsed user request."""
        if plan.requested_obstacle_count is not None:
            return max(0, int(plan.requested_obstacle_count))
        if plan.requested_gate_obstacle_count is not None:
            return max(0, int(plan.requested_gate_obstacle_count))
        return None

    def _obstacle_props(self, plan: TemplatePlan, count: int) -> list[str]:
        """Return catalog-owned obstacle props for generated placements."""
        if count <= 0:
            return []
        valid_props = self._requested_valid_props(plan)
        if not valid_props:
            valid_props = ["obstacle.road_cone_01"]
        return [valid_props[index % len(valid_props)] for index in range(count)]

    def _requested_valid_props(self, plan: TemplatePlan) -> list[str]:
        """Return requested props that are present in the static obstacle catalog."""
        allowed_props = get_allowed_static_obstacle_prop_ids()
        valid_props = [
            requested
            for requested in (normalize_legacy_static_obstacle_prop_id(prop) for prop in plan.requested_props)
            if isinstance(requested, str) and requested in allowed_props
        ]
        if valid_props:
            return valid_props
        requested = normalize_legacy_static_obstacle_prop_id(plan.requested_prop)
        if isinstance(requested, str) and requested in allowed_props:
            return [requested]
        return []

    def _fixed_obstacle_placements(
        self,
        count: int,
        segment_id: str,
        segment_range: tuple[float, float],
        *,
        props: list[str],
        explicit_blocking: bool,
        start_index: int = 0,
    ) -> list[dict[str, Any]]:
        """Return catalog-safe fixed obstacle placements inside the conflict segment."""
        placements = []
        offset_centers = [-0.35, 0.25, -0.15, 0.35, -0.25, 0.15]
        for index in range(count):
            global_index = start_index + index
            offset_center = offset_centers[global_index % len(offset_centers)]
            prop = self._prop_at(props, global_index)
            placement: dict[str, Any] = {
                "kind": "fixed",
                "id": self._obstacle_id(prop, global_index),
                "prop": prop,
                "at": {
                    "segment": segment_id,
                    "along_m": self._distributed_along_range(segment_range, index, count),
                    "offset_m": self._offset_range(offset_center),
                    "lane": "walkway",
                },
                "yaw_deg": 0,
            }
            if explicit_blocking:
                placement["allow_blocking"] = True
            placements.append(placement)
        return placements

    def _fixed_obstacle_placements_across_segments(
        self,
        count: int,
        target_segments: list[tuple[str, tuple[float, float]]],
        *,
        props: list[str],
        explicit_blocking: bool,
        requested_counts: list[int],
    ) -> list[dict[str, Any]]:
        """Return repeated obstacles distributed across one or more target segments."""
        if not target_segments:
            return []
        segment_counts = self._segment_obstacle_counts(count, len(target_segments), requested_counts)
        placements: list[dict[str, Any]] = []
        start_index = 0
        for (segment_id, segment_range), segment_count in zip(target_segments, segment_counts, strict=False):
            placements.extend(
                self._fixed_obstacle_placements(
                    segment_count,
                    segment_id,
                    segment_range,
                    props=props,
                    explicit_blocking=explicit_blocking,
                    start_index=start_index,
                )
            )
            start_index += segment_count
        return placements

    def _segment_obstacle_counts(self, count: int, segment_count: int, requested_counts: list[int]) -> list[int]:
        """Map requested obstacle groups onto available target segments."""
        if segment_count <= 0:
            return []
        if len(requested_counts) > 1:
            counts = [0 for _ in range(segment_count)]
            for index, requested_count in enumerate(requested_counts):
                counts[min(index, segment_count - 1)] += max(0, int(requested_count))
            return counts
        base = count // segment_count
        remainder = count % segment_count
        return [base + (1 if index < remainder else 0) for index in range(segment_count)]

    def _gate_pair_placements(
        self,
        segment_id: str,
        segment_range: tuple[float, float],
        *,
        props: list[str],
    ) -> list[dict[str, Any]]:
        """Return exactly one left/right gate pair for prompts that request two obstacles."""
        along_range = self._obstacle_along_range(segment_range, width_m=0.4)
        left_prop = self._prop_at(props, 0)
        right_prop = self._prop_at(props, 1)
        return [
            {
                "kind": "fixed",
                "id": "gate_panel_left",
                "prop": left_prop,
                "at": {
                    "segment": segment_id,
                    "along_m": along_range,
                    "offset_m": {"min": -0.35, "max": -0.25},
                    "lane": "walkway",
                },
                "yaw_deg": 0,
                "allow_blocking": False,
            },
            {
                "kind": "fixed",
                "id": "gate_panel_right",
                "prop": right_prop,
                "at": {
                    "segment": segment_id,
                    "along_m": along_range,
                    "offset_m": {"min": 0.25, "max": 0.35},
                    "lane": "walkway",
                },
                "yaw_deg": 0,
                "allow_blocking": False,
            },
        ]

    def _prop_at(self, props: list[str], index: int) -> str:
        """Return the requested prop at index or a catalog-safe fallback."""
        if props:
            return props[index % len(props)]
        return "obstacle.road_cone_01"

    def _target_obstacle_segment(self, corridor: dict[str, Any]) -> tuple[str, tuple[float, float]]:
        """Return a valid segment anchor for generated fallback obstacles."""
        segments = [segment for segment in corridor.get("segments", []) if isinstance(segment, dict)]
        for preferred_id in ("pre_corner_construction", "conflict", "road_curve", "curve_right_1", "main"):
            for segment in segments:
                if segment.get("id") == preferred_id:
                    return str(preferred_id), self._segment_range(segment)
        if segments:
            index = min(1, len(segments) - 1)
            segment = segments[index]
            return str(segment.get("id") or "main"), self._segment_range(segment)
        return "main", (0.0, 1.0)

    def _target_obstacle_segments(
        self,
        corridor: dict[str, Any],
        plan: TemplatePlan,
    ) -> list[tuple[str, tuple[float, float]]]:
        """Return one or more segment anchors for generated fallback obstacles."""
        if plan.corridor_profile == "complex":
            segments = [
                (str(segment.get("id")), self._segment_range(segment))
                for segment in corridor.get("segments", [])
                if isinstance(segment, dict) and segment.get("type") == "narrowing" and isinstance(segment.get("id"), str)
            ]
            if segments:
                return segments
        return [self._target_obstacle_segment(corridor)]

    def _segment_range(self, segment: dict[str, Any]) -> tuple[float, float]:
        """Return a segment along range or a valid fallback range."""
        along_range = segment.get("along_range_m")
        if (
            isinstance(along_range, list)
            and len(along_range) == 2
            and isinstance(along_range[0], int | float)
            and isinstance(along_range[1], int | float)
            and float(along_range[0]) <= float(along_range[1])
        ):
            return float(along_range[0]), float(along_range[1])
        return 0.0, 1.0

    def _obstacle_along_range(self, segment_range: tuple[float, float], *, width_m: float = 0.6) -> dict[str, float]:
        """Return a centered obstacle along band inside a target segment."""
        start_m, end_m = segment_range
        span = max(0.0, end_m - start_m)
        center = start_m + span * 0.5
        half_width = min(width_m * 0.5, max(0.1, span * 0.1))
        return {
            "min": self._rounded(max(start_m, center - half_width)),
            "max": self._rounded(min(end_m, center + half_width)),
        }

    def _distributed_along_range(
        self,
        segment_range: tuple[float, float],
        index: int,
        count: int,
    ) -> dict[str, float]:
        """Return a non-overlapping-ish along band distributed through the segment."""
        start_m, end_m = segment_range
        span = max(0.0, end_m - start_m)
        slot_count = max(1, count)
        center = start_m + span * ((index + 1) / (slot_count + 1))
        half_width = min(0.12, max(0.05, span / (slot_count + 1) * 0.15))
        return {
            "min": self._rounded(self._clamp(center - half_width, start_m, end_m)),
            "max": self._rounded(self._clamp(center + half_width, start_m, end_m)),
        }

    def _offset_range(self, center_m: float) -> dict[str, float]:
        """Return a narrow offset band around the requested obstacle lane center."""
        return {"min": self._rounded(center_m - 0.05), "max": self._rounded(center_m + 0.05)}

    def _obstacle_id(self, prop: str, index: int) -> str:
        """Return a stable placement id that reflects cone-specific prompt intent."""
        if prop == "obstacle.road_cone_01":
            return f"cone_{index + 1:02d}"
        return "center_obstacle" if index == 0 else f"center_obstacle_{index + 1}"

    def _pedestrians(self, plan: TemplatePlan) -> dict[str, Any]:
        """Return alpha fallback pedestrian output from parsed pedestrian intent."""
        return {"background": {"count": 0, "speed_mps": 1.0}, "encounters": []}

    def _robot(self, corridor: dict[str, Any]) -> dict[str, Any]:
        """Return start and goal anchors that stay inside fallback corridor segments."""
        segments = [segment for segment in corridor.get("segments", []) if isinstance(segment, dict)]
        if not segments:
            return {
                "start": self._corridor_pose_anchor("main", (0.0, 1.0), prefer_start=True),
                "goal": self._corridor_pose_anchor("main", (0.0, 1.0), prefer_start=False),
            }
        first = segments[0]
        last = segments[-1]
        return {
            "start": self._corridor_pose_anchor(str(first.get("id") or "main"), self._segment_range(first), prefer_start=True),
            "goal": self._corridor_pose_anchor(str(last.get("id") or "main"), self._segment_range(last), prefer_start=False),
        }

    def _corridor_pose_anchor(
        self,
        segment_id: str,
        segment_range: tuple[float, float],
        *,
        prefer_start: bool,
    ) -> dict[str, Any]:
        """Build a corridor-local robot anchor inside a segment range."""
        start_m, end_m = segment_range
        span = max(0.0, end_m - start_m)
        offset = min(1.0, max(0.25, span * 0.25))
        along_m = start_m + offset if prefer_start else end_m - offset
        return {
            "type": "corridor_pose",
            "segment": segment_id,
            "along_m": self._rounded(along_m),
            "offset_m": 0.0,
            "lane": "walkway",
            "heading": "forward",
        }

    def _scenario_id(self, plan: TemplatePlan, obstacle_count: int) -> str:
        """Return a scenario id that matches the intent-based fallback shape."""
        if plan.corridor_profile == "complex":
            return "complex_s_curve_corner_obstacles" if obstacle_count else "complex_s_curve_corner_navigation"
        if plan.corridor_profile == "curved":
            return "curved_road_static_obstacle" if obstacle_count else "curved_road_sidewalk"
        if plan.corridor_profile == "s-curve":
            return "long_multi_curve_open_clearance_03"
        if plan.corridor_profile in {"g-shape", "l-shape"}:
            return "g_shape_pre_corner_construction_cones" if obstacle_count else "g_shape_corridor_navigation"
        if plan.corridor_profile == "construction":
            return "construction_fallback_static_obstacle" if obstacle_count else "construction_fallback_sidewalk"
        if plan.requested_length_m is not None:
            return f"straight_{self._length_label(plan.requested_length_m)}_sidewalk"
        return plan.scenario_id

    def _intent(self, plan: TemplatePlan, obstacle_count: int) -> str:
        """Return intent text aligned with generated fallback geometry."""
        if plan.corridor_profile == "complex":
            return (
                "Evaluate long sidewalk navigation through straight, S-curve, and right-angle corner sections with distributed cones."
                if obstacle_count
                else "Evaluate long sidewalk navigation through straight, S-curve, and right-angle corner sections."
            )
        if plan.corridor_profile == "curved":
            return (
                "Evaluate route following on a curved road sidewalk with user-requested static obstacles."
                if obstacle_count
                else "Evaluate route following on a curved road sidewalk without static obstacles."
            )
        if plan.corridor_profile == "s-curve":
            return "Evaluate route following on a long S-curve sidewalk with intent-based obstacle placement."
        if plan.corridor_profile in {"g-shape", "l-shape"}:
            return (
                "Evaluate right-angle corridor navigation with pre-corner construction cone placement."
                if obstacle_count
                else "Evaluate right-angle corridor navigation without static obstacles."
            )
        if plan.requested_length_m is not None:
            return f"Evaluate straight sidewalk navigation over {plan.requested_length_m:g}m."
        return plan.intent

    def _length_label(self, length_m: float) -> str:
        """Return a snake-case-safe meter label for scenario ids."""
        if float(length_m).is_integer():
            return f"{int(length_m)}m"
        return f"{str(length_m).replace('.', '_')}m"

    def _rounded(self, value: float) -> float:
        """Round generated geometry values to stable millimeter precision."""
        return round(float(value), 3)

    def _axis_path_length(self, points: list[list[float]]) -> float:
        """Return the 2D length of a polyline used for generated corridor scaling."""
        total = 0.0
        for previous, current in zip(points, points[1:], strict=False):
            total += (
                (float(current[0]) - float(previous[0])) ** 2
                + (float(current[1]) - float(previous[1])) ** 2
            ) ** 0.5
        return total

    def _clamp(self, value: float, minimum: float, maximum: float) -> float:
        """Clamp a number to an inclusive range."""
        return min(max(value, minimum), maximum)
