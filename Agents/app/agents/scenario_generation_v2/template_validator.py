from __future__ import annotations

from typing import Any

from app.models.scenario_generation_v2 import V2ValidationIssue, V2ValidationResult


SUPPORTED_OBJECT_TYPES = {"box", "cone", "bollard", "static_obstacle"}
SUPPORTED_REGION_TYPES = {"blocked", "walkable", "penalty"}


class TemplateValidator:
    def validate(self, template: dict[str, Any]) -> V2ValidationResult:
        errors: list[V2ValidationIssue] = []
        warnings: list[V2ValidationIssue] = []
        if not isinstance(template, dict):
            return V2ValidationResult(
                valid=False,
                errors=[V2ValidationIssue(field="scenario_template", message="scenario_template은 dict여야 합니다.")],
                warnings=[],
            )

        if not template.get("scenario_id"):
            errors.append(V2ValidationIssue(field="scenario_id", message="scenario_id가 필요합니다."))
        if "schema" not in template and "version" not in template:
            errors.append(V2ValidationIssue(field="schema", message="schema 또는 version이 필요합니다."))
        if not template.get("summary") and not self._nested(template, "intent", "summary"):
            errors.append(V2ValidationIssue(field="summary", message="summary 또는 intent.summary가 필요합니다."))
        if "ground_model" not in template and "ground" not in template:
            errors.append(V2ValidationIssue(field="ground_model", message="ground_model 또는 ground가 필요합니다."))
        if "robot" not in template and not self._nested(template, "actors", "robot"):
            errors.append(V2ValidationIssue(field="robot", message="robot 또는 actors.robot이 필요합니다."))
        if not self._has_active_condition(template):
            errors.append(V2ValidationIssue(field="scenario_template", message="정적 장애물 또는 보행자 조건 중 최소 하나가 필요합니다."))

        self._check_ranges(template, errors)
        width = self._nested(template, "ground_model", "sidewalk", "width_m")
        if isinstance(width, dict) and width.get("min", 0) < 0.8:
            errors.append(V2ValidationIssue(field="ground_model.sidewalk.width_m", message="보도 폭이 비정상적으로 작습니다."))
        elif isinstance(width, dict) and width.get("min", 0) < 1.2:
            warnings.append(
                V2ValidationIssue(
                    field="ground_model.sidewalk.width_m",
                    message="보도 폭 최소값이 로봇 폭 대비 여유가 작습니다.",
                )
            )

        default_region_type = self._nested(template, "ground_model", "default_region_type")
        if default_region_type is not None and default_region_type not in SUPPORTED_REGION_TYPES:
            errors.append(
                V2ValidationIssue(
                    field="ground_model.default_region_type",
                    message="default_region_type은 blocked/walkable/penalty 중 하나여야 합니다.",
                )
            )

        object_types = self._nested(template, "static_obstacles", "object_types") or []
        for object_type in object_types:
            if object_type not in SUPPORTED_OBJECT_TYPES:
                errors.append(V2ValidationIssue(field="static_obstacles.object_types", message="지원하지 않는 object type입니다."))

        speed = self._nested(template, "pedestrians", "speed_mps")
        if isinstance(speed, dict) and speed.get("max", 0) > 3.0:
            warnings.append(V2ValidationIssue(field="pedestrians.speed_mps", message="보행자 속도 범위가 큽니다."))

        start_x = self._nested(template, "robot", "start_area", "x_m")
        goal_x = self._nested(template, "robot", "goal_area", "x_m")
        if isinstance(start_x, dict) and isinstance(goal_x, dict) and start_x.get("max", 0) >= goal_x.get("min", 0):
            errors.append(V2ValidationIssue(field="robot.goal_area", message="로봇 시작 영역과 목표 영역이 분리되지 않았습니다."))

        self._check_coordinate_arrays(template, errors)

        return V2ValidationResult(valid=not errors, errors=errors, warnings=warnings)

    def _check_ranges(self, value: Any, errors: list[V2ValidationIssue], path: str = "") -> None:
        if isinstance(value, dict):
            if set(value) >= {"min", "max"}:
                minimum = value.get("min")
                maximum = value.get("max")
                if isinstance(minimum, int | float) and isinstance(maximum, int | float) and minimum > maximum:
                    errors.append(V2ValidationIssue(field=path, message="min 값이 max 값보다 큽니다."))
                if path.endswith("count") and isinstance(minimum, int | float) and minimum < 0:
                    errors.append(V2ValidationIssue(field=path, message="count 값은 음수일 수 없습니다."))
            for key, child in value.items():
                if key == "count" and isinstance(child, int | float) and child < 0:
                    errors.append(V2ValidationIssue(field=f"{path}.{key}" if path else key, message="count 값은 음수일 수 없습니다."))
                self._check_ranges(child, errors, f"{path}.{key}" if path else str(key))
        elif isinstance(value, list):
            for index, child in enumerate(value):
                self._check_ranges(child, errors, f"{path}[{index}]")

    def _check_coordinate_arrays(self, value: Any, errors: list[V2ValidationIssue], path: str = "") -> None:
        if isinstance(value, dict):
            for key, child in value.items():
                child_path = f"{path}.{key}" if path else str(key)
                if key in {"position", "coordinates", "xy", "xyz"} and isinstance(child, list):
                    if not all(isinstance(item, int | float) for item in child):
                        errors.append(V2ValidationIssue(field=child_path, message="좌표 배열은 숫자만 포함해야 합니다."))
                self._check_coordinate_arrays(child, errors, child_path)
        elif isinstance(value, list):
            for index, child in enumerate(value):
                self._check_coordinate_arrays(child, errors, f"{path}[{index}]")

    def _has_active_condition(self, template: dict[str, Any]) -> bool:
        for key in ("static_obstacles", "pedestrians"):
            value = template.get(key)
            if not isinstance(value, dict):
                continue
            count = value.get("count")
            if isinstance(count, dict) and max(count.get("min", 0), count.get("max", 0)) > 0:
                return True
            if isinstance(count, int | float) and count > 0:
                return True
        return False

    def _nested(self, value: dict[str, Any], *keys: str) -> Any:
        current: Any = value
        for key in keys:
            if not isinstance(current, dict):
                return None
            current = current.get(key)
        return current
