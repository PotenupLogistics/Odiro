from __future__ import annotations

from collections import Counter, defaultdict
from typing import Any

from app.agents.result_analysis_v2.episode_metric_extractor import EpisodeMetrics


class RunAggregator:
    def aggregate(self, episodes: list[EpisodeMetrics]) -> list[dict[str, Any]]:
        grouped: dict[tuple[str, str], list[EpisodeMetrics]] = defaultdict(list)
        for episode in episodes:
            grouped[(episode.experiment_id, episode.run_id)].append(episode)

        summaries: list[dict[str, Any]] = []
        for (experiment_id, run_id), items in sorted(grouped.items()):
            success_count = sum(1 for item in items if item.success is True)
            failure_count = sum(1 for item in items if item.success is False)
            failure_types = Counter(item.failure_type for item in items if item.failure_type)
            summaries.append(
                {
                    "experiment_id": experiment_id,
                    "run_id": run_id,
                    "episode_count": len(items),
                    "success_count": success_count,
                    "failure_count": failure_count,
                    "success_rate": success_count / len(items) if items else 0,
                    "collision_count": sum(item.collision_count for item in items),
                    "near_miss_count": sum(item.near_miss_count for item in items),
                    "blocked_region_violation_count": sum(item.blocked_region_violation_count for item in items),
                    "penalty_region_violation_count": sum(item.penalty_region_violation_count for item in items),
                    "main_failure_types": [failure for failure, _ in failure_types.most_common(3)],
                }
            )
        return summaries
