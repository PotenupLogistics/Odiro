"""Environment candidate artifact creation for analysis review sessions."""

from __future__ import annotations

import json
from pathlib import Path

from app.agents.result_analysis_v2.policy_candidate_writer import CandidateWriteResult


class EnvironmentCandidateWriter:
    """Copies root scenario.json and edits only the review candidate copy."""

    def write(self, *, project_path: Path, review_dir: Path) -> CandidateWriteResult:
        """Create a conservative scenario candidate from <project_path>/scenario.json."""
        source_path = project_path / "scenario.json"
        target_path = review_dir / "scenario.json"
        if not source_path.is_file():
            return CandidateWriteResult(
                generated=False,
                path=None,
                generated_files=[],
                warnings=[f"scenario source file does not exist: {source_path}"],
            )

        try:
            scenario = json.loads(source_path.read_text(encoding="utf-8-sig"))
        except Exception as exc:
            return CandidateWriteResult(
                generated=False,
                path=None,
                generated_files=[],
                warnings=[f"scenario source file could not be parsed: {exc}"],
            )
        if not isinstance(scenario, dict):
            return CandidateWriteResult(
                generated=False,
                path=None,
                generated_files=[],
                warnings=["scenario source file must contain a JSON object."],
            )

        modified = self._apply_conservative_environment_changes(dict(scenario))
        target_path.write_text(json.dumps(modified, ensure_ascii=False, indent=2, sort_keys=True), encoding="utf-8")
        return CandidateWriteResult(
            generated=True,
            path=target_path.relative_to(project_path).as_posix(),
            generated_files=[target_path.relative_to(project_path).as_posix()],
            warnings=[],
        )

    def _apply_conservative_environment_changes(self, scenario: dict) -> dict:
        """Increase clearance-related fields without adding private top-level metadata."""
        changed = False
        obstacles = scenario.get("obstacles")
        if not isinstance(obstacles, dict):
            obstacles = {}
            scenario["obstacles"] = obstacles

        min_clear_width = obstacles.get("min_clear_width_m")
        new_min_clear_width = self._increase_value(min_clear_width, minimum=1.2, delta=0.2)
        if new_min_clear_width != min_clear_width:
            obstacles["min_clear_width_m"] = new_min_clear_width
            changed = True

        placements = obstacles.get("placements")
        if isinstance(placements, list):
            for placement in placements:
                if isinstance(placement, dict) and "allow_blocking" in placement and placement["allow_blocking"] is not False:
                    placement["allow_blocking"] = False
                    changed = True

        corridor = scenario.get("corridor")
        if isinstance(corridor, dict):
            walkway_width = corridor.get("walkway_width_m")
            new_walkway_width = self._increase_value(walkway_width, minimum=2.5, delta=0.5)
            if new_walkway_width != walkway_width:
                corridor["walkway_width_m"] = new_walkway_width
                changed = True

        if not changed:
            obstacles["min_clear_width_m"] = 1.2
        return scenario

    def _increase_value(self, value, *, minimum: float, delta: float):
        """Increase fixed or range numeric scenario values toward safer clearance."""
        if isinstance(value, int | float):
            return round(max(float(value) + delta, minimum), 3)
        if isinstance(value, dict):
            changed = False
            updated = dict(value)
            min_value = updated.get("min")
            max_value = updated.get("max")
            if isinstance(min_value, int | float):
                updated["min"] = round(max(float(min_value) + delta, minimum), 3)
                changed = True
            if isinstance(max_value, int | float):
                floor = updated["min"] + 0.1 if isinstance(updated.get("min"), int | float) else minimum
                updated["max"] = round(max(float(max_value) + delta, floor), 3)
                changed = True
            return updated if changed else value
        return value
