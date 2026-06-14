from __future__ import annotations

import re
from typing import Any


class RepairHandler:
    def repair(self, template: dict[str, Any]) -> dict[str, Any]:
        repaired = dict(template)
        scenario_id = str(repaired.get("scenario_id", "scenario_template")).strip()
        repaired["scenario_id"] = re.sub(r"[^a-zA-Z0-9]+", "_", scenario_id).strip("_").lower() or "scenario_template"
        self._swap_inverted_ranges(repaired)
        return repaired

    def _swap_inverted_ranges(self, value: Any) -> None:
        if isinstance(value, dict):
            if set(value) >= {"min", "max"} and isinstance(value["min"], int | float) and isinstance(value["max"], int | float):
                if value["min"] > value["max"]:
                    value["min"], value["max"] = value["max"], value["min"]
            for child in value.values():
                self._swap_inverted_ranges(child)
        elif isinstance(value, list):
            for child in value:
                self._swap_inverted_ranges(child)
