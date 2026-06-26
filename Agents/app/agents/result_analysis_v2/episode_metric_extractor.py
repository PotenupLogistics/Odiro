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
    "Repath": "repath",
    "Stuck": "stuck",
    "PolicyDecisionError": "policy_decision_error",
    "SetupFailed": "setup_failed",
    "GoalNotReached": "goal_not_reached",
    "GoalReached": "goal_reached",
}

SUCCESS_TERMINAL_REASONS = {"goal_reached"}

KNOWN_TERMINAL_REASONS = {
    "blocked_region_collision",
    "blocked_region_violation",
    "collision",
    "goal_not_reached",
    "pedestrian_collision",
    "policy_decision_error",
    "repath",
    "robot_tip_over",
    "setup_failed",
    "static_obstacle_collision",
    "stuck",
    "timeout",
}


@dataclass(frozen=True)
class SetupFailureDetails:
    """Structured setup-stage failure details extracted from logged payload fields."""

    category: str | None
    error_code: str | None
    message: str | None
    resource_type: str | None
    resource_id: str | None
    source: str


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
    repath_count: int
    policy_decision_error_count: int
    stuck_count: int
    robot_tip_over_count: int
    stuck_duration_s: float | None
    terminal_reason: str | None
    setup_failure_details: SetupFailureDetails | None


