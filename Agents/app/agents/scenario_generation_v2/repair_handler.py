from __future__ import annotations

import re
from typing import Any


class RepairHandler:
    """Applies deterministic, local-only repairs before validation."""

    def repair(self, template: dict[str, Any]) -> dict[str, Any]:
        """Normalize template_id and simple inverted ranges without changing ownership fields."""
        repaired = dict(template)
        template_id = str(repaired.get("template_id") or repaired.get("scenario_id") or "scenario_template").strip()
        repaired["template_id"] = re.sub(r"[^a-zA-Z0-9]+", "_", template_id).strip("_").lower() or "scenario_template"
        repaired.pop("scenario_id", None)
        self._swap_inverted_ranges(repaired)
        return repaired

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
