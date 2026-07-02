from __future__ import annotations

import re
from typing import Any

from app.catalogs.static_obstacle_catalog import get_allowed_static_obstacle_prop_ids
from app.models.scenario_generation_v2 import V2ValidationIssue, V2ValidationResult


ALLOWED_SURFACES = {"sidewalk", "crosswalk_stripe", "grass", "road", "driveway", "wall", "building"}
ALLOWED_PROPS = get_allowed_static_obstacle_prop_ids()
ALLOWED_SEGMENT_TYPES = {"straight"}
ALLOWED_PLACEMENT_KINDS = {"fixed"}
ALLOWED_LANES = {"walkway", "building_edge", "center", "curb_edge"}
FORBIDDEN_ROOT_FIELDS = {
    "base_seed",
    "center_xy_m",
    "episode",
    "episode_count",
    "experiment_id",
    "generation_mode",
    "generated_count",
    "analysis",
    "analysis_result",
    "params",
    "policy",
    "project_path",
    "radius_m",
    "robot_setup",
    "route",
    "run_id",
    "sample_count",
    "sample_id",
    "scenario_path",
    "scenario_sample",
    "scenario_template",
    "semantic",
    "seed",
    "source",
    "template_hash",
    "template_id",
    "template_path",
    "ue_payload",
    "validation",
    "world_xy",
    "pedestrians",
}


