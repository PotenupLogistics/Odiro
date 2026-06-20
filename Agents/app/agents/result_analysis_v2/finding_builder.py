from __future__ import annotations

from dataclasses import dataclass
from typing import Any

from app.agents.result_analysis_v2.artifact_parser import ParsedArtifact
from app.agents.result_analysis_v2.episode_metric_extractor import EpisodeMetrics
from app.agents.result_analysis_v2.review_text import evidence_message, finding_summary, finding_title


@dataclass(frozen=True)
class FindingBuilder:
    """Creates evidence-backed alpha findings from parsed run metrics."""

    def build(
        self,
        *,
        episodes: list[EpisodeMetrics],
        parsed_artifacts: list[ParsedArtifact],
        prompt_focus: list[str],
    ) -> tuple[list[dict[str, Any]], list[dict[str, Any]]]:
        """Return findings and evidence, never creating findings without evidence."""
        source_paths = self._result_source_paths(parsed_artifacts)
        evidence: list[dict[str, Any]] = []
        finding_inputs: dict[str, list[str]] = {}

        for episode in episodes:
            for finding_type, metric, value, message in self._episode_signals(episode):
                if value <= 0:
                    continue
                evidence_id = f"EV-{len(evidence) + 1:04d}"
                evidence.append(
                    {
                        "evidence_id": evidence_id,
                        "kind": "metric",
                        "run_id": episode.run_id,
                        "episode_id": episode.episode_id,
                        "source_file": source_paths.get(
                            (episode.experiment_id, episode.run_id, episode.episode_id),
                            f"runs/{episode.run_id}/episodes/{episode.episode_id}/result.json",
                        ),
                        "metric": metric,
                        "value": value,
                        "event_type": None,
                        "message": message,
                    }
                )
                finding_inputs.setdefault(finding_type, []).append(evidence_id)

        findings = [
            self._finding(finding_type=finding_type, evidence_ids=evidence_ids, prompt_focus=prompt_focus, index=index)
            for index, (finding_type, evidence_ids) in enumerate(sorted(finding_inputs.items()), start=1)
            if evidence_ids
        ]
        return findings, evidence

    def _episode_signals(self, episode: EpisodeMetrics) -> list[tuple[str, str, int, str]]:
        """Map one episode metrics object to supported alpha finding signals."""
        signals = [
            (
                "penalty_region_violation",
                "penalty_region_violation_count",
                episode.penalty_region_violation_count,
                evidence_message("penalty_region_violation", episode.penalty_region_violation_count),
            ),
            (
                "static_obstacle_collision",
                "static_obstacle_collision_count",
                episode.static_obstacle_collision_count,
                evidence_message("static_obstacle_collision", episode.static_obstacle_collision_count),
            ),
            (
                "pedestrian_collision",
                "pedestrian_collision_count",
                episode.pedestrian_collision_count,
                evidence_message("pedestrian_collision", episode.pedestrian_collision_count),
            ),
            (
                "blocked_region_collision",
                "blocked_region_violation_count",
                episode.blocked_region_violation_count,
                evidence_message("blocked_region_collision", episode.blocked_region_violation_count),
            ),
            ("near_miss", "near_miss_count", episode.near_miss_count, evidence_message("near_miss", episode.near_miss_count)),
            ("timeout", "timeout", 1 if episode.timeout else 0, evidence_message("timeout", 1)),
            (
                "policy_decision_error",
                "policy_decision_error_count",
                episode.policy_decision_error_count,
                evidence_message("policy_decision_error", episode.policy_decision_error_count),
            ),
            (
                "stuck",
                "stuck_count",
                episode.stuck_count
                or (1 if episode.failure_type == "stuck" else 0)
                or (1 if episode.stuck_duration_s and episode.stuck_duration_s > 0 else 0),
                evidence_message("stuck", 1),
            ),
            (
                "robot_tip_over",
                "robot_tip_over_count",
                episode.robot_tip_over_count,
                evidence_message("robot_tip_over", episode.robot_tip_over_count),
            ),
            (
                "goal_not_reached",
                "goal_reached",
                1 if episode.goal_reached is False or (episode.success is False and episode.failure_type != "timeout") else 0,
                evidence_message("goal_not_reached", 1),
            ),
        ]
        return signals

    def _finding(
        self,
        *,
        finding_type: str,
        evidence_ids: list[str],
        prompt_focus: list[str],
        index: int,
    ) -> dict[str, Any]:
        """Create a stable finding record for report.json."""
        return {
            "finding_id": f"F-{index:04d}",
            "type": finding_type,
            "severity": "medium",
            "title": finding_title(finding_type),
            "summary": finding_summary(finding_type, len(evidence_ids)),
            "evidence_ids": evidence_ids,
            "prompt_focus": prompt_focus,
        }

    def _result_source_paths(self, parsed_artifacts: list[ParsedArtifact]) -> dict[tuple[str, str, str], str]:
        """Map episode ids to result.json source files."""
        paths: dict[tuple[str, str, str], str] = {}
        for artifact in parsed_artifacts:
            info = artifact.info
            if info.artifact_type == "episode_result" and info.experiment_id and info.run_id and info.episode_id:
                paths[(info.experiment_id, info.run_id, info.episode_id)] = info.relative_path
        return paths
