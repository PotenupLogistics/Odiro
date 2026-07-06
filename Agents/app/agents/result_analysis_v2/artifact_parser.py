from __future__ import annotations

from collections import Counter
import json
from dataclasses import dataclass
from pathlib import Path
from typing import Any

from app.agents.result_analysis_v2.artifact_classifier import ArtifactInfo


# Stable marker for compact action summaries stored in review artifacts.
ACTIONS_STREAMING_SUMMARY_TYPE = "actions_streaming_summary"

# Defensive field names observed across action log variants.
ACTION_TYPE_KEYS = ("action", "action_type", "type", "command")

# Defensive field names for action decision reasons.
ACTION_REASON_KEYS = ("reason", "decision_reason", "cause")

# Defensive field names for velocity-like values in action records.
ACTION_SPEED_KEYS = ("speed", "speed_mps", "velocity")

# Defensive field names for action timestamps.
ACTION_TIMESTAMP_KEYS = ("timestamp", "time_s", "t")

# Defensive field names for path tracking error values.
ACTION_PATH_ERROR_KEYS = ("path_error", "path_error_m")

# Defensive field names for remaining goal distance values.
ACTION_GOAL_DISTANCE_KEYS = ("goal_distance", "goal_distance_m", "distance_to_goal")

# Maximum distinct counter entries kept in compact action summaries.
ACTION_SUMMARY_COUNTER_LIMIT = 10


@dataclass(frozen=True)
class ParsedArtifact:
    info: ArtifactInfo
    data: Any
    warnings: list[str]


