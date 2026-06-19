from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path
from typing import Any

from app.agents.result_analysis_v2.snapshot_hash import SnapshotHashBuilder


@dataclass(frozen=True)
class PreviousRunComparator:
    """Compares the current run snapshot against the closest previous run."""

    snapshot_hash_builder: SnapshotHashBuilder = SnapshotHashBuilder()

    def compare(self, *, project_path: Path, run_id: str, current_snapshot_hashes: dict[str, Any]) -> dict[str, Any]:
        """Return alpha snapshot comparison details for manifest.json."""
        previous_run_id = self._previous_run_id(project_path=project_path, run_id=run_id)
        if previous_run_id is None:
            return {
                "previous_run_id": None,
                "comparison_status": "no_baseline",
                "changed_artifacts": [],
                "added_artifacts": [],
                "removed_artifacts": [],
                "unchanged_artifacts": [],
            }

        previous_hashes = self.snapshot_hash_builder.build(project_path=project_path, run_id=previous_run_id)
        current_files = current_snapshot_hashes.get("files", {})
        previous_files = previous_hashes.get("files", {})
        current_keys = set(current_files)
        previous_keys = set(previous_files)
        added = sorted(current_keys - previous_keys)
        removed = sorted(previous_keys - current_keys)
        changed = sorted(
            key
            for key in current_keys & previous_keys
            if current_files[key].get("sha256") != previous_files[key].get("sha256")
        )
        unchanged = sorted(
            key
            for key in current_keys & previous_keys
            if current_files[key].get("sha256") == previous_files[key].get("sha256")
        )
        return {
            "previous_run_id": previous_run_id,
            "comparison_status": "changed" if added or removed or changed else "unchanged",
            "changed_artifacts": changed,
            "added_artifacts": added,
            "removed_artifacts": removed,
            "unchanged_artifacts": unchanged,
        }

    def _previous_run_id(self, *, project_path: Path, run_id: str) -> str | None:
        """Select the nearest lower six-digit run id from the project runs directory."""
        runs_path = project_path / "runs"
        if not runs_path.is_dir():
            return None
        current = int(run_id)
        candidates = [
            path.name
            for path in runs_path.iterdir()
            if path.is_dir() and len(path.name) == 6 and path.name.isdecimal() and int(path.name) < current
        ]
        return max(candidates) if candidates else None
