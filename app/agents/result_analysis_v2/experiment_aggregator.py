from __future__ import annotations

from collections import defaultdict
from typing import Any


class ExperimentAggregator:
    def aggregate(self, run_summaries: list[dict[str, Any]]) -> list[dict[str, Any]]:
        grouped: dict[str, list[dict[str, Any]]] = defaultdict(list)
        for summary in run_summaries:
            grouped[summary["experiment_id"]].append(summary)

        experiments: list[dict[str, Any]] = []
        for experiment_id, runs in sorted(grouped.items()):
            ordered = sorted(runs, key=lambda item: item["run_id"])
            trend = {}
            if len(ordered) >= 2:
                trend = {
                    "success_rate_delta": ordered[-1]["success_rate"] - ordered[0]["success_rate"],
                    "blocked_region_violation_delta": ordered[-1]["blocked_region_violation_count"]
                    - ordered[0]["blocked_region_violation_count"],
                }
            experiments.append(
                {
                    "experiment_id": experiment_id,
                    "run_count": len(ordered),
                    "episode_count": sum(run["episode_count"] for run in ordered),
                    "overall_success_rate": self._overall_success_rate(ordered),
                    "main_failure_patterns": self._main_failure_patterns(ordered),
                    "runs": [{"run_id": run["run_id"], "success_rate": run["success_rate"]} for run in ordered],
                    "trend": trend,
                }
            )
        return experiments

    def _overall_success_rate(self, runs: list[dict[str, Any]]) -> float:
        episode_count = sum(run["episode_count"] for run in runs)
        if episode_count == 0:
            return 0.0
        return sum(run["success_count"] for run in runs) / episode_count

    def _main_failure_patterns(self, runs: list[dict[str, Any]]) -> list[str]:
        patterns: list[str] = []
        for run in runs:
            patterns.extend(run.get("main_failure_types", []))
        return sorted(set(patterns))
