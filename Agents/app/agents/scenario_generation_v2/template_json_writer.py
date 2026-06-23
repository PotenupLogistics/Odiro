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
        obstacle_prop = self._obstacle_prop(plan)
        placements = []
        segment_id, segment_range = self._target_obstacle_segment(corridor)
        if obstacle_count == 2:
            placements.extend(self._gate_pair_placements(segment_id, segment_range, prop=obstacle_prop))
        elif obstacle_count > 0:
            placements.extend(
                self._fixed_obstacle_placements(
                    obstacle_count,
                    segment_id,
                    segment_range,
                    prop=obstacle_prop,
                    explicit_blocking=plan.explicit_blocking,
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
        if plan.corridor_profile == "curved":
            return self._curved_corridor(length_m)
        if plan.corridor_profile == "s-curve":
            return self._s_curve_corridor(length_m)
        if plan.corridor_profile == "construction":
            return self._construction_corridor(length_m)
        return self._straight_corridor(length_m, single_segment=plan.requested_length_m is not None)

    def _corridor_length(self, plan: TemplatePlan) -> float:
        """Return the positive corridor length requested by the user or a profile default."""
        if plan.requested_length_m is not None and plan.requested_length_m > 0:
            return float(plan.requested_length_m)
        if plan.corridor_profile == "s-curve":
            return 16.5
        if plan.corridor_profile == "construction":
            return 12.0
        if plan.corridor_profile == "curved":
            return 10.0
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
        return 1 if plan.include_obstacle else 0

    def _requested_obstacle_count(self, plan: TemplatePlan) -> int | None:
        """Return the explicit obstacle count carried by the parsed user request."""
        if plan.requested_obstacle_count is not None:
            return max(0, int(plan.requested_obstacle_count))
        if plan.requested_gate_obstacle_count is not None:
            return max(0, int(plan.requested_gate_obstacle_count))
        return None

    def _obstacle_prop(self, plan: TemplatePlan) -> str:
        """Return a catalog-owned fallback obstacle prop for generated placements."""
        requested = normalize_legacy_static_obstacle_prop_id(plan.requested_prop)
        if isinstance(requested, str) and requested in get_allowed_static_obstacle_prop_ids():
            return requested
        return "obstacle.road_cone_01"

    def _fixed_obstacle_placements(
        self,
        count: int,
        segment_id: str,
        segment_range: tuple[float, float],
        *,
        prop: str,
        explicit_blocking: bool,
    ) -> list[dict[str, Any]]:
        """Return catalog-safe fixed obstacle placements inside the conflict segment."""
        placements = []
        offsets = [
            {"min": 0.45, "max": 0.75},
            {"min": -0.35, "max": -0.05},
            {"min": 0.05, "max": 0.35},
        ]
        along_range = self._obstacle_along_range(segment_range)
        for index in range(count):
            placement: dict[str, Any] = {
                "kind": "fixed",
                "id": "center_obstacle" if index == 0 else f"center_obstacle_{index + 1}",
                "prop": prop,
                "at": {
                    "segment": segment_id,
                    "along_m": self._shifted_along_range(along_range, segment_range, index),
                    "offset_m": offsets[index % len(offsets)],
                    "lane": "walkway",
                },
                "yaw_deg": 0,
            }
            if explicit_blocking:
                placement["allow_blocking"] = True
            placements.append(placement)
        return placements

    def _gate_pair_placements(
        self,
        segment_id: str,
        segment_range: tuple[float, float],
        *,
        prop: str,
    ) -> list[dict[str, Any]]:
        """Return exactly one left/right gate pair for prompts that request two obstacles."""
        along_range = self._obstacle_along_range(segment_range, width_m=0.4)
        return [
            {
                "kind": "fixed",
                "id": "gate_panel_left",
                "prop": prop,
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
                "prop": prop,
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

    def _target_obstacle_segment(self, corridor: dict[str, Any]) -> tuple[str, tuple[float, float]]:
        """Return a valid segment anchor for generated fallback obstacles."""
        segments = [segment for segment in corridor.get("segments", []) if isinstance(segment, dict)]
        for preferred_id in ("conflict", "road_curve", "curve_right_1", "main"):
            for segment in segments:
                if segment.get("id") == preferred_id:
                    return str(preferred_id), self._segment_range(segment)
        if segments:
            index = min(1, len(segments) - 1)
            segment = segments[index]
            return str(segment.get("id") or "main"), self._segment_range(segment)
        return "main", (0.0, 1.0)

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

    def _shifted_along_range(
        self,
        along_range: dict[str, float],
        segment_range: tuple[float, float],
        index: int,
    ) -> dict[str, float]:
        """Return a slightly shifted along band for repeated generated obstacles."""
        if index == 0:
            return along_range
        start_m, end_m = segment_range
        shift = index * 0.25
        return {
            "min": self._rounded(min(max(start_m, along_range["min"] + shift), end_m)),
            "max": self._rounded(min(max(start_m, along_range["max"] + shift), end_m)),
        }

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
        if plan.corridor_profile == "curved":
            return "curved_road_static_obstacle" if obstacle_count else "curved_road_sidewalk"
        if plan.corridor_profile == "s-curve":
            return "long_multi_curve_open_clearance_03"
        if plan.corridor_profile == "construction":
            return "construction_fallback_static_obstacle" if obstacle_count else "construction_fallback_sidewalk"
        if plan.requested_length_m is not None:
            return f"straight_{self._length_label(plan.requested_length_m)}_sidewalk"
        return plan.scenario_id

    def _intent(self, plan: TemplatePlan, obstacle_count: int) -> str:
        """Return intent text aligned with generated fallback geometry."""
        if plan.corridor_profile == "curved":
            return (
                "Evaluate route following on a curved road sidewalk with user-requested static obstacles."
                if obstacle_count
                else "Evaluate route following on a curved road sidewalk without static obstacles."
            )
        if plan.corridor_profile == "s-curve":
            return "Evaluate route following on a long S-curve sidewalk with intent-based obstacle placement."
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
