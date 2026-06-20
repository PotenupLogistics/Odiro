from __future__ import annotations

from dataclasses import dataclass
import re
from typing import Any


# Canonical mapping for documented event names and legacy spellings.
EVENT_TYPE_ALIASES = {
    "PenaltyRegionViolation": "penalty_region_violation",
    "StaticObstacleCollision": "static_obstacle_collision",
    "BlockedRegionCollision": "blocked_region_collision",
    "BlockedRegionViolation": "blocked_region_violation",
    "PedestrianNearMiss": "pedestrian_near_miss",
    "PedestrianCollision": "pedestrian_collision",
    "Timeout": "timeout",
    "RobotTipOver": "robot_tip_over",
    "Stuck": "stuck",
    "PolicyDecisionError": "policy_decision_error",
}


@dataclass(frozen=True)
class EpisodeMetrics:
    """Normalized per-episode metrics used by v2 result analysis."""

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
    policy_decision_error_count: int
    stuck_count: int
    robot_tip_over_count: int
    stuck_duration_s: float | None


class EpisodeMetricExtractor:
    """Extracts latest and legacy episode result metrics into one shape."""

    def extract(self, experiment_id: str, run_id: str, episode_id: str, result: dict[str, Any] | None, events: list[Any]) -> EpisodeMetrics:
        """Read result.json and JSONL events with latest-schema fields preferred."""
        result = result or {}
        summary = result.get("summary") if isinstance(result.get("summary"), dict) else {}
        metrics = result.get("metrics") if isinstance(result.get("metrics"), dict) else {}
        event_summary = result.get("event_summary") if isinstance(result.get("event_summary"), dict) else {}
        failure_type = self._normalize_signal(
            self._text(summary, "terminal_reason")
            or self._text(result, "failure_type")
            or self._text(result, "failureType")
        )
        success = self._first_bool(summary, result, key="success")
        goal_reached = self._first_bool(summary, result, key="goal_reached")
        timeout_flag = self._first_bool(summary, result, key="timeout")
        timeout = failure_type == "timeout" or timeout_flag is True

        return EpisodeMetrics(
            experiment_id=experiment_id,
            run_id=run_id,
            episode_id=episode_id,
            success=success,
            failure_type=failure_type,
            goal_reached=goal_reached,
            timeout=timeout,
            collision_count=self._count(
                result,
                metrics,
                event_summary,
                events,
                "collision_count",
                {"collision", "static_obstacle_collision", "pedestrian_collision"},
            ),
            near_miss_count=self._count(
                result,
                metrics,
                event_summary,
                events,
                "near_miss_count",
                {"near_miss", "pedestrian_near_miss"},
            ),
            blocked_region_violation_count=self._count(
                result,
                metrics,
                event_summary,
                events,
                "blocked_region_violation_count",
                {"blocked_region_violation", "blocked_region_collision"},
            ),
            penalty_region_violation_count=self._count(
                result,
                metrics,
                event_summary,
                events,
                "penalty_region_violation_count",
                {"penalty_region_violation"},
            ),
            pedestrian_collision_count=self._count(
                result,
                metrics,
                event_summary,
                events,
                "pedestrian_collision_count",
                {"pedestrian_collision"},
            ),
            static_obstacle_collision_count=self._count(
                result,
                metrics,
                event_summary,
                events,
                "static_obstacle_collision_count",
                {"static_obstacle_collision"},
            ),
            duration_s=self._first_number(metrics, result, key="duration_s"),
            min_pedestrian_distance_m=self._first_number(metrics, result, key="min_pedestrian_distance_m"),
            min_obstacle_distance_m=self._first_number(metrics, result, key="min_obstacle_distance_m"),
            average_speed_mps=self._first_number(metrics, result, key="average_speed_mps"),
            max_speed_mps=self._first_number(metrics, result, key="max_speed_mps"),
            stop_count=self._count(result, metrics, event_summary, events, "stop_count", {"stop"}),
            policy_decision_error_count=self._count(
                result,
                metrics,
                event_summary,
                events,
                "policy_decision_error_count",
                {"policy_decision_error"},
            ),
            stuck_count=self._count(result, metrics, event_summary, events, "stuck_count", {"stuck"}),
            robot_tip_over_count=self._count(
                result,
                metrics,
                event_summary,
                events,
                "robot_tip_over_count",
                {"robot_tip_over"},
            ),
            stuck_duration_s=self._first_number(metrics, result, key="stuck_duration_s"),
        )

    def _count(
        self,
        result: dict[str, Any],
        metrics: dict[str, Any],
        event_summary: dict[str, Any],
        events: list[Any],
        key: str,
        event_names: set[str],
    ) -> int:
        """Read a count from latest metrics, legacy root fields, event summary, then events."""
        for source in (metrics, result):
            value = source.get(key)
            if isinstance(value, int | float):
                return max(0, int(value))
        summary_count = self._event_summary_count(event_summary, event_names)
        if summary_count:
            return summary_count
        return sum(1 for event in events if isinstance(event, dict) and self._event_type(event) in event_names)

    def _event_type(self, event: dict[str, Any]) -> str:
        """Return a normalized event type from common JSONL event fields."""
        return self._normalize_signal(str(event.get("event_type") or event.get("event") or event.get("type") or ""))

    def _event_summary_count(self, event_summary: dict[str, Any], event_names: set[str]) -> int:
        """Count matching events from latest result.event_summary forms."""
        candidates: list[dict[str, Any]] = []
        by_type = event_summary.get("by_type")
        if isinstance(by_type, dict):
            candidates.append(by_type)
        candidates.append(event_summary)

        count = 0
        for source in candidates:
            for key, value in source.items():
                if self._normalize_signal(str(key)) in event_names and isinstance(value, int | float):
                    count += max(0, int(value))
        return count

    def _first_bool(self, *sources: dict[str, Any], key: str) -> bool | None:
        """Return the first boolean value for a key without treating False as absent."""
        for source in sources:
            value = self._bool(source, key)
            if value is not None:
                return value
        return None

    def _first_number(self, *sources: dict[str, Any], key: str) -> float | None:
        """Return the first numeric value for a key from preferred sources."""
        for source in sources:
            value = self._number(source, key)
            if value is not None:
                return value
        return None

    def _normalize_signal(self, value: str | None) -> str | None:
        """Normalize PascalCase, spaced, and snake_case event names to snake_case."""
        if value is None:
            return None
        text = value.strip()
        if not text:
            return None
        if text in EVENT_TYPE_ALIASES:
            return EVENT_TYPE_ALIASES[text]
        snake = re.sub(r"(.)([A-Z][a-z]+)", r"\1_\2", text)
        snake = re.sub(r"([a-z0-9])([A-Z])", r"\1_\2", snake)
        snake = re.sub(r"[\s\-.]+", "_", snake).casefold()
        return EVENT_TYPE_ALIASES.get(snake, snake)

    def _bool(self, source: dict[str, Any], key: str) -> bool | None:
        """Return a boolean field value when present."""
        value = source.get(key)
        return value if isinstance(value, bool) else None

    def _text(self, source: dict[str, Any], key: str) -> str | None:
        """Return a non-empty string field value when present."""
        value = source.get(key)
        return value if isinstance(value, str) and value else None

    def _number(self, source: dict[str, Any], key: str) -> float | None:
        """Return a numeric field value as float when present."""
        value = source.get(key)
        if isinstance(value, int | float):
            return float(value)
        return None
