from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path

from app.agents.result_analysis_v2.artifact_parser import ACTIONS_STREAMING_SUMMARY_TYPE, ParsedArtifact


@dataclass(frozen=True)
class DataCoverageBuilder:
    """Builds structured coverage facts for alpha review reports."""

    def build(self, *, run_path: Path, parsed_artifacts: list[ParsedArtifact], warnings: list[str]) -> dict:
        """Summarize which run artifacts were available and which were degraded."""
        summary_artifacts = [artifact for artifact in parsed_artifacts if artifact.info.artifact_type == "run_summary"]
        actions_summaries = self._actions_summaries(parsed_artifacts)
        actions_summary_broken_lines = self._actions_summary_total(actions_summaries, "broken_actions_line_count")
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
            "summarized_actions_file_count": len(actions_summaries),
            "skipped_large_actions_file_count": self._skipped_large_file_count(warnings, "actions.jsonl"),
            "actions_summary_line_count": self._actions_summary_total(actions_summaries, "actions_line_count"),
            "actions_summary_parsed_line_count": self._actions_summary_total(
                actions_summaries,
                "parsed_actions_line_count",
            ),
            "actions_summary_broken_line_count": actions_summary_broken_lines,
            "actions_summary_truncated_count": sum(1 for summary in actions_summaries if summary.get("truncated") is True),
            "actions_summary_warnings": [
                warning
                for warning in warnings
                if "actions.jsonl" in warning and "action summary" in warning
            ],
            "trace_file_count": self._count(parsed_artifacts, "episode_trace"),
            "broken_json_count": sum(1 for warning in warnings if "JSON parse failed" in warning),
            "broken_json_paths": self._warning_paths(warnings, "JSON parse failed"),
            "broken_jsonl_line_count": sum(1 for warning in warnings if "JSONL parse failed" in warning)
            + actions_summary_broken_lines,
            "broken_jsonl_line_paths": self._jsonl_warning_paths(warnings, actions_summaries),
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

    def _actions_summaries(self, parsed_artifacts: list[ParsedArtifact]) -> list[dict]:
        """Return parsed actions summaries with the expected marker."""
        return [
            artifact.data
            for artifact in parsed_artifacts
            if artifact.info.artifact_type == "episode_actions"
            and isinstance(artifact.data, dict)
            and artifact.data.get("summary_type") == ACTIONS_STREAMING_SUMMARY_TYPE
        ]

    def _actions_summary_total(self, actions_summaries: list[dict], field_name: str) -> int:
        """Sum integer counters from action summaries."""
        total = 0
        for summary in actions_summaries:
            value = summary.get(field_name)
            if isinstance(value, int | float):
                total += int(value)
        return total

    def _jsonl_warning_paths(self, warnings: list[str], actions_summaries: list[dict]) -> list[str]:
        """Combine generic JSONL parse paths with action-summary parse paths."""
        paths = self._warning_paths(warnings, "JSONL parse failed")
        if any(summary.get("broken_actions_line_count", 0) for summary in actions_summaries):
            for warning in warnings:
                if "actions.jsonl" not in warning or "action summary" not in warning:
                    continue
                path = warning.split(":", 1)[0]
                if path not in paths:
                    paths.append(path)
        return paths

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
