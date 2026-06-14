from __future__ import annotations

from dataclasses import dataclass
from typing import Any


@dataclass(frozen=True)
class EpisodeMetrics:
    experiment_id: str
    run_id: str
    episode_id: str
    success: bool | None
    failure_type: str | None
    goal_reached: bool | None
    timeout: bool | None
    collision_count: int
    near_miss_count: int
    blocked_region_violation_count: int
    penalty_region_violation_count: int
    pedestrian_collision_count: int
    static_obstacle_collision_count: int
    duration_s: float | None
    min_pedestrian_distance_m: float | None
    min_obstacle_distance_m: float | None
    average_speed_mps: float | None
    max_speed_mps: float | None
    stop_count: int
    stuck_duration_s: float | None


class EpisodeMetricExtractor:
    def extract(self, experiment_id: str, run_id: str, episode_id: str, result: dict[str, Any] | None, events: list[Any]) -> EpisodeMetrics:
        result = result or {}
        metrics = result.get("metrics") if isinstance(result.get("metrics"), dict) else {}
        failure_type = self._text(result, "failure_type") or self._text(result, "failureType")
        success = self._bool(result, "success")
        goal_reached = self._bool(result, "goal_reached")
        timeout = failure_type == "timeout" or self._bool(result, "timeout")

        return EpisodeMetrics(
            experiment_id=experiment_id,
            run_id=run_id,
            episode_id=episode_id,
            success=success,
            failure_type=failure_type,
            goal_reached=goal_reached,
            timeout=timeout,
            collision_count=self._count(result, metrics, events, "collision_count", "collision"),
            near_miss_count=self._count(result, metrics, events, "near_miss_count", "near_miss"),
            blocked_region_violation_count=self._count(result, metrics, events, "blocked_region_violation_count", "blocked_region_violation"),
            penalty_region_violation_count=self._count(result, metrics, events, "penalty_region_violation_count", "penalty_region_violation"),
            pedestrian_collision_count=self._count(result, metrics, events, "pedestrian_collision_count", "pedestrian_collision"),
            static_obstacle_collision_count=self._count(result, metrics, events, "static_obstacle_collision_count", "static_obstacle_collision"),
            duration_s=self._number(result, "duration_s") or self._number(metrics, "duration_s"),
            min_pedestrian_distance_m=self._number(result, "min_pedestrian_distance_m") or self._number(metrics, "min_pedestrian_distance_m"),
            min_obstacle_distance_m=self._number(result, "min_obstacle_distance_m") or self._number(metrics, "min_obstacle_distance_m"),
            average_speed_mps=self._number(result, "average_speed_mps") or self._number(metrics, "average_speed_mps"),
            max_speed_mps=self._number(result, "max_speed_mps") or self._number(metrics, "max_speed_mps"),
            stop_count=self._count(result, metrics, events, "stop_count", "stop"),
            stuck_duration_s=self._number(result, "stuck_duration_s") or self._number(metrics, "stuck_duration_s"),
        )

    def _count(self, result: dict[str, Any], metrics: dict[str, Any], events: list[Any], key: str, event_name: str) -> int:
        for source in (result, metrics):
            value = source.get(key)
            if isinstance(value, int | float):
                return max(0, int(value))
        return sum(1 for event in events if isinstance(event, dict) and self._event_type(event) == event_name)

    def _event_type(self, event: dict[str, Any]) -> str:
        return str(event.get("event_type") or event.get("event") or event.get("type") or "")

    def _bool(self, source: dict[str, Any], key: str) -> bool | None:
        value = source.get(key)
        return value if isinstance(value, bool) else None

    def _text(self, source: dict[str, Any], key: str) -> str | None:
        value = source.get(key)
        return value if isinstance(value, str) and value else None

    def _number(self, source: dict[str, Any], key: str) -> float | None:
        value = source.get(key)
        if isinstance(value, int | float):
            return float(value)
        return None
