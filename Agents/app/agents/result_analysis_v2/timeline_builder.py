from __future__ import annotations

from dataclasses import asdict, is_dataclass
from typing import Any


KEY_EVENT_TYPES = {
    "obstacle_detected",
    "pedestrian_detected",
    "slow_down_started",
    "stop_started",
    "avoidance_started",
    "near_miss",
    "collision",
    "static_obstacle_collision",
    "pedestrian_collision",
    "blocked_region_violation",
    "penalty_region_violation",
    "goal_reached",
    "timeout",
    "stuck",
}


class EventTimelineBuilderV2:
    def build_episode_timelines(
        self,
        parsed_artifacts: dict,
        episode_metrics: list[dict],
    ) -> list[dict]:
        episode_inputs = self._episode_inputs(parsed_artifacts)
        timelines: list[dict] = []
        for metric in [self._as_dict(item) for item in episode_metrics]:
            key = self._episode_key(metric)
            item = episode_inputs.get(key, {})
            timelines.append(
                self.build_single_episode_timeline(
                    experiment_id=str(metric.get("experiment_id") or ""),
                    run_id=str(metric.get("run_id") or ""),
                    episode_id=str(metric.get("episode_id") or ""),
                    events=item.get("events", []),
                    actions=item.get("actions", []),
                    action_summary=item.get("action_summary"),
                )
            )
        return timelines

    def build_single_episode_timeline(
        self,
        *,
        experiment_id: str,
        run_id: str,
        episode_id: str,
        events: list[dict],
        actions: list[dict] | None = None,
        action_summary: dict[str, Any] | None = None,
    ) -> dict:
        """Build a bounded timeline record for one episode."""
        _ = actions
        normalized_events = [
            normalized
            for raw_event in events
            if isinstance(raw_event, dict)
            for normalized in [self.normalize_event_record(raw_event, str(raw_event.get("_source_path") or "events.jsonl"))]
            if normalized is not None
        ]
        key_events = self.select_key_events(normalized_events)
        key_events.sort(key=lambda event: (event["time_s"] is None, event["time_s"] or 0.0, event["event_type"]))
        return {
            "experiment_id": experiment_id,
            "run_id": run_id,
            "episode_id": episode_id,
            "timeline": key_events,
            "timeline_summary": self._summary(key_events),
            "action_summary": self._compact_action_summary(action_summary),
        }

    def normalize_event_record(
        self,
        raw_event: dict,
        source_path: str,
    ) -> dict | None:
        event_type = self._first_text(raw_event, ("event_type", "type", "name", "event"))
        if not event_type:
            return None
        return {
            "time_s": self._first_number(raw_event, ("time_s", "timestamp_s", "t", "time")),
            "event_type": event_type,
            "summary": self._summary_text(raw_event, event_type),
            "source": source_path,
            "position_xy_m": self._first_value(raw_event, ("position_xy_m", "xy_m", "location_xy_m")),
        }

    def select_key_events(
        self,
        normalized_events: list[dict],
    ) -> list[dict]:
        return [event for event in normalized_events if event.get("event_type") in KEY_EVENT_TYPES]

    def _episode_inputs(self, parsed_artifacts: dict) -> dict[tuple[str, str, str], dict[str, Any]]:
        inputs: dict[tuple[str, str, str], dict[str, Any]] = {}
        for item in parsed_artifacts.get("episodes", []):
            if not isinstance(item, dict):
                continue
            key = self._episode_key(item)
            if not all(key):
                continue
            events = item.get("events") if isinstance(item.get("events"), list) else []
            actions = item.get("actions") if isinstance(item.get("actions"), list) else []
            action_summary = item.get("action_summary") if isinstance(item.get("action_summary"), dict) else None
            source_path = str(item.get("source_path") or "events.jsonl")
            inputs[key] = {
                "events": [
                    {**event, "_source_path": str(event.get("_source_path") or source_path)}
                    for event in events
                    if isinstance(event, dict)
                ],
                "actions": [action for action in actions if isinstance(action, dict)],
                "action_summary": action_summary,
            }
        return inputs

    def _episode_key(self, item: dict[str, Any]) -> tuple[str, str, str]:
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

    def _first_text(self, source: dict[str, Any], keys: tuple[str, ...]) -> str | None:
        value = self._first_value(source, keys)
        return str(value) if isinstance(value, str) and value else None

    def _first_number(self, source: dict[str, Any], keys: tuple[str, ...]) -> float | None:
        value = self._first_value(source, keys)
        return float(value) if isinstance(value, int | float) else None

    def _first_value(self, source: dict[str, Any], keys: tuple[str, ...]) -> Any:
        for key in keys:
            if key in source:
                return source[key]
        return None

    def _summary_text(self, raw_event: dict[str, Any], event_type: str) -> str:
        summary = raw_event.get("summary") or raw_event.get("message") or raw_event.get("description")
        return str(summary) if isinstance(summary, str) and summary else event_type.replace("_", " ")

    def _summary(self, events: list[dict]) -> str:
        if not events:
            return "핵심 이벤트가 없습니다."
        event_names = [str(event["event_type"]).replace("_", " ") for event in events[:3]]
        if len(event_names) == 1:
            return f"{event_names[0]} 이벤트가 발생했습니다."
        return f"{' 후 '.join(event_names)}이 발생했습니다."

    def _compact_action_summary(self, action_summary: dict[str, Any] | None) -> dict[str, Any] | None:
        """Return only action summary fields that are safe for analysis context."""
        if not isinstance(action_summary, dict):
            return None
        return {
            "actions_line_count": action_summary.get("actions_line_count", 0),
            "parsed_actions_line_count": action_summary.get("parsed_actions_line_count", 0),
            "broken_actions_line_count": action_summary.get("broken_actions_line_count", 0),
            "action_type_counts": action_summary.get("action_type_counts", {}),
            "decision_reason_counts": action_summary.get("decision_reason_counts", {}),
            "repath_action_count": action_summary.get("repath_action_count", 0),
            "slowdown_action_count": action_summary.get("slowdown_action_count", 0),
            "stop_action_count": action_summary.get("stop_action_count", 0),
            "follow_path_action_count": action_summary.get("follow_path_action_count", 0),
            "speed_stats": action_summary.get("speed_stats", {}),
            "path_error_stats": action_summary.get("path_error_stats", {}),
            "goal_distance_change": action_summary.get("goal_distance_change", {}),
            "first_timestamp": action_summary.get("first_timestamp"),
            "last_timestamp": action_summary.get("last_timestamp"),
            "truncated": action_summary.get("truncated", False),
        }
