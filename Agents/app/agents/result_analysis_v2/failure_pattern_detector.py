from __future__ import annotations

from collections import defaultdict
from typing import Any

from app.agents.result_analysis_v2.episode_metric_extractor import EpisodeMetrics


class FailurePatternDetector:
    def detect(self, episodes: list[EpisodeMetrics]) -> list[dict[str, Any]]:
        by_type: dict[str, list[EpisodeMetrics]] = defaultdict(list)
        for episode in episodes:
            if episode.failure_type:
                by_type[episode.failure_type].append(episode)
            if episode.blocked_region_violation_count:
                by_type["blocked_region_violation"].append(episode)
            if episode.near_miss_count:
                by_type["near_miss"].append(episode)
            if episode.collision_count:
                by_type["collision"].append(episode)
            if episode.timeout:
                by_type["timeout"].append(episode)
            if episode.goal_reached is False:
                by_type["goal_not_reached"].append(episode)

        patterns: list[dict[str, Any]] = []
        for index, (pattern_type, items) in enumerate(sorted(by_type.items()), start=1):
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
