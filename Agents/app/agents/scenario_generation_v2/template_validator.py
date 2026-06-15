from __future__ import annotations

import re
from typing import Any

from app.models.scenario_generation_v2 import V2ValidationIssue, V2ValidationResult


ALLOWED_SURFACES = {"sidewalk", "crosswalk_stripe", "grass", "road", "driveway", "wall", "building"}
ALLOWED_PROPS = {"traffic_cone_01"}
ALLOWED_SEGMENT_TYPES = {"straight", "narrowing", "crosswalk", "entrance"}
ALLOWED_PLACEMENT_KINDS = {"fixed", "pattern", "scatter"}
ALLOWED_ENCOUNTER_TYPES = {"oncoming_pass", "overtake", "cross_path", "standing_group"}
ALLOWED_PERSONAS = {"passive", "normal", "assertive", "vulnerable"}
FORBIDDEN_ROOT_FIELDS = {
    "base_seed",
    "experiment_id",
    "generated_count",
    "policy",
    "robot_setup",
    "run_id",
    "sample_count",
    "sample_id",
    "scenario_id",
    "scenario_path",
    "seed",
    "template_path",
    "ue_payload",
}


class TemplateValidator:
    """Deterministically validates current scenario_template v1 objects."""

    def validate(self, template: dict[str, Any]) -> V2ValidationResult:
        """Return catalog and structure diagnostics without invoking an LLM."""
        errors: list[V2ValidationIssue] = []
        warnings: list[V2ValidationIssue] = []
        if not isinstance(template, dict):
            return V2ValidationResult(
                valid=False,
                errors=[V2ValidationIssue(field="template", message="template은 dict여야 합니다.")],
                warnings=[],
            )

        self._validate_root(template, errors)
        segment_ids = self._validate_corridor(template.get("corridor"), errors)
        self._validate_obstacles(template.get("obstacles"), segment_ids, errors, warnings)
        self._validate_pedestrians(template.get("pedestrians"), segment_ids, errors)
        self._validate_robot(template.get("robot"), self._segment_ranges(template.get("corridor")), segment_ids, errors)
        self._check_ranges(template, errors)

        return V2ValidationResult(valid=not errors, errors=errors, warnings=warnings)

    def _validate_root(self, template: dict[str, Any], errors: list[V2ValidationIssue]) -> None:
        """Validate required root fields and reject obsolete generation ownership fields."""
        if template.get("schema") != "scenario_template":
            errors.append(V2ValidationIssue(field="schema", message="schema는 scenario_template이어야 합니다."))
        if template.get("version") != 1:
            errors.append(V2ValidationIssue(field="version", message="version은 1이어야 합니다."))
        template_id = template.get("template_id")
        if not isinstance(template_id, str) or not template_id:
            errors.append(V2ValidationIssue(field="template_id", message="template_id가 필요합니다."))
        elif not re.fullmatch(r"[a-z0-9]+(?:_[a-z0-9]+)*", template_id):
            errors.append(V2ValidationIssue(field="template_id", message="template_id는 snake_case여야 합니다."))
        if not isinstance(template.get("intent"), str) or not template["intent"].strip():
            errors.append(V2ValidationIssue(field="intent", message="intent가 필요합니다."))
        if "corridor" not in template:
            errors.append(V2ValidationIssue(field="corridor", message="corridor가 필요합니다."))
        if "robot" not in template:
            errors.append(V2ValidationIssue(field="robot", message="robot이 필요합니다."))
        for field in sorted(FORBIDDEN_ROOT_FIELDS):
            if field in template:
                errors.append(V2ValidationIssue(field=field, message="scenario_template root에 포함할 수 없는 필드입니다."))
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
            prop = placement.get("prop")
            if prop is not None and prop not in ALLOWED_PROPS:
                errors.append(V2ValidationIssue(field=f"{field}.prop", message="catalog에 없는 prop입니다."))
            at = placement.get("at")
            segment = at.get("segment") if isinstance(at, dict) else None
            if segment is not None and segment not in segment_ids:
                errors.append(V2ValidationIssue(field=f"{field}.at.segment", message="존재하지 않는 segment 참조입니다."))

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
            self._validate_range(background.get("speed_mps"), "pedestrians.background.speed_mps", errors)
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
            if encounter.get("type") not in ALLOWED_ENCOUNTER_TYPES:
                errors.append(V2ValidationIssue(field=f"{field}.type", message="지원하지 않는 encounter type입니다."))
            if encounter.get("at") not in segment_ids:
                errors.append(V2ValidationIssue(field=f"{field}.at", message="존재하지 않는 segment 참조입니다."))
            if encounter.get("persona") not in ALLOWED_PERSONAS:
                errors.append(V2ValidationIssue(field=f"{field}.persona", message="지원하지 않는 persona입니다."))

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
            errors.append(V2ValidationIssue(field="robot.setup", message="robot setup 세부값은 scenario_template에 포함할 수 없습니다."))
        for key in ("start", "goal"):
            anchor = robot.get(key)
            field = f"robot.{key}"
            if not isinstance(anchor, dict):
                errors.append(V2ValidationIssue(field=field, message="robot anchor가 필요합니다."))
                continue
            anchor_type = anchor.get("type")
            if anchor_type not in {"entry", "exit", "corridor_pose"}:
                errors.append(V2ValidationIssue(field=f"{field}.type", message="지원하지 않는 robot anchor type입니다."))
            if anchor_type == "corridor_pose":
                segment = anchor.get("segment")
                if segment not in segment_ids:
                    errors.append(V2ValidationIssue(field=f"{field}.segment", message="존재하지 않는 segment 참조입니다."))
                    continue
                along_m = anchor.get("along_m")
                if isinstance(along_m, int | float) and segment in segment_ranges:
                    minimum, maximum = segment_ranges[segment]
                    if not minimum <= float(along_m) <= maximum:
                        errors.append(V2ValidationIssue(field=f"{field}.along_m", message="along_m이 segment 범위를 벗어났습니다."))
                elif along_m is not None:
                    errors.append(V2ValidationIssue(field=f"{field}.along_m", message="along_m은 숫자여야 합니다."))

    def _validate_surface_list(self, lanes: Any, field: str, errors: list[V2ValidationIssue]) -> None:
        """Validate surface ids in corridor side lane arrays."""
        if not isinstance(lanes, list):
            errors.append(V2ValidationIssue(field=field, message="surface 목록은 list여야 합니다."))
            return
        for index, lane in enumerate(lanes):
            surface = lane.get("surface") if isinstance(lane, dict) else None
            if surface not in ALLOWED_SURFACES:
                errors.append(V2ValidationIssue(field=f"{field}[{index}].surface", message="catalog에 없는 surface입니다."))

    def _validate_range(self, value: Any, field: str, errors: list[V2ValidationIssue]) -> None:
        """Validate a fixed number or min/max range object."""
        if isinstance(value, int | float):
            return
        if isinstance(value, dict):
            minimum = value.get("min")
            maximum = value.get("max")
            if isinstance(minimum, int | float) and isinstance(maximum, int | float) and minimum <= maximum:
                return
        errors.append(V2ValidationIssue(field=field, message="값은 숫자 또는 min/max 범위여야 합니다."))

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
