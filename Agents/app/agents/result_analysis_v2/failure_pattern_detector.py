from __future__ import annotations

from collections import defaultdict
from typing import Any

from app.agents.result_analysis_v2.episode_metric_extractor import EpisodeMetrics


class FailurePatternDetector:
    """Detects repeated failure patterns across normalized episode metrics."""

    def detect(self, episodes: list[EpisodeMetrics]) -> list[dict[str, Any]]:
        """Return repeated pattern records using unique episode evidence per pattern."""
        by_type: dict[str, dict[tuple[str, str], EpisodeMetrics]] = defaultdict(dict)
        for episode in episodes:
            if episode.failure_type:
                self._add_episode(by_type, episode.failure_type, episode)
            if episode.blocked_region_violation_count:
                self._add_episode(by_type, "blocked_region_violation", episode)
            if episode.near_miss_count:
                self._add_episode(by_type, "near_miss", episode)
            if episode.collision_count:
                self._add_episode(by_type, "collision", episode)
            if episode.pedestrian_collision_count:
                self._add_episode(by_type, "pedestrian_collision", episode)
            if episode.timeout:
                self._add_episode(by_type, "timeout", episode)
            if episode.goal_reached is False:
                self._add_episode(by_type, "goal_not_reached", episode)
            if episode.policy_decision_error_count:
                self._add_episode(by_type, "policy_decision_error", episode)
            if episode.stuck_count:
                self._add_episode(by_type, "stuck", episode)

        patterns: list[dict[str, Any]] = []
        for index, (pattern_type, item_by_episode) in enumerate(sorted(by_type.items()), start=1):
            items = list(item_by_episode.values())
            if len(items) < 2:
                continue
            patterns.append(
                {
                    "pattern_id": f"PATTERN-{index:03d}",
                    "type": f"{pattern_type}_repeated",
                    "count": len(items),
                    "evidence": [
                        {
                            "experiment_id": item.experiment_id,
                            "run_id": item.run_id,
                            "episode_id": item.episode_id,
                        }
                        for item in items[:5]
                    ],
                }
            )
        return patterns

    def _add_episode(
        self,
        by_type: dict[str, dict[tuple[str, str], EpisodeMetrics]],
        pattern_type: str,
        episode: EpisodeMetrics,
    ) -> None:
        """Keep one evidence entry per run/episode for each pattern type."""
        by_type[pattern_type].setdefault((episode.run_id, episode.episode_id), episode)