class TemplateValidator:
    """Deterministically validates current project scenario v1 objects."""

    def validate(self, template: dict[str, Any]) -> V2ValidationResult:
        """Return catalog and structure diagnostics without invoking an LLM."""
        errors: list[V2ValidationIssue] = []
        warnings: list[V2ValidationIssue] = []
        if not isinstance(template, dict):
            return V2ValidationResult(valid=False, errors=[V2ValidationIssue(field="scenario", message="scenario는 dict여야 합니다.")], warnings=[])

        self._validate_root(template, errors)
        corridor = template.get("corridor")
        segment_ids = self._validate_corridor(corridor, errors)
        segment_ranges = self._segment_ranges(corridor)
        self._validate_obstacles(template.get("obstacles"), segment_ids, segment_ranges, errors, warnings)
        self._validate_robot(template.get("robot"), segment_ranges, segment_ids, errors)
        self._check_ranges(template, errors)

        return V2ValidationResult(valid=not errors, errors=errors, warnings=warnings)

    def _validate_root(self, template: dict[str, Any], errors: list[V2ValidationIssue]) -> None:
        """Validate required root fields and reject obsolete generation ownership fields."""
        if template.get("schema") != "scenario":
            errors.append(V2ValidationIssue(field="schema", message="schema는 scenario이어야 합니다."))
        if template.get("version") != 1:
            errors.append(V2ValidationIssue(field="version", message="version은 1이어야 합니다."))
        scenario_id = template.get("scenario_id")
        if not isinstance(scenario_id, str) or not scenario_id:
            errors.append(V2ValidationIssue(field="scenario_id", message="scenario_id가 필요합니다."))
        elif not re.fullmatch(r"[a-z0-9]+(?:_[a-z0-9]+)*", scenario_id):
            errors.append(V2ValidationIssue(field="scenario_id", message="scenario_id는 snake_case여야 합니다."))
        if not isinstance(template.get("intent"), str) or not template["intent"].strip():
            errors.append(V2ValidationIssue(field="intent", message="intent가 필요합니다."))
        if "corridor" not in template:
            errors.append(V2ValidationIssue(field="corridor", message="corridor가 필요합니다."))
        if "robot" not in template:
            errors.append(V2ValidationIssue(field="robot", message="robot이 필요합니다."))
        for field in sorted(FORBIDDEN_ROOT_FIELDS):
            if field in template:
                errors.append(V2ValidationIssue(field=field, message="scenario root에 포함할 수 없는 필드입니다."))
        if "ground_model" in template:
            errors.append(V2ValidationIssue(field="ground_model", message="ground_model 대신 corridor를 사용해야 합니다."))
        if "static_obstacles" in template:
            errors.append(V2ValidationIssue(field="static_obstacles", message="static_obstacles 대신 obstacles.placements를 사용해야 합니다."))

    def _validate_corridor(self, corridor: Any, errors: list[V2ValidationIssue]) -> set[str]:
        """Validate corridor axis, width, surfaces, and segment definitions."""
        segment_ids: set[str] = set()
        if not isinstance(corridor, dict):
            errors.append(V2ValidationIssue(field="corridor", message="corridor는 object여야 합니다."))
            return segment_ids

        axis = corridor.get("axis")
        if not isinstance(axis, dict) or axis.get("type") != "polyline":
            errors.append(V2ValidationIssue(field="corridor.axis.type", message="axis.type은 polyline이어야 합니다."))
        points = axis.get("points_m") if isinstance(axis, dict) else None
        if not isinstance(points, list) or len(points) < 2:
            errors.append(V2ValidationIssue(field="corridor.axis.points_m", message="points_m은 2개 이상의 점을 가져야 합니다."))

        self._validate_range(corridor.get("walkway_width_m"), "corridor.walkway_width_m", errors)
        self._validate_surface_list(corridor.get("building_side", []), "corridor.building_side", errors, expected_surface="building")
        self._validate_surface_list(corridor.get("curb_side", []), "corridor.curb_side", errors, expected_surface="road")

        segments = corridor.get("segments")
        if not isinstance(segments, list) or not segments:
            errors.append(V2ValidationIssue(field="corridor.segments", message="segments가 필요합니다."))
            return segment_ids
        for index, segment in enumerate(segments):
            field = f"corridor.segments[{index}]"
            if not isinstance(segment, dict):
                errors.append(V2ValidationIssue(field=field, message="segment는 object여야 합니다."))
                continue
            segment_id = segment.get("id")
            if not isinstance(segment_id, str) or not segment_id:
                errors.append(V2ValidationIssue(field=f"{field}.id", message="segment id가 필요합니다."))
            elif segment_id in segment_ids:
                errors.append(V2ValidationIssue(field=f"{field}.id", message="segment id는 중복될 수 없습니다."))
            else:
                segment_ids.add(segment_id)
            if segment.get("type") not in ALLOWED_SEGMENT_TYPES:
                errors.append(V2ValidationIssue(field=f"{field}.type", message="지원하지 않는 segment type입니다."))
            along_range = segment.get("along_range_m")
            if not self._is_ordered_pair(along_range):
                errors.append(V2ValidationIssue(field=f"{field}.along_range_m", message="along_range_m은 증가하는 숫자 2개여야 합니다."))
            self._validate_replaced_by(segment.get("replaced_by"), f"{field}.replaced_by", errors)
        return segment_ids

    def _validate_obstacles(
        self,
        obstacles: Any,
        segment_ids: set[str],
        segment_ranges: dict[str, tuple[float, float]],
        errors: list[V2ValidationIssue],
        warnings: list[V2ValidationIssue],
    ) -> None:
        """Validate static obstacle placement rules and prop catalog use."""
        if obstacles is None:
            return
        if not isinstance(obstacles, dict):
            errors.append(V2ValidationIssue(field="obstacles", message="obstacles는 object여야 합니다."))
            return
        min_clear_width = obstacles.get("min_clear_width_m")
        self._validate_number_or_range(min_clear_width, "obstacles.min_clear_width_m", errors)
        if isinstance(min_clear_width, int | float) and min_clear_width < 0.8:
            warnings.append(V2ValidationIssue(field="obstacles.min_clear_width_m", message="최소 통로 폭이 작습니다."))
        placements = obstacles.get("placements")
        if not isinstance(placements, list):
            errors.append(V2ValidationIssue(field="obstacles.placements", message="placements는 list여야 합니다."))
            return
        seen: set[str] = set()
        for index, placement in enumerate(placements):
            field = f"obstacles.placements[{index}]"
            if not isinstance(placement, dict):
                errors.append(V2ValidationIssue(field=field, message="placement는 object여야 합니다."))
                continue
            placement_id = placement.get("id")
            if not isinstance(placement_id, str) or not placement_id:
                errors.append(V2ValidationIssue(field=f"{field}.id", message="placement id가 필요합니다."))
            elif placement_id in seen:
                errors.append(V2ValidationIssue(field=f"{field}.id", message="placement id는 중복될 수 없습니다."))
            else:
                seen.add(placement_id)
            for direct_field in ("segment", "along_m", "offset_m", "lane"):
                if direct_field in placement:
                    errors.append(
                        V2ValidationIssue(
                            field=f"{field}.{direct_field}",
                            message="placement pose는 at object 안에 포함해야 합니다.",
                        )
                    )
            if placement.get("kind") not in ALLOWED_PLACEMENT_KINDS:
                errors.append(V2ValidationIssue(field=f"{field}.kind", message="지원하지 않는 placement kind입니다."))
                continue
            for legacy_field in ("pattern", "count", "spacing_m", "gap_width_m", "zone", "density_per_10m", "palette"):
                if legacy_field in placement:
                    errors.append(
                        V2ValidationIssue(
                            field=f"{field}.{legacy_field}",
                            message="현재 scenario authoring surface에서 지원하지 않는 placement field입니다.",
                        )
                    )
            self._validate_fixed_placement(placement, field, segment_ids, segment_ranges, errors)
            allow_blocking = placement.get("allow_blocking")
            if allow_blocking is not None and not isinstance(allow_blocking, bool):
                errors.append(V2ValidationIssue(field=f"{field}.allow_blocking", message="allow_blocking은 boolean이어야 합니다."))

    def _validate_fixed_placement(
        self,
        placement: dict[str, Any],
        field: str,
        segment_ids: set[str],
        segment_ranges: dict[str, tuple[float, float]],
        errors: list[V2ValidationIssue],
    ) -> None:
        """Validate one fixed static obstacle placement."""
        prop = placement.get("prop")
        if prop is None:
            errors.append(V2ValidationIssue(field=f"{field}.prop", message="prop가 필요합니다."))
        elif not isinstance(prop, str) or prop not in ALLOWED_PROPS:
            errors.append(V2ValidationIssue(field=f"{field}.prop", message="catalog에 없는 prop입니다."))
        at = placement.get("at")
        if not isinstance(at, dict):
            errors.append(V2ValidationIssue(field=f"{field}.at", message="at object가 필요합니다."))
        else:
            self._validate_placement_anchor(at, f"{field}.at", segment_ids, segment_ranges, errors)
        if "yaw_deg" in placement:
            self._validate_number_or_range(placement.get("yaw_deg"), f"{field}.yaw_deg", errors)

    def _validate_placement_anchor(
        self,
        at: dict[str, Any],
        field: str,
        segment_ids: set[str],
        segment_ranges: dict[str, tuple[float, float]],
        errors: list[V2ValidationIssue],
    ) -> None:
        """Validate corridor-local obstacle anchor fields."""
        segment = at.get("segment")
        if segment is None:
            errors.append(V2ValidationIssue(field=f"{field}.segment", message="segment가 필요합니다."))
        elif segment not in segment_ids:
            errors.append(V2ValidationIssue(field=f"{field}.segment", message="존재하지 않는 segment 참조입니다."))
        if "along_m" not in at:
            errors.append(V2ValidationIssue(field=f"{field}.along_m", message="along_m이 필요합니다."))
            along_bounds = None
        else:
            along_m = at.get("along_m")
            self._validate_number_or_range(along_m, f"{field}.along_m", errors)
            along_bounds = self._numeric_bounds(along_m)
        if along_bounds is not None and segment in segment_ranges:
            minimum, maximum = segment_ranges[segment]
            if along_bounds[0] < minimum or along_bounds[1] > maximum:
                errors.append(V2ValidationIssue(field=f"{field}.along_m", message="along_m이 segment 범위를 벗어났습니다."))
        if "offset_m" not in at:
            errors.append(V2ValidationIssue(field=f"{field}.offset_m", message="offset_m이 필요합니다."))
        else:
            self._validate_number_or_range(at.get("offset_m"), f"{field}.offset_m", errors)
        lane = at.get("lane")
        if lane is not None and (not isinstance(lane, str) or lane not in ALLOWED_LANES):
            errors.append(V2ValidationIssue(field=f"{field}.lane", message="지원하지 않는 lane hint입니다."))

    def _validate_robot(
        self,
        robot: Any,
        segment_ranges: dict[str, tuple[float, float]],
        segment_ids: set[str],
        errors: list[V2ValidationIssue],
    ) -> None:
        """Validate robot anchors while keeping positions abstract by default."""
        if not isinstance(robot, dict):
            errors.append(V2ValidationIssue(field="robot", message="robot은 object여야 합니다."))
            return
        if "setup" in robot:
            errors.append(V2ValidationIssue(field="robot.setup", message="robot setup 세부값은 scenario에 포함할 수 없습니다."))
        for key in ("start", "goal"):
            anchor = robot.get(key)
            field = f"robot.{key}"
            if not isinstance(anchor, dict):
                errors.append(V2ValidationIssue(field=field, message="robot anchor가 필요합니다."))
                continue
            anchor_type = anchor.get("type")
            if not isinstance(anchor_type, str) or anchor_type not in {"entry", "exit", "corridor_pose"}:
                errors.append(V2ValidationIssue(field=f"{field}.type", message="지원하지 않는 robot anchor type입니다."))
                continue
            if anchor_type in {"entry", "exit"}:
                for concrete_field in ("segment", "along_m", "offset_m", "lane", "heading"):
                    if concrete_field in anchor:
                        errors.append(
                            V2ValidationIssue(
                                field=f"{field}.{concrete_field}",
                                message="entry/exit anchor에는 concrete pose field를 포함할 수 없습니다.",
                            )
                        )
                continue
            if anchor_type == "corridor_pose":
                segment = anchor.get("segment")
                if segment is None:
                    errors.append(V2ValidationIssue(field=f"{field}.segment", message="segment가 필요합니다."))
                elif segment not in segment_ids:
                    errors.append(V2ValidationIssue(field=f"{field}.segment", message="존재하지 않는 segment 참조입니다."))
                if "along_m" not in anchor:
                    errors.append(V2ValidationIssue(field=f"{field}.along_m", message="along_m이 필요합니다."))
                    along_m = None
                else:
                    along_m = anchor.get("along_m")
                    self._validate_number_or_range(along_m, f"{field}.along_m", errors)
                if "offset_m" not in anchor:
                    errors.append(V2ValidationIssue(field=f"{field}.offset_m", message="offset_m이 필요합니다."))
                else:
                    self._validate_number_or_range(anchor.get("offset_m"), f"{field}.offset_m", errors)
                along_bounds = self._numeric_bounds(along_m)
                if along_bounds is not None and segment in segment_ranges:
                    minimum, maximum = segment_ranges[segment]
                    if along_bounds[0] < minimum or along_bounds[1] > maximum:
                        errors.append(V2ValidationIssue(field=f"{field}.along_m", message="along_m이 segment 범위를 벗어났습니다."))
                heading = anchor.get("heading")
                if heading is not None and (not isinstance(heading, str) or heading not in {"forward", "backward", "auto"}):
                    errors.append(V2ValidationIssue(field=f"{field}.heading", message="지원하지 않는 heading입니다."))
                lane = anchor.get("lane")
                if lane is not None and (not isinstance(lane, str) or lane not in ALLOWED_LANES):
                    errors.append(V2ValidationIssue(field=f"{field}.lane", message="지원하지 않는 lane hint입니다."))

    def _validate_surface_list(
        self,
        lanes: Any,
        field: str,
        errors: list[V2ValidationIssue],
        *,
        expected_surface: str,
    ) -> None:
        """Validate surface ids in corridor side lane arrays."""
        if not isinstance(lanes, list):
            errors.append(V2ValidationIssue(field=field, message="surface 목록은 list여야 합니다."))
            return
        for index, lane in enumerate(lanes):
            if not isinstance(lane, dict):
                errors.append(V2ValidationIssue(field=f"{field}[{index}]", message="surface entry는 object여야 합니다."))
                continue
            surface = lane.get("surface")
            if not isinstance(surface, str) or surface not in ALLOWED_SURFACES:
                errors.append(V2ValidationIssue(field=f"{field}[{index}].surface", message="catalog에 없는 surface입니다."))
            elif surface != expected_surface:
                errors.append(V2ValidationIssue(field=f"{field}[{index}].surface", message="허용된 side surface가 아닙니다."))
            self._validate_number_or_range(lane.get("width_m"), f"{field}[{index}].width_m", errors)

    def _validate_range(self, value: Any, field: str, errors: list[V2ValidationIssue]) -> None:
        """Validate a fixed number or min/max range object."""
        self._validate_number_or_range(value, field, errors)

    def _validate_number_or_range(self, value: Any, field: str, errors: list[V2ValidationIssue]) -> None:
        """Validate a fixed number or min/max numeric range."""
        if isinstance(value, int | float):
            return
        if isinstance(value, dict):
            minimum = value.get("min")
            maximum = value.get("max")
            if isinstance(minimum, int | float) and isinstance(maximum, int | float) and minimum <= maximum:
                return
        errors.append(V2ValidationIssue(field=field, message="값은 숫자 또는 min/max 범위여야 합니다."))

    def _validate_integer_or_range(self, value: Any, field: str, errors: list[V2ValidationIssue]) -> None:
        """Validate a fixed integer or integer min/max range."""
        if isinstance(value, bool):
            errors.append(V2ValidationIssue(field=field, message="값은 정수 또는 정수 min/max 범위여야 합니다."))
            return
        if isinstance(value, int):
            return
        if isinstance(value, dict):
            minimum = value.get("min")
            maximum = value.get("max")
            if (
                isinstance(minimum, int)
                and not isinstance(minimum, bool)
                and isinstance(maximum, int)
                and not isinstance(maximum, bool)
                and minimum <= maximum
            ):
                return
        errors.append(V2ValidationIssue(field=field, message="값은 정수 또는 정수 min/max 범위여야 합니다."))

    def _validate_replaced_by(self, replaced_by: Any, field: str, errors: list[V2ValidationIssue]) -> None:
        """Validate the only v1 field that currently accepts string choices."""
        if replaced_by is None:
            return
        if isinstance(replaced_by, str):
            return
        if isinstance(replaced_by, dict):
            choices = replaced_by.get("choices")
            if isinstance(choices, list) and choices and all(isinstance(choice, str) for choice in choices):
                return
        errors.append(V2ValidationIssue(field=field, message="replaced_by는 string 또는 choices string list여야 합니다."))

    def _numeric_bounds(self, value: Any) -> tuple[float, float] | None:
        """Return comparable numeric bounds for a fixed number or min/max range."""
        if isinstance(value, int | float):
            numeric = float(value)
            return (numeric, numeric)
        if isinstance(value, dict):
            minimum = value.get("min")
            maximum = value.get("max")
            if isinstance(minimum, int | float) and isinstance(maximum, int | float) and minimum <= maximum:
                return (float(minimum), float(maximum))
        return None

    def _check_ranges(self, value: Any, errors: list[V2ValidationIssue], path: str = "") -> None:
        """Recursively catch inverted numeric range objects."""
        if isinstance(value, dict):
            if set(value) >= {"min", "max"}:
                minimum = value.get("min")
                maximum = value.get("max")
                if isinstance(minimum, int | float) and isinstance(maximum, int | float) and minimum > maximum:
                    errors.append(V2ValidationIssue(field=path, message="min 값이 max 값보다 큽니다."))
            for key, child in value.items():
                self._check_ranges(child, errors, f"{path}.{key}" if path else str(key))
        elif isinstance(value, list):
            for index, child in enumerate(value):
                self._check_ranges(child, errors, f"{path}[{index}]")

    def _is_ordered_pair(self, value: Any) -> bool:
        """Return whether a value is an increasing two-number array."""
        return (
            isinstance(value, list)
            and len(value) == 2
            and all(isinstance(item, int | float) for item in value)
            and value[0] < value[1]
        )

    def _segment_ranges(self, corridor: Any) -> dict[str, tuple[float, float]]:
        """Return validated segment ranges for corridor_pose bounds checks."""
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
