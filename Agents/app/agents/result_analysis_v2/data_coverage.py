from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path

from app.agents.result_analysis_v2.artifact_parser import ParsedArtifact


@dataclass(frozen=True)
class DataCoverageBuilder:
    """Builds structured coverage facts for alpha review reports."""

    def build(self, *, run_path: Path, parsed_artifacts: list[ParsedArtifact], warnings: list[str]) -> dict:
        """Summarize which run artifacts were available and which were degraded."""
        summary_artifacts = [artifact for artifact in parsed_artifacts if artifact.info.artifact_type == "run_summary"]
        if summary_artifacts and any(isinstance(artifact.data, dict) for artifact in summary_artifacts):
            summary_status = "present"
        elif summary_artifacts:
            summary_status = "broken"
        else:
            summary_status = "missing"

        return {
            "summary_json": summary_status,
            "episode_dirs_count": self._episode_dirs_count(run_path),
            "result_file_count": self._count(parsed_artifacts, "episode_result"),
            "events_file_count": self._count(parsed_artifacts, "episode_events"),
            "actions_file_count": self._existing_episode_file_count(run_path, "actions.jsonl"),
            "parsed_actions_file_count": self._count(parsed_artifacts, "episode_actions"),
            "skipped_large_actions_file_count": self._skipped_large_file_count(warnings, "actions.jsonl"),
            "trace_file_count": self._count(parsed_artifacts, "episode_trace"),
            "broken_json_count": sum(1 for warning in warnings if "JSON parse failed" in warning),
            "broken_json_paths": self._warning_paths(warnings, "JSON parse failed"),
            "broken_jsonl_line_count": sum(1 for warning in warnings if "JSONL parse failed" in warning),
            "broken_jsonl_line_paths": self._warning_paths(warnings, "JSONL parse failed"),
            "missing_artifact_warnings": [warning for warning in warnings if warning.endswith(" is missing.")],
            "large_file_warnings": [warning for warning in warnings if warning.startswith("skipped large file:")],
        }

    def _count(self, parsed_artifacts: list[ParsedArtifact], artifact_type: str) -> int:
        """Count parsed artifacts of one classifier type."""
        return sum(1 for artifact in parsed_artifacts if artifact.info.artifact_type == artifact_type)

    def _episode_dirs_count(self, run_path: Path) -> int:
        """Count episode directories when the run layout exists."""
        episodes_path = run_path / "episodes"
        if not episodes_path.is_dir():
            return 0
        return sum(1 for path in episodes_path.iterdir() if path.is_dir())

    def _existing_episode_file_count(self, run_path: Path, filename: str) -> int:
        """Count episode files on disk even when parsing skipped a large file."""
        episodes_path = run_path / "episodes"
        if not episodes_path.is_dir():
            return 0
        return sum(1 for path in episodes_path.iterdir() if path.is_dir() and (path / filename).is_file())

    def _skipped_large_file_count(self, warnings: list[str], filename: str) -> int:
        """Count run-scoped large-file skip warnings for one artifact name."""
        return sum(1 for warning in warnings if warning.startswith("skipped large file:") and filename in warning)

    def _warning_paths(self, warnings: list[str], marker: str) -> list[str]:
        """Extract artifact paths from parse warnings for report display."""
        paths: list[str] = []
        for warning in warnings:
            if marker not in warning:
                continue
            path = warning.split(":", 1)[0]
            if path not in paths:
                paths.append(path)
        return paths
