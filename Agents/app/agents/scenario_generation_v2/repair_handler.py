from __future__ import annotations

from copy import deepcopy
import re
from typing import Any


class RepairHandler:
    """Applies deterministic, local-only repairs before validation."""

    def repair(self, template: dict[str, Any]) -> dict[str, Any]:
        """Normalize template_id and simple inverted ranges without changing ownership fields."""
        repaired = deepcopy(template)
        template_id = str(repaired.get("template_id") or repaired.get("scenario_id") or "scenario_template").strip()
        repaired["template_id"] = re.sub(r"[^a-zA-Z0-9]+", "_", template_id).strip("_").lower() or "scenario_template"
        repaired.pop("scenario_id", None)
        self._remove_null_fields(repaired)
        self._repair_robot_anchors(repaired)
        self._swap_inverted_ranges(repaired)
        return repaired

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
