from __future__ import annotations

from dataclasses import asdict, is_dataclass
from typing import Any


class RepresentativeFailedEpisodeSelectorV2:
    def select(
        self,
        *,
        episode_metrics: list[dict],
        episode_timelines: list[dict],
        max_episodes: int = 5,
    ) -> list[dict]:
        timelines = {self._key(item): item for item in episode_timelines}
        candidates = [self._as_dict(item) for item in episode_metrics]
        ranked = sorted(candidates, key=self._rank)
        selected: list[dict] = []
        for metric in ranked:
            if self._rank(metric)[0] >= 99:
                continue
            item = dict(metric)
            timeline = timelines.get(self._key(item))
            if timeline is not None:
                item["timeline"] = timeline.get("timeline", [])
                item["timeline_summary"] = timeline.get("timeline_summary", "")
            selected.append(item)
            if len(selected) >= max_episodes:
                break
        return selected

    def _rank(self, metric: dict[str, Any]) -> tuple[int, str, str, str]:
        failure_type = str(metric.get("failure_type") or "")
        if failure_type == "setup_failed":
            priority = 99
        elif self._positive(metric, "collision_count") or failure_type in {"collision", "static_obstacle_collision", "pedestrian_collision"}:
            priority = 1
        elif self._positive(metric, "blocked_region_violation_count") or failure_type == "blocked_region_violation":
            priority = 2
        elif self._positive(metric, "near_miss_count") or failure_type == "near_miss":
            priority = 3
        elif metric.get("timeout") is True or failure_type == "timeout":
            priority = 4
        elif metric.get("goal_reached") is False or failure_type == "goal_not_reached":
            priority = 5
        elif failure_type.endswith("_repeated"):
            priority = 6
        else:
            priority = 99
        return (
            priority,
            str(metric.get("experiment_id") or ""),
            str(metric.get("run_id") or ""),
            str(metric.get("episode_id") or ""),
        )

    def _positive(self, metric: dict[str, Any], key: str) -> bool:
        value = metric.get(key)
        return isinstance(value, int | float) and value > 0

    def _key(self, item: dict[str, Any]) -> tuple[str, str, str]:
        return (
            str(item.get("experiment_id") or ""),
            str(item.get("run_id") or ""),
            str(item.get("episode_id") or ""),
        )

    def _as_dict(self, item: Any) -> dict[str, Any]:
        if isinstance(item, dict):
            return item
        if is_dataclass(item):
            return asdict(item)
        return {}