class EpisodeMetricExtractor:
    """Extracts latest and legacy episode result metrics into one shape."""

    def extract(self, experiment_id: str, run_id: str, episode_id: str, result: dict[str, Any] | None, events: list[Any]) -> EpisodeMetrics:
        """Read result.json and JSONL events with latest-schema fields preferred."""
        result = result or {}
        summary = result.get("summary") if isinstance(result.get("summary"), dict) else {}
        metrics = result.get("metrics") if isinstance(result.get("metrics"), dict) else {}
        event_summary = result.get("event_summary") if isinstance(result.get("event_summary"), dict) else {}
        pipeline = result.get("pipeline") if isinstance(result.get("pipeline"), dict) else {}
        result_events = result.get("events") if isinstance(result.get("events"), list) else []
        all_events = [*events, *result_events]
        terminal_reason = self._text(summary, "terminal_reason") or self._text(result, "terminal_reason")
        success = self._first_bool(summary, result, key="success")
        failure_type = self._failure_type(
            terminal_reason=terminal_reason,
            legacy_failure_type=self._text(result, "failure_type") or self._text(result, "failureType"),
            success=success,
        )
        goal_reached = self._first_bool(summary, result, key="goal_reached")
        timeout_flag = self._first_bool(summary, result, key="timeout")
        timeout = failure_type == "timeout" or timeout_flag is True
        setup_failed = failure_type == "setup_failed"
        setup_failure_details = (
            self._setup_failure_details(summary=summary, metrics=metrics, pipeline=pipeline, events=all_events)
            if setup_failed
            else None
        )

        return EpisodeMetrics(
            experiment_id=experiment_id,
            run_id=run_id,
            episode_id=episode_id,
            success=success,
            failure_type=failure_type,
            goal_reached=goal_reached,
            timeout=timeout,
            collision_count=0 if setup_failed else self._count(
                result,
                metrics,
                event_summary,
                all_events,
                "collision_count",
                {"collision", "static_obstacle_collision", "pedestrian_collision"},
            ),
            near_miss_count=0 if setup_failed else self._count(
                result,
                metrics,
                event_summary,
                all_events,
                "near_miss_count",
                {"near_miss", "pedestrian_near_miss"},
            ),
            blocked_region_violation_count=0 if setup_failed else self._count(
                result,
                metrics,
                event_summary,
                all_events,
                "blocked_region_violation_count",
                {"blocked_region_violation", "blocked_region_collision"},
            ),
            penalty_region_violation_count=0 if setup_failed else self._count(
                result,
                metrics,
                event_summary,
                all_events,
                "penalty_region_violation_count",
                {"penalty_region_violation"},
            ),
            pedestrian_collision_count=0 if setup_failed else self._count(
                result,
                metrics,
                event_summary,
                all_events,
                "pedestrian_collision_count",
                {"pedestrian_collision"},
            ),
            static_obstacle_collision_count=0 if setup_failed else self._count(
                result,
                metrics,
                event_summary,
                all_events,
                "static_obstacle_collision_count",
                {"static_obstacle_collision"},
            ),
            duration_s=self._first_number(metrics, result, key="duration_s"),
            min_pedestrian_distance_m=self._first_number(metrics, result, key="min_pedestrian_distance_m"),
            min_obstacle_distance_m=self._first_number(metrics, result, key="min_obstacle_distance_m"),
            average_speed_mps=self._first_number(metrics, result, key="average_speed_mps"),
            max_speed_mps=self._first_number(metrics, result, key="max_speed_mps"),
            stop_count=0 if setup_failed else self._count(result, metrics, event_summary, all_events, "stop_count", {"stop"}),
            repath_count=0 if setup_failed else self._count(result, metrics, event_summary, all_events, "repath_count", {"repath"}),
            policy_decision_error_count=0 if setup_failed else self._count(
                result,
                metrics,
                event_summary,
                all_events,
                "policy_decision_error_count",
                {"policy_decision_error"},
            ),
            stuck_count=0 if setup_failed else self._count(result, metrics, event_summary, all_events, "stuck_count", {"stuck"}),
            robot_tip_over_count=0 if setup_failed else self._count(
                result,
                metrics,
                event_summary,
                all_events,
                "robot_tip_over_count",
                {"robot_tip_over"},
            ),
            stuck_duration_s=self._first_number(metrics, result, key="stuck_duration_s"),
            terminal_reason=terminal_reason,
            setup_failure_details=setup_failure_details,
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

    def _failure_type(
        self,
        *,
        terminal_reason: str | None,
        legacy_failure_type: str | None,
        success: bool | None,
    ) -> str | None:
        """Prefer terminal_reason while preserving unknown terminal reasons explicitly."""
        if terminal_reason:
            normalized_terminal = self._normalize_signal(terminal_reason)
            if normalized_terminal in SUCCESS_TERMINAL_REASONS:
                return None
            if success is True and normalized_terminal not in KNOWN_TERMINAL_REASONS:
                return None
            return normalized_terminal if normalized_terminal in KNOWN_TERMINAL_REASONS else "unknown_failure"
        return self._normalize_signal(legacy_failure_type)

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

    def _setup_failure_details(
        self,
        *,
        summary: dict[str, Any],
        metrics: dict[str, Any],
        pipeline: dict[str, Any],
        events: list[Any],
    ) -> SetupFailureDetails:
        """Extract setup failure details without inferring resource ids from free text."""
        diagnostics = pipeline.get("diagnostics")
        if isinstance(diagnostics, list):
            for index, item in enumerate(diagnostics):
                detail = self._detail_from_payload(item, source=f"pipeline.diagnostics[{index}]")
                if detail is not None:
                    return detail
        elif diagnostics:
            detail = self._detail_from_payload(diagnostics, source="pipeline.diagnostics")
            if detail is not None:
                return detail

        outcome = self._text(summary, "outcome")
        if outcome:
            return SetupFailureDetails(
                category="setup_failed",
                error_code=None,
                message=outcome,
                resource_type=None,
                resource_id=None,
                source="summary.outcome",
            )

        failure_message = self._text(metrics, "delivery_bot_failure_message")
        if failure_message:
            return SetupFailureDetails(
                category="setup_failed",
                error_code=None,
                message=failure_message,
                resource_type=None,
                resource_id=None,
                source="metrics.delivery_bot_failure_message",
            )

        for index, event in enumerate(events):
            if not isinstance(event, dict) or not self._is_setup_event(event):
                continue
            properties = event.get("properties")
            if isinstance(properties, dict):
                detail = self._detail_from_payload(properties, source=f"events[{index}].properties")
                if detail is not None:
                    return detail
            message = self._text(event, "message")
            if message:
                return SetupFailureDetails(
                    category="setup_failed",
                    error_code=None,
                    message=message,
                    resource_type=None,
                    resource_id=None,
                    source=f"events[{index}].message",
                )

        return SetupFailureDetails(
            category="setup_failed",
            error_code=None,
            message=None,
            resource_type=None,
            resource_id=None,
            source="summary.terminal_reason",
        )

    def _detail_from_payload(self, payload: Any, *, source: str) -> SetupFailureDetails | None:
        """Read structured setup diagnostics from dicts or preserve string messages only."""
        if isinstance(payload, str) and payload.strip():
            return SetupFailureDetails(
                category="setup_failed",
                error_code=None,
                message=payload.strip(),
                resource_type=None,
                resource_id=None,
                source=source,
            )
        if not isinstance(payload, dict):
            return None

        error_code = self._first_text(payload, ("code", "error_code", "errorCode"))
        message = self._first_text(payload, ("message", "detail", "reason"))
        resource_type, resource_id = self._resource_from_payload(payload)
        category = self._first_text(payload, ("category", "type", "stage")) or "setup_failed"
        return SetupFailureDetails(
            category=category,
            error_code=error_code,
            message=message,
            resource_type=resource_type,
            resource_id=resource_id,
            source=source,
        )

    def _resource_from_payload(self, payload: dict[str, Any]) -> tuple[str | None, str | None]:
        """Return structured prop or asset identifiers when the payload names them."""
        for resource_type, keys in (
            ("prop", ("prop_id", "propId", "failed_prop_id", "failedPropId")),
            ("asset", ("asset_id", "assetId", "failed_asset_id", "failedAssetId")),
            ("map", ("map_id", "mapId")),
            ("segment", ("segment_id", "segmentId")),
        ):
            resource_id = self._first_text(payload, keys)
            if resource_id:
                return resource_type, resource_id
        return None, None

    def _first_text(self, source: dict[str, Any], keys: tuple[str, ...]) -> str | None:
        """Return the first non-empty text value among equivalent field names."""
        for key in keys:
            value = self._text(source, key)
            if value:
                return value
        return None

    def _is_setup_event(self, event: dict[str, Any]) -> bool:
        """Identify setup-stage event records without treating runtime events as setup data."""
        event_type = self._event_type(event)
        return event_type in {"setup_failed", "setup_failure", "world_setup_failed", "setup_error"}
