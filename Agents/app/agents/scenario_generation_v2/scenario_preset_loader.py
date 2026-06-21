from __future__ import annotations

import json
from copy import deepcopy
from pathlib import Path
from typing import Any


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
