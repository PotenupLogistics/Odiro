from __future__ import annotations

import json
from copy import deepcopy
from dataclasses import dataclass
from pathlib import Path
from typing import Any


@dataclass(frozen=True)
class ScenarioPresetLoadResult:
    """Optional preset load result used to keep missing presets out of API errors."""

    preset_id: str
    scenario: dict[str, Any] | None
    error: Exception | None = None


class ScenarioPresetLoader:
    """Loads bundled project scenario presets without owning user project files."""

    def __init__(self, *, start_path: Path | None = None) -> None:
        """Store the filesystem anchor used to locate repository static templates."""
        self.start_path = (start_path or Path(__file__)).resolve()

    def load_scenario_preset(self, preset_id: str) -> dict[str, Any]:
        """Return a deep-copied scenario preset by safe preset id."""
        if not preset_id or any(token in preset_id for token in ("/", "\\", "..")):
            raise ValueError("scenario preset id must be a safe path segment")
        preset_path = self._find_scenario_preset_path(preset_id)
        with preset_path.open(encoding="utf-8") as preset_file:
            loaded = json.load(preset_file)
        if not isinstance(loaded, dict):
            raise ValueError("scenario preset root must be an object")
        return deepcopy(loaded)

    def try_load_scenario_preset(self, preset_id: str) -> ScenarioPresetLoadResult:
        """Return a preset object or a fallback reason without raising file/JSON errors."""
        try:
            return ScenarioPresetLoadResult(preset_id=preset_id, scenario=self.load_scenario_preset(preset_id))
        except (FileNotFoundError, json.JSONDecodeError, ValueError) as exc:
            return ScenarioPresetLoadResult(preset_id=preset_id, scenario=None, error=exc)

    def _find_scenario_preset_path(self, preset_id: str) -> Path:
        """Find a bundled scenario preset under repo static templates."""
        filename = f"{preset_id}.json"
        for root in self._candidate_roots():
            candidate = root / "static" / "templates" / "scenario" / filename
            if candidate.is_file():
                return candidate
        raise FileNotFoundError(f"scenario preset not found: {preset_id}")

    def _candidate_roots(self) -> list[Path]:
        """Return ancestor directories that may contain bundled template resources."""
        roots: list[Path] = []
        for candidate in (self.start_path, *self.start_path.parents):
            root = candidate if candidate.is_dir() else candidate.parent
            if root not in roots:
                roots.append(root)
        return roots
