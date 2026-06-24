from __future__ import annotations

from copy import deepcopy
import math
from typing import Any

from app.agents.scenario_generation_v2.intent_parser import ScenarioIntent
from app.agents.scenario_generation_v2.prop_normalizer import normalize_legacy_static_obstacle_prop_id
from app.catalogs.static_obstacle_catalog import get_allowed_static_obstacle_prop_ids


ALLOWED_STATIC_OBSTACLE_PROPS = get_allowed_static_obstacle_prop_ids()
"""Catalog-owned static obstacle prop ids accepted in patched preset output."""


class ScenarioPresetPatcher:
    """Applies user intent to a loaded scenario preset before validation."""

    def patch(
        self,
        preset: dict[str, Any],
        intent: ScenarioIntent,
        *,
        preset_id: str,
        source_scenario: dict[str, Any] | None = None,
    ) -> dict[str, Any]:
        """Return a patched deepcopy of the preset for validator-gated use."""
        scenario = deepcopy(preset)
        scenario["schema"] = "scenario"
        scenario["version"] = 1
        self._apply_corridor_profile(scenario, intent)
        self._apply_requested_length(scenario, intent)
        self._apply_robot_anchor_consistency(scenario, intent)
        self._apply_obstacle_intent(scenario, intent, preset_id=preset_id, source_scenario=source_scenario)
        self._apply_alpha_pedestrians(scenario)
        self._apply_scenario_identity(scenario, intent, preset_id=preset_id)
        return scenario

    def _apply_corridor_profile(self, scenario: dict[str, Any], intent: ScenarioIntent) -> None:
        """Replace preset geometry when a higher-priority road-shape intent requires it."""
        if intent.corridor_profile not in {"g-shape", "l-shape"}:
            return
        length_m = intent.requested_length_m if intent.requested_length_m is not None and intent.requested_length_m > 0 else 20.0
        scenario["corridor"] = self._g_shape_corridor(length_m)

    def _g_shape_corridor(self, length_m: float) -> dict[str, Any]:
        """Return a right-angle corridor with a construction zone before the corner."""
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

    def _apply_requested_length(self, scenario: dict[str, Any], intent: ScenarioIntent) -> None:
        """Scale corridor axis and segment along ranges when the user gives a meter length."""
        requested_length_m = intent.requested_length_m
        if requested_length_m is None or requested_length_m <= 0:
            return
        corridor = scenario.get("corridor")
        if not isinstance(corridor, dict):
            return
        points = self._axis_points(corridor)
        axis_length = self._axis_length(points)
        if axis_length > 0:
            corridor["axis"]["points_m"] = self._scaled_axis_points(points, requested_length_m / axis_length)

        max_along = self._corridor_max_along(corridor)
        if max_along <= 0:
            return
        range_factor = requested_length_m / max_along
        for segment in corridor.get("segments", []):
            if not isinstance(segment, dict):
                continue
            along_range = segment.get("along_range_m")
            if self._is_numeric_pair(along_range):
                segment["along_range_m"] = [
                    self._rounded(float(along_range[0]) * range_factor),
                    self._rounded(float(along_range[1]) * range_factor),
                ]

    def _apply_robot_anchor_consistency(self, scenario: dict[str, Any], intent: ScenarioIntent) -> None:
        """Keep robot start and goal anchors inside the patched corridor ranges."""
        if intent.robot_anchor_only and intent.robot_start_anchor is not None and intent.robot_goal_anchor is not None:
            scenario["robot"] = {"start": deepcopy(intent.robot_start_anchor), "goal": deepcopy(intent.robot_goal_anchor)}
            return
        corridor = scenario.get("corridor")
        if not isinstance(corridor, dict):
            return
        segments = [segment for segment in corridor.get("segments", []) if isinstance(segment, dict)]
        if not segments:
            return
        first_segment = segments[0]
        last_segment = segments[-1]
        scenario["robot"] = {
            "start": self._corridor_pose_anchor(first_segment, prefer_start=True),
            "goal": self._corridor_pose_anchor(last_segment, prefer_start=False),
        }

    def _apply_obstacle_intent(
        self,
        scenario: dict[str, Any],
        intent: ScenarioIntent,
        *,
        preset_id: str,
        source_scenario: dict[str, Any] | None,
    ) -> None:
        """Patch obstacle placements so explicit user obstacle constraints win over presets."""
        obstacles = scenario.setdefault("obstacles", {})
        if not isinstance(obstacles, dict):
            scenario["obstacles"] = {"min_clear_width_m": 0.9, "placements": []}
            return
        preset_placements = self._placements_from(scenario)
        source_placements = self._placements_from(source_scenario)
        desired_count = self._desired_obstacle_count(intent, preset_id=preset_id, preset_placements=preset_placements)
        min_clear_width = self._min_clear_width(source_scenario, scenario)
        if desired_count <= 0:
            scenario["obstacles"] = {"min_clear_width_m": min_clear_width, "placements": []}
            return

        placement_source = source_placements if source_placements and self._has_static_obstacle_intent(intent) else preset_placements
        placements = [deepcopy(placement) for placement in placement_source[:desired_count]]
        while len(placements) < desired_count:
            placements.append(self._default_obstacle_placement(scenario, preset_id=preset_id, index=len(placements), intent=intent))
        normalized = [
            self._normalize_obstacle_placement(placement, scenario, preset_id=preset_id, intent=intent)
            for placement in placements
        ]
        normalized = self._redistribute_requested_obstacles(normalized, scenario, preset_id=preset_id, intent=intent)
        scenario["obstacles"] = {
            "min_clear_width_m": min_clear_width,
            "placements": self._dedupe_placement_ids(normalized, preset_id=preset_id),
        }

    def _apply_alpha_pedestrians(self, scenario: dict[str, Any]) -> None:
        """Keep alpha scenario generation output free of pedestrian runtime actors."""
        scenario["pedestrians"] = {"background": {"count": 0, "speed_mps": 1.0}, "encounters": []}

    def _apply_scenario_identity(self, scenario: dict[str, Any], intent: ScenarioIntent, *, preset_id: str) -> None:
        """Adjust ids and intent text when patching changes the preset semantics."""
        placement_count = len(self._placements_from(scenario))
        if intent.corridor_profile in {"g-shape", "l-shape"}:
            scenario["scenario_id"] = (
                "g_shape_pre_corner_construction_cones" if placement_count else "g_shape_corridor_navigation"
            )
            scenario["intent"] = (
                "Evaluate right-angle corridor navigation with pre-corner construction cone placement."
                if placement_count
                else "Evaluate right-angle corridor navigation without static obstacles."
            )
        elif preset_id == "curved":
            scenario["scenario_id"] = "curved_road_static_obstacle" if placement_count else "curved_road_sidewalk"
            scenario["intent"] = (
                "Evaluate route following on a curved road sidewalk with user-requested static obstacles."
                if placement_count
                else "Evaluate route following on a curved road sidewalk without static obstacles."
            )
        elif preset_id == "line" and intent.requested_length_m is not None:
            label = self._length_label(intent.requested_length_m)
            scenario["scenario_id"] = f"straight_{label}_sidewalk"
            scenario["intent"] = f"Evaluate straight sidewalk navigation over {intent.requested_length_m:g}m."
        elif preset_id == "line" and placement_count == 0:
            scenario["scenario_id"] = "line_sidewalk"
            scenario["intent"] = "Evaluate straight sidewalk navigation without static obstacles."
        elif preset_id == "s-curve" and placement_count == 0:
            scenario["intent"] = "Evaluate route following on a long S-curve sidewalk without static obstacles."

    def _axis_points(self, corridor: dict[str, Any]) -> list[list[float]]:
        """Return numeric corridor polyline points or an empty list."""
        axis = corridor.get("axis")
        points = axis.get("points_m") if isinstance(axis, dict) else None
        if not isinstance(points, list):
            return []
        numeric_points: list[list[float]] = []
        for point in points:
            if (
                isinstance(point, list)
                and len(point) >= 2
                and isinstance(point[0], int | float)
                and isinstance(point[1], int | float)
            ):
                numeric_points.append([float(point[0]), float(point[1])])
        return numeric_points if len(numeric_points) == len(points) else []

    def _axis_length(self, points: list[list[float]]) -> float:
        """Return the total 2D length of a polyline."""
        if len(points) < 2:
            return 0.0
        total = 0.0
        for previous, current in zip(points, points[1:], strict=False):
            total += math.dist(previous[:2], current[:2])
        return total

    def _scaled_axis_points(self, points: list[list[float]], factor: float) -> list[list[float]]:
        """Scale axis points around the first point by a uniform factor."""
        origin_x, origin_y = points[0]
        return [
            [
                self._rounded(origin_x + (point[0] - origin_x) * factor),
                self._rounded(origin_y + (point[1] - origin_y) * factor),
            ]
            for point in points
        ]

    def _corridor_max_along(self, corridor: dict[str, Any]) -> float:
        """Return the maximum segment along value used by robot and obstacle anchors."""
        max_along = 0.0
        for segment in corridor.get("segments", []):
            if not isinstance(segment, dict):
                continue
            along_range = segment.get("along_range_m")
            if self._is_numeric_pair(along_range):
                max_along = max(max_along, float(along_range[1]))
        return max_along

    def _corridor_pose_anchor(self, segment: dict[str, Any], *, prefer_start: bool) -> dict[str, Any]:
        """Build a corridor pose anchor inside a segment's along range."""
        segment_id = str(segment.get("id") or "main")
        along_range = segment.get("along_range_m")
        start_m, end_m = self._numeric_pair_or_default(along_range, (0.0, 1.0))
        span = max(0.0, end_m - start_m)
        inner_offset = min(1.0, max(0.0, span * 0.25))
        if span <= 4.0:
            inner_offset = min(0.5, max(0.0, span * 0.125))
        along_m = start_m + inner_offset if prefer_start else end_m - inner_offset
        return {
            "type": "corridor_pose",
            "segment": segment_id,
            "along_m": self._rounded(along_m),
            "offset_m": 0.0,
            "lane": "walkway",
            "heading": "forward",
        }

    def _desired_obstacle_count(
        self,
        intent: ScenarioIntent,
        *,
        preset_id: str,
        preset_placements: list[dict[str, Any]],
    ) -> int:
        """Return the obstacle count after applying user intent over preset defaults."""
        if intent.robot_anchor_only or intent.explicit_no_obstacles:
            return 0
        if intent.requested_obstacle_count is not None:
            return max(0, int(intent.requested_obstacle_count))
        if preset_id == "barricade":
            return len(preset_placements)
        if self._has_static_obstacle_intent(intent):
            return 1
        return 0

    def _has_static_obstacle_intent(self, intent: ScenarioIntent) -> bool:
        """Return whether user intent asks for at least one static obstacle."""
        return (
            not intent.explicit_no_obstacles
            and ("static_obstacle_ahead" in intent.risk_factors or intent.requested_gate_obstacle_count == 2)
        )

    def _placements_from(self, scenario: dict[str, Any] | None) -> list[dict[str, Any]]:
        """Return dictionary obstacle placements from a scenario-like object."""
        if not isinstance(scenario, dict):
            return []
        obstacles = scenario.get("obstacles")
        if not isinstance(obstacles, dict):
            return []
        placements = obstacles.get("placements")
        if not isinstance(placements, list):
            return []
        return [placement for placement in placements if isinstance(placement, dict)]

    def _min_clear_width(self, source_scenario: dict[str, Any] | None, scenario: dict[str, Any]) -> object:
        """Prefer source min-clear-width, then preset width, then a valid default."""
        for candidate in (source_scenario, scenario):
            if not isinstance(candidate, dict):
                continue
            obstacles = candidate.get("obstacles")
            if isinstance(obstacles, dict) and obstacles.get("min_clear_width_m") is not None:
                return deepcopy(obstacles["min_clear_width_m"])
        return 0.9

    def _default_obstacle_placement(
        self,
        scenario: dict[str, Any],
        *,
        preset_id: str,
        index: int,
        intent: ScenarioIntent,
    ) -> dict[str, Any]:
        """Create a validator-safe fixed obstacle when the source has too few placements."""
        segment_id, segment_range = self._target_segment(scenario, preset_id=preset_id, requested_segment=None)
        placement: dict[str, Any] = {
            "kind": "fixed",
            "id": f"{preset_id.replace('-', '_')}_obstacle_{index + 1}",
            "prop": self._requested_catalog_prop(intent) or "obstacle.road_cone_01",
            "at": {
                "segment": segment_id,
                "along_m": self._default_along_range(segment_range, preset_id=preset_id),
                "offset_m": {"min": -0.25, "max": 0.05} if index % 2 == 0 else {"min": 0.15, "max": 0.45},
                "lane": "center",
            },
            "yaw_deg": 0,
            "allow_blocking": bool(intent.explicit_blocking),
        }
        return placement

    def _normalize_obstacle_placement(
        self,
        placement: dict[str, Any],
        scenario: dict[str, Any],
        *,
        preset_id: str,
        intent: ScenarioIntent,
    ) -> dict[str, Any]:
        """Normalize placement anchors so patched presets validate against their corridor."""
        normalized = deepcopy(placement)
        kind = normalized.get("kind")
        if kind in {"fixed", "pattern"}:
            self._normalize_legacy_prop(normalized)
            requested_prop = self._requested_catalog_prop(intent)
            if requested_prop is not None:
                normalized["prop"] = requested_prop
            at = normalized.get("at")
            if not isinstance(at, dict):
                at = {}
            requested_segment = at.get("segment") if isinstance(at.get("segment"), str) else None
            segment_id, segment_range = self._target_segment(scenario, preset_id=preset_id, requested_segment=requested_segment)
            at["segment"] = segment_id
            at["along_m"] = self._normalized_along_value(at.get("along_m"), segment_range, preset_id=preset_id)
            at.setdefault("offset_m", 0.0)
            at.setdefault("lane", "walkway")
            normalized["at"] = at
        elif kind == "scatter":
            segment_id, _ = self._target_segment(scenario, preset_id=preset_id, requested_segment=None)
            zone = normalized.get("zone")
            if not isinstance(zone, dict):
                zone = {}
            zone["segments"] = [segment_id]
            normalized["zone"] = zone
        if intent.explicit_blocking:
            normalized["allow_blocking"] = True
        return normalized

    def _normalize_legacy_prop(self, placement: dict[str, Any]) -> None:
        """Replace known legacy preset prop ids with catalog-owned prop ids."""
        if "prop" in placement:
            placement["prop"] = normalize_legacy_static_obstacle_prop_id(placement["prop"])

    def _requested_catalog_prop(self, intent: ScenarioIntent) -> str | None:
        """Return a requested prop only when it is present in the static obstacle catalog."""
        prop = normalize_legacy_static_obstacle_prop_id(intent.requested_prop)
        if isinstance(prop, str) and prop in ALLOWED_STATIC_OBSTACLE_PROPS:
            return prop
        return None

    def _redistribute_requested_obstacles(
        self,
        placements: list[dict[str, Any]],
        scenario: dict[str, Any],
        *,
        preset_id: str,
        intent: ScenarioIntent,
    ) -> list[dict[str, Any]]:
        """Distribute explicitly requested repeated obstacles across the target segment."""
        if len(placements) <= 1:
            return placements
        if intent.requested_obstacle_count is None and intent.corridor_profile not in {"g-shape", "l-shape"}:
            return placements
        segment_id, segment_range = self._target_segment(scenario, preset_id=preset_id, requested_segment=None)
        requested_prop = self._requested_catalog_prop(intent)
        redistributed: list[dict[str, Any]] = []
        for index, placement in enumerate(placements):
            normalized = deepcopy(placement)
            if requested_prop is not None:
                normalized["prop"] = requested_prop
            normalized["id"] = self._placement_id(normalized, preset_id=preset_id, index=index)
            at = normalized.get("at") if isinstance(normalized.get("at"), dict) else {}
            at["segment"] = segment_id
            at["along_m"] = self._distributed_along_range(segment_range, index, len(placements))
            at["offset_m"] = self._offset_range(self._zigzag_offset(index))
            at.setdefault("lane", "walkway")
            normalized["at"] = at
            normalized["allow_blocking"] = bool(intent.explicit_blocking)
            redistributed.append(normalized)
        return redistributed

    def _placement_id(self, placement: dict[str, Any], *, preset_id: str, index: int) -> str:
        """Return a stable id for redistributed obstacle placements."""
        prop = placement.get("prop")
        if prop == "obstacle.road_cone_01":
            return f"cone_{index + 1:02d}"
        placement_id = placement.get("id")
        if isinstance(placement_id, str) and placement_id:
            return placement_id
        return f"{preset_id.replace('-', '_')}_obstacle_{index + 1}"

    def _distributed_along_range(
        self,
        segment_range: tuple[float, float],
        index: int,
        count: int,
    ) -> dict[str, float]:
        """Return a small along band spaced apart from other requested obstacles."""
        minimum, maximum = segment_range
        span = max(0.0, maximum - minimum)
        center = minimum + span * ((index + 1) / (max(1, count) + 1))
        half_width = min(0.12, max(0.05, span / (max(1, count) + 1) * 0.15))
        return {
            "min": self._rounded(self._clamp(center - half_width, minimum, maximum)),
            "max": self._rounded(self._clamp(center + half_width, minimum, maximum)),
        }

    def _zigzag_offset(self, index: int) -> float:
        """Return alternating left/center/right offsets for repeated obstacles."""
        pattern = [-0.35, 0.25, -0.15, 0.35, -0.25, 0.15]
        return pattern[index % len(pattern)]

    def _offset_range(self, center_m: float) -> dict[str, float]:
        """Return a narrow offset band around a zigzag center value."""
        return {"min": self._rounded(center_m - 0.05), "max": self._rounded(center_m + 0.05)}

    def _dedupe_placement_ids(self, placements: list[dict[str, Any]], *, preset_id: str) -> list[dict[str, Any]]:
        """Ensure patched placement ids stay unique after trimming or expanding a preset."""
        seen: set[str] = set()
        for index, placement in enumerate(placements):
            placement_id = placement.get("id")
            if not isinstance(placement_id, str) or not placement_id or placement_id in seen:
                placement_id = f"{preset_id.replace('-', '_')}_obstacle_{index + 1}"
                placement["id"] = placement_id
            seen.add(placement_id)
        return placements

    def _target_segment(
        self,
        scenario: dict[str, Any],
        *,
        preset_id: str,
        requested_segment: str | None,
    ) -> tuple[str, tuple[float, float]]:
        """Return the segment id and range to use for patched obstacle anchors."""
        ranges = self._segment_ranges(scenario)
        if "pre_corner_construction" in ranges:
            return "pre_corner_construction", ranges["pre_corner_construction"]
        if preset_id == "curved" and "road_curve" in ranges:
            return "road_curve", ranges["road_curve"]
        if preset_id == "s-curve":
            for segment_id in ("curve_right_1", "curve_left_2", "curve_left_1", "curve_right_2"):
                if segment_id in ranges:
                    return segment_id, ranges[segment_id]
        if requested_segment in ranges:
            return requested_segment, ranges[requested_segment]
        if "conflict" in ranges:
            return "conflict", ranges["conflict"]
        if ranges:
            first_id = next(iter(ranges))
            return first_id, ranges[first_id]
        return "main", (0.0, 1.0)

    def _segment_ranges(self, scenario: dict[str, Any]) -> dict[str, tuple[float, float]]:
        """Return validated numeric segment ranges keyed by segment id."""
        corridor = scenario.get("corridor")
        if not isinstance(corridor, dict):
            return {}
        ranges: dict[str, tuple[float, float]] = {}
        for segment in corridor.get("segments", []):
            if not isinstance(segment, dict) or not isinstance(segment.get("id"), str):
                continue
            along_range = segment.get("along_range_m")
            if self._is_numeric_pair(along_range):
                ranges[segment["id"]] = (float(along_range[0]), float(along_range[1]))
        return ranges

    def _normalized_along_value(
        self,
        value: object,
        segment_range: tuple[float, float],
        *,
        preset_id: str,
    ) -> object:
        """Clamp or default an obstacle along value into its target segment."""
        if preset_id == "curved":
            return self._curved_along_value(value, segment_range)
        minimum, maximum = segment_range
        if isinstance(value, dict):
            value_min = value.get("min")
            value_max = value.get("max")
            if isinstance(value_min, int | float) and isinstance(value_max, int | float):
                clamped_min = self._clamp(float(value_min), minimum, maximum)
                clamped_max = self._clamp(float(value_max), minimum, maximum)
                if clamped_min < clamped_max:
                    return {"min": self._rounded(clamped_min), "max": self._rounded(clamped_max)}
        if isinstance(value, int | float):
            return self._rounded(self._clamp(float(value), minimum, maximum))
        return self._default_along_range(segment_range, preset_id=preset_id)

    def _curved_along_value(self, value: object, segment_range: tuple[float, float]) -> object:
        """Preserve valid curved-road obstacle ranges while avoiding collapsed edge ranges."""
        minimum, maximum = segment_range
        if isinstance(value, dict):
            value_min = value.get("min")
            value_max = value.get("max")
            if isinstance(value_min, int | float) and isinstance(value_max, int | float):
                clamped_min = self._clamp(float(value_min), minimum, maximum)
                clamped_max = self._clamp(float(value_max), minimum, maximum)
                if clamped_min < clamped_max:
                    return {"min": self._rounded(clamped_min), "max": self._rounded(clamped_max)}
                if clamped_min == clamped_max and clamped_min not in {minimum, maximum}:
                    return {"min": self._rounded(clamped_min), "max": self._rounded(clamped_max)}
        if isinstance(value, int | float):
            return self._rounded(self._clamp(float(value), minimum, maximum))
        return self._default_along_range(segment_range, preset_id="curved")

    def _default_along_range(self, segment_range: tuple[float, float], *, preset_id: str) -> dict[str, float]:
        """Return a stable default along band within the target segment."""
        if preset_id == "curved":
            return {"min": 6.5, "max": 8.5}
        minimum, maximum = segment_range
        span = max(0.0, maximum - minimum)
        center = minimum + span * 0.5
        half_width = min(0.3, max(0.1, span * 0.05))
        return {
            "min": self._rounded(self._clamp(center - half_width, minimum, maximum)),
            "max": self._rounded(self._clamp(center + half_width, minimum, maximum)),
        }

    def _is_numeric_pair(self, value: object) -> bool:
        """Return whether a value is a two-number increasing pair."""
        return (
            isinstance(value, list)
            and len(value) == 2
            and isinstance(value[0], int | float)
            and isinstance(value[1], int | float)
            and float(value[0]) <= float(value[1])
        )

    def _numeric_pair_or_default(self, value: object, default: tuple[float, float]) -> tuple[float, float]:
        """Return a numeric pair or a caller-provided fallback."""
        if self._is_numeric_pair(value):
            return float(value[0]), float(value[1])
        return default

    def _length_label(self, length_m: float) -> str:
        """Return a snake-case-safe meter label for scenario ids."""
        if float(length_m).is_integer():
            return f"{int(length_m)}m"
        return f"{str(length_m).replace('.', '_')}m"

    def _clamp(self, value: float, minimum: float, maximum: float) -> float:
        """Clamp a number to an inclusive range."""
        return min(max(value, minimum), maximum)

    def _rounded(self, value: float) -> float:
        """Round generated geometry values to stable millimeter precision."""
        return round(float(value), 3)