class ArtifactParser:
    """Parses classified run artifacts into bounded in-memory structures."""

    def parse(self, info: ArtifactInfo) -> ParsedArtifact:
        """Parse one artifact, summarizing large action logs instead of storing rows."""
        if info.artifact_type in {"episode_preview", "episode_capture"}:
            return ParsedArtifact(info=info, data=self._metadata(info.path), warnings=[])
        if info.artifact_type == "episode_actions":
            return self._parse_actions_summary(info)
        if info.path.suffix.lower() == ".json":
            return self._parse_json(info)
        if info.path.suffix.lower() == ".jsonl":
            return self._parse_jsonl(info)
        if info.path.suffix.lower() == ".py":
            return self._parse_text(info)
        return ParsedArtifact(info=info, data=None, warnings=[])

    def _parse_json(self, info: ArtifactInfo) -> ParsedArtifact:
        try:
            return ParsedArtifact(info=info, data=json.loads(info.path.read_text(encoding="utf-8-sig")), warnings=[])
        except Exception as exc:
            return ParsedArtifact(info=info, data=None, warnings=[f"{info.relative_path}: JSON parse failed: {exc}"])

    def _parse_jsonl(self, info: ArtifactInfo) -> ParsedArtifact:
        """Parse small JSONL artifacts into row lists for existing event consumers."""
        rows: list[Any] = []
        warnings: list[str] = []
        try:
            lines = info.path.read_text(encoding="utf-8-sig").splitlines()
        except Exception as exc:
            return ParsedArtifact(info=info, data=[], warnings=[f"{info.relative_path}: read failed: {exc}"])

        for line_number, line in enumerate(lines, start=1):
            if not line.strip():
                continue
            try:
                rows.append(json.loads(line))
            except json.JSONDecodeError as exc:
                warnings.append(f"{info.relative_path}:{line_number}: JSONL parse failed: {exc.msg}")
        return ParsedArtifact(info=info, data=rows, warnings=warnings)

    def _parse_actions_summary(self, info: ArtifactInfo) -> ParsedArtifact:
        """Stream actions.jsonl and retain only aggregate behavior statistics."""
        summary = self._empty_actions_summary()
        speed_stats = self._new_numeric_accumulator()
        path_error_stats = self._new_numeric_accumulator()
        action_type_counts: Counter[str] = Counter()
        decision_reason_counts: Counter[str] = Counter()
        warnings: list[str] = []
        first_goal_distance: float | None = None
        last_goal_distance: float | None = None

        try:
            with info.path.open("r", encoding="utf-8-sig", errors="replace") as handle:
                for line in handle:
                    if not line.strip():
                        continue
                    summary["actions_line_count"] += 1
                    try:
                        record = json.loads(line)
                    except json.JSONDecodeError:
                        summary["broken_actions_line_count"] += 1
                        continue
                    if not isinstance(record, dict):
                        summary["broken_actions_line_count"] += 1
                        continue

                    summary["parsed_actions_line_count"] += 1
                    action_type = self._canonical_action_type(self._first_text(record, ACTION_TYPE_KEYS))
                    if action_type:
                        action_type_counts[action_type] += 1
                        self._increment_action_category(summary, action_type)
                    reason = self._normalize_token(self._first_text(record, ACTION_REASON_KEYS))
                    if reason:
                        decision_reason_counts[reason] += 1
                    self._update_numeric_stats(speed_stats, self._first_number(record, ACTION_SPEED_KEYS))
                    self._update_numeric_stats(path_error_stats, self._first_number(record, ACTION_PATH_ERROR_KEYS))

                    goal_distance = self._first_number(record, ACTION_GOAL_DISTANCE_KEYS)
                    if goal_distance is not None:
                        if first_goal_distance is None:
                            first_goal_distance = goal_distance
                        last_goal_distance = goal_distance
                    timestamp = self._first_timestamp(record, ACTION_TIMESTAMP_KEYS)
                    if timestamp is not None:
                        if summary["first_timestamp"] is None:
                            summary["first_timestamp"] = timestamp
                        summary["last_timestamp"] = timestamp
        except Exception as exc:
            return ParsedArtifact(info=info, data=summary, warnings=[f"{info.relative_path}: read failed: {exc}"])

        summary["action_type_counts"] = self._top_counter_items(action_type_counts, ACTION_SUMMARY_COUNTER_LIMIT)
        summary["decision_reason_counts"] = self._top_counter_items(decision_reason_counts, ACTION_SUMMARY_COUNTER_LIMIT)
        summary["speed_stats"] = self._finalize_numeric_stats(speed_stats)
        summary["path_error_stats"] = self._finalize_numeric_stats(path_error_stats)
        summary["goal_distance_change"] = self._goal_distance_change(first_goal_distance, last_goal_distance)
        if summary["broken_actions_line_count"]:
            warnings.append(
                f"{info.relative_path}: action summary skipped {summary['broken_actions_line_count']} broken JSONL line(s)."
            )
        return ParsedArtifact(info=info, data=summary, warnings=warnings)

    def _empty_actions_summary(self) -> dict[str, Any]:
        """Return the default shape for action summaries used by report artifacts."""
        return {
            "summary_type": ACTIONS_STREAMING_SUMMARY_TYPE,
            "actions_line_count": 0,
            "parsed_actions_line_count": 0,
            "broken_actions_line_count": 0,
            "action_type_counts": {},
            "decision_reason_counts": {},
            "repath_action_count": 0,
            "slowdown_action_count": 0,
            "stop_action_count": 0,
            "follow_path_action_count": 0,
            "speed_stats": {"count": 0, "min": None, "max": None, "avg": None},
            "path_error_stats": {"count": 0, "min": None, "max": None, "avg": None},
            "goal_distance_change": {"first": None, "last": None, "delta": None},
            "first_timestamp": None,
            "last_timestamp": None,
            "truncated": False,
        }

    def _increment_action_category(self, summary: dict[str, Any], action_type: str) -> None:
        """Increment category counters used by policy-review evidence rules."""
        if "repath" in action_type or "replan" in action_type:
            summary["repath_action_count"] += 1
        if "slowdown" in action_type or "slow_down" in action_type:
            summary["slowdown_action_count"] += 1
        if "stop" in action_type:
            summary["stop_action_count"] += 1
        if action_type == "follow_path" or ("follow" in action_type and "path" in action_type):
            summary["follow_path_action_count"] += 1

    def _canonical_action_type(self, value: str | None) -> str | None:
        """Normalize action names while preserving unknown action categories."""
        normalized = self._normalize_token(value)
        if normalized in {"slow_down", "slowdown"} or normalized.startswith("slow_down"):
            return "slowdown"
        if normalized in {"followpath", "follow_path"}:
            return "follow_path"
        if normalized in {"replan", "repath", "re_path"}:
            return "repath"
        return normalized

    def _normalize_token(self, value: str | None) -> str | None:
        """Normalize a text token for stable count keys."""
        if value is None:
            return None
        normalized = value.strip().casefold().replace("-", "_").replace(" ", "_")
        return normalized or None

    def _new_numeric_accumulator(self) -> dict[str, float | int | None]:
        """Create an internal accumulator for min/max/average stats."""
        return {"count": 0, "min": None, "max": None, "sum": 0.0}

    def _update_numeric_stats(self, stats: dict[str, float | int | None], value: float | None) -> None:
        """Fold one numeric sample into an accumulator."""
        if value is None:
            return
        stats["count"] = int(stats["count"] or 0) + 1
        stats["sum"] = float(stats["sum"] or 0.0) + value
        stats["min"] = value if stats["min"] is None else min(float(stats["min"]), value)
        stats["max"] = value if stats["max"] is None else max(float(stats["max"]), value)

    def _finalize_numeric_stats(self, stats: dict[str, float | int | None]) -> dict[str, float | int | None]:
        """Convert an accumulator into the persisted public-report shape."""
        count = int(stats["count"] or 0)
        if count == 0:
            return {"count": 0, "min": None, "max": None, "avg": None}
        return {
            "count": count,
            "min": stats["min"],
            "max": stats["max"],
            "avg": round(float(stats["sum"] or 0.0) / count, 6),
        }

    def _goal_distance_change(self, first: float | None, last: float | None) -> dict[str, float | None]:
        """Return first/last/delta goal distance values when available."""
        delta = round(last - first, 6) if first is not None and last is not None else None
        return {"first": first, "last": last, "delta": delta}

    def _first_text(self, source: dict[str, Any], keys: tuple[str, ...]) -> str | None:
        """Return the first non-empty text-like value from candidate keys."""
        value = self._first_value(source, keys)
        if value is None:
            return None
        text = str(value).strip()
        return text or None

    def _first_number(self, source: dict[str, Any], keys: tuple[str, ...]) -> float | None:
        """Return the first numeric value from candidate keys."""
        value = self._first_value(source, keys)
        if isinstance(value, bool) or value is None:
            return None
        if isinstance(value, int | float):
            return float(value)
        if isinstance(value, str):
            try:
                return float(value.strip())
            except ValueError:
                return None
        return None

    def _first_timestamp(self, source: dict[str, Any], keys: tuple[str, ...]) -> float | str | None:
        """Return the first timestamp as a number when possible, otherwise text."""
        value = self._first_value(source, keys)
        if isinstance(value, bool) or value is None:
            return None
        if isinstance(value, int | float):
            return float(value)
        if isinstance(value, str) and value.strip():
            try:
                return float(value.strip())
            except ValueError:
                return value.strip()
        return None

    def _first_value(self, source: dict[str, Any], keys: tuple[str, ...]) -> Any:
        """Read the first present value from candidate keys."""
        for key in keys:
            if key in source:
                return source[key]
        return None

    def _top_counter_items(self, counter: Counter[str], limit: int) -> dict[str, int]:
        """Return a count-descending, name-stable bounded counter mapping."""
        return {
            key: value
            for key, value in sorted(counter.items(), key=lambda item: (-item[1], item[0]))[:limit]
        }

    def _parse_text(self, info: ArtifactInfo) -> ParsedArtifact:
        try:
            text = info.path.read_text(encoding="utf-8")
        except Exception as exc:
            return ParsedArtifact(info=info, data="", warnings=[f"{info.relative_path}: read failed: {exc}"])
        return ParsedArtifact(info=info, data=text[:20_000], warnings=[])

    def _metadata(self, path: Path) -> dict[str, Any]:
        stat = path.stat()
        return {"path": str(path), "size_bytes": stat.st_size, "modified_time": stat.st_mtime}
