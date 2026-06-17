from __future__ import annotations

import re
from typing import Any

from app.models.scenario_generation_v2 import V2ValidationIssue, V2ValidationResult


ALLOWED_SURFACES = {"sidewalk", "crosswalk_stripe", "grass", "road", "driveway", "wall", "building"}
ALLOWED_PROPS = {"traffic_cone_01"}
ALLOWED_SEGMENT_TYPES = {"straight", "narrowing", "crosswalk", "entrance"}
ALLOWED_PLACEMENT_KINDS = {"fixed", "pattern", "scatter"}
ALLOWED_PATTERNS = {"gate", "line", "cluster"}
ALLOWED_ENCOUNTER_TYPES = {"oncoming_pass", "overtake", "cross_path", "standing_group"}
ALLOWED_PERSONAS = {"passive", "normal", "assertive", "vulnerable"}
ALLOWED_OVERRIDE_FIELDS = {
    "cooperation",
    "evasiveness",
    "personal_space_m",
    "awareness_horizon_s",
    "max_yield_wait_s",
    "sidestep_distance_m",
}
FORBIDDEN_ROOT_FIELDS = {
    "base_seed",
    "center_xy_m",
    "episode",
    "episode_count",
    "experiment_id",
    "generated_count",
    "params",
    "policy",
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
        segment_ids = self._validate_corridor(template.get("corridor"), errors)
        self._validate_obstacles(template.get("obstacles"), segment_ids, errors, warnings)
        self._validate_pedestrians(template.get("pedestrians"), segment_ids, errors)
        self._validate_robot(template.get("robot"), self._segment_ranges(template.get("corridor")), segment_ids, errors)
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
        self._validate_surface_list(corridor.get("building_side", []), "corridor.building_side", errors)
        self._validate_surface_list(corridor.get("curb_side", []), "corridor.curb_side", errors)

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
            if placement.get("kind") not in ALLOWED_PLACEMENT_KINDS:
                errors.append(V2ValidationIssue(field=f"{field}.kind", message="지원하지 않는 placement kind입니다."))
                continue
            kind = str(placement.get("kind"))
            if kind in {"fixed", "pattern"}:
                self._validate_fixed_or_pattern_placement(placement, field, segment_ids, errors)
            elif kind == "scatter":
                self._validate_scatter_placement(placement, field, segment_ids, errors)
            allow_blocking = placement.get("allow_blocking")
            if allow_blocking is not None and not isinstance(allow_blocking, bool):
                errors.append(V2ValidationIssue(field=f"{field}.allow_blocking", message="allow_blocking은 boolean이어야 합니다."))

    def _validate_fixed_or_pattern_placement(
        self,
        placement: dict[str, Any],
        field: str,
        segment_ids: set[str],
        errors: list[V2ValidationIssue],
    ) -> None:
        """Validate fixed and pattern placement fields."""
        kind = placement.get("kind")
        prop = placement.get("prop")
        if prop is None:
            errors.append(V2ValidationIssue(field=f"{field}.prop", message="prop가 필요합니다."))
        elif not isinstance(prop, str) or prop not in ALLOWED_PROPS:
            errors.append(V2ValidationIssue(field=f"{field}.prop", message="catalog에 없는 prop입니다."))
        if kind == "pattern":
            pattern = placement.get("pattern")
            if pattern is None:
                errors.append(V2ValidationIssue(field=f"{field}.pattern", message="pattern이 필요합니다."))
            elif not isinstance(pattern, str) or pattern not in ALLOWED_PATTERNS:
                errors.append(V2ValidationIssue(field=f"{field}.pattern", message="지원하지 않는 pattern입니다."))
            for name, validator in (
                ("count", self._validate_integer_or_range),
                ("spacing_m", self._validate_number_or_range),
                ("gap_width_m", self._validate_number_or_range),
            ):
                if name in placement:
                    validator(placement.get(name), f"{field}.{name}", errors)
        at = placement.get("at")
        if not isinstance(at, dict):
            errors.append(V2ValidationIssue(field=f"{field}.at", message="at object가 필요합니다."))
        else:
            self._validate_placement_anchor(at, f"{field}.at", segment_ids, errors)
        if "yaw_deg" in placement:
            self._validate_number_or_range(placement.get("yaw_deg"), f"{field}.yaw_deg", errors)

    def _validate_scatter_placement(
        self,
        placement: dict[str, Any],
        field: str,
        segment_ids: set[str],
        errors: list[V2ValidationIssue],
    ) -> None:
        """Validate scatter placement fields."""
        if "density_per_10m" not in placement:
            errors.append(V2ValidationIssue(field=f"{field}.density_per_10m", message="density_per_10m이 필요합니다."))
        else:
            self._validate_number_or_range(placement.get("density_per_10m"), f"{field}.density_per_10m", errors)
        zone = placement.get("zone")
        if zone is not None:
            if not isinstance(zone, dict):
                errors.append(V2ValidationIssue(field=f"{field}.zone", message="zone은 object여야 합니다."))
            else:
                segments = zone.get("segments")
                if segments is not None:
                    if not isinstance(segments, list):
                        errors.append(V2ValidationIssue(field=f"{field}.zone.segments", message="segments는 list여야 합니다."))
                    else:
                        for index, segment in enumerate(segments):
                            if segment not in segment_ids:
                                errors.append(
                                    V2ValidationIssue(
                                        field=f"{field}.zone.segments[{index}]",
                                        message="존재하지 않는 segment 참조입니다.",
                                    )
                                )
                lanes = zone.get("lanes")
                if lanes is not None and not isinstance(lanes, list):
                    errors.append(V2ValidationIssue(field=f"{field}.zone.lanes", message="lanes는 list여야 합니다."))
        palette = placement.get("palette")
        if palette is not None and not isinstance(palette, dict):
            errors.append(V2ValidationIssue(field=f"{field}.palette", message="palette는 object여야 합니다."))

    def _validate_placement_anchor(
        self,
        at: dict[str, Any],
        field: str,
        segment_ids: set[str],
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
        else:
            self._validate_number_or_range(at.get("along_m"), f"{field}.along_m", errors)
        if "offset_m" not in at:
            errors.append(V2ValidationIssue(field=f"{field}.offset_m", message="offset_m이 필요합니다."))
        else:
            self._validate_number_or_range(at.get("offset_m"), f"{field}.offset_m", errors)
        lane = at.get("lane")
        if lane is not None and not isinstance(lane, str):
            errors.append(V2ValidationIssue(field=f"{field}.lane", message="lane은 고정 string이어야 합니다."))

    def _validate_pedestrians(self, pedestrians: Any, segment_ids: set[str], errors: list[V2ValidationIssue]) -> None:
        """Validate pedestrian encounters and reject direct path authoring."""
        if pedestrians is None:
            return
        if not isinstance(pedestrians, dict):
            errors.append(V2ValidationIssue(field="pedestrians", message="pedestrians는 object여야 합니다."))
            return
        if "path" in pedestrians:
            errors.append(V2ValidationIssue(field="pedestrians.path", message="path 대신 encounters를 사용해야 합니다."))
        background = pedestrians.get("background")
        if isinstance(background, dict):
            self._validate_integer_or_range(background.get("count"), "pedestrians.background.count", errors)
            self._validate_number_or_range(background.get("speed_mps"), "pedestrians.background.speed_mps", errors)
            self._validate_spawn_zone(background.get("spawn_zone"), segment_ids, errors)
        encounters = pedestrians.get("encounters")
        if not isinstance(encounters, list):
            errors.append(V2ValidationIssue(field="pedestrians.encounters", message="encounters는 list여야 합니다."))
            return
        seen: set[str] = set()
        for index, encounter in enumerate(encounters):
            field = f"pedestrians.encounters[{index}]"
            if not isinstance(encounter, dict):
                errors.append(V2ValidationIssue(field=field, message="encounter는 object여야 합니다."))
                continue
            encounter_id = encounter.get("id")
            if not isinstance(encounter_id, str) or not encounter_id:
                errors.append(V2ValidationIssue(field=f"{field}.id", message="encounter id가 필요합니다."))
            elif encounter_id in seen:
                errors.append(V2ValidationIssue(field=f"{field}.id", message="encounter id는 중복될 수 없습니다."))
            else:
                seen.add(encounter_id)
            encounter_type = encounter.get("type")
            if not isinstance(encounter_type, str) or encounter_type not in ALLOWED_ENCOUNTER_TYPES:
                errors.append(V2ValidationIssue(field=f"{field}.type", message="지원하지 않는 encounter type입니다."))
            if encounter.get("at") not in segment_ids:
                errors.append(V2ValidationIssue(field=f"{field}.at", message="존재하지 않는 segment 참조입니다."))
            persona = encounter.get("persona")
            if not isinstance(persona, str) or persona not in ALLOWED_PERSONAS:
                errors.append(V2ValidationIssue(field=f"{field}.persona", message="지원하지 않는 persona입니다."))
            if "meet_offset_m" in encounter:
                self._validate_number_or_range(encounter.get("meet_offset_m"), f"{field}.meet_offset_m", errors)
            self._validate_overrides(encounter.get("overrides"), field, errors)

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
                if lane is not None and not isinstance(lane, str):
                    errors.append(V2ValidationIssue(field=f"{field}.lane", message="lane은 고정 string이어야 합니다."))

    def _validate_surface_list(self, lanes: Any, field: str, errors: list[V2ValidationIssue]) -> None:
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

    def _validate_spawn_zone(self, spawn_zone: Any, segment_ids: set[str], errors: list[V2ValidationIssue]) -> None:
        """Validate optional background pedestrian spawn zone segment references."""
        if spawn_zone is None:
            return
        if not isinstance(spawn_zone, dict):
            errors.append(V2ValidationIssue(field="pedestrians.background.spawn_zone", message="spawn_zone은 object여야 합니다."))
            return
        segments = spawn_zone.get("segments")
        if not isinstance(segments, list):
            errors.append(V2ValidationIssue(field="pedestrians.background.spawn_zone.segments", message="segments는 list여야 합니다."))
            return
        for index, segment in enumerate(segments):
            if segment not in segment_ids:
                errors.append(
                    V2ValidationIssue(
                        field=f"pedestrians.background.spawn_zone.segments[{index}]",
                        message="존재하지 않는 segment 참조입니다.",
                    )
                )

    def _validate_overrides(self, overrides: Any, field: str, errors: list[V2ValidationIssue]) -> None:
        """Validate supported persona override numeric fields."""
        if overrides is None:
            return
        if not isinstance(overrides, dict):
            errors.append(V2ValidationIssue(field=f"{field}.overrides", message="overrides는 object여야 합니다."))
            return
        for key, value in overrides.items():
            override_field = f"{field}.overrides.{key}"
            if key not in ALLOWED_OVERRIDE_FIELDS:
                errors.append(V2ValidationIssue(field=override_field, message="지원하지 않는 override field입니다."))
                continue
            self._validate_number_or_range(value, override_field, errors)

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
