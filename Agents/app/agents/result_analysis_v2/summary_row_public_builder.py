"""Builds public analysis response fields from run summary rows."""

from __future__ import annotations

from dataclasses import dataclass
from math import floor, sqrt
from typing import Any

from app.models.analysis_v2 import (
    AnalysisEpisodeV2,
    AnalysisMetricsV2,
    AnalysisRunOverviewDisplayV2,
    AnalysisRunOverviewV2,
)


# UE dashboard tolerance for setup-time GoalReached rows that should not count as success.
INSTANT_GOAL_DURATION_TOLERANCE_S = 0.1
# UE dashboard tolerance added to the configured goal radius.
GOAL_DISTANCE_TOLERANCE_M = 0.001


@dataclass(frozen=True)
class SummaryRowsPublicData:
    """Public dashboard values derived from summary.json rows."""

    run_overview: AnalysisRunOverviewV2 | None
    episodes: list[AnalysisEpisodeV2]
    metrics: AnalysisMetricsV2
    has_rows: bool


class SummaryRowPublicBuilder:
    """Mirrors the UE project run dashboard aggregation for public API fields."""

    def build(self, rows: list[dict[str, Any]]) -> SummaryRowsPublicData:
        """Return overview, episode rows, and metrics calculated from summary rows."""
        valid_rows = [row for row in rows if isinstance(row, dict)]
        if not valid_rows:
            return SummaryRowsPublicData(
                run_overview=None,
                episodes=[],
                metrics=self._metrics(
                    episode_count=0,
                    success_count=0,
                    collision_count=0,
                    detail_counts={},
                ),
                has_rows=False,
            )

        episodes: list[AnalysisEpisodeV2] = []
        total_play_time_s = 0.0
        success_count = 0
        collision_count = 0
        detail_counts = {
            "static_obstacle_collision_count": 0,
            "pedestrian_collision_count": 0,
            "near_miss_count": 0,
            "repath_count": 0,
            "robot_tip_over_count": 0,
            "blocked_region_violation_count": 0,
            "penalty_region_violation_count": 0,
        }

        for row in valid_rows:
            duration_s = max(0.0, self._duration_s(row))
            is_success = self._is_success_row(row=row, duration_s=duration_s)
            total_play_time_s += duration_s
            success_count += 1 if is_success else 0
            collision_count += self._primary_collision_count(row)
            for key in detail_counts:
                detail_counts[key] += self._metric_int(row, key)
            episodes.append(
                AnalysisEpisodeV2(
                    episode_id=str(row.get("episode_id") or ""),
                    duration_s=duration_s,
                    outcome="success" if is_success else "failure",
                    display={
                        "duration": f"{duration_s:.1f} s",
                        "outcome": "성공" if is_success else "실패",
                    },
                )
            )

        episode_count = len(valid_rows)
        success_rate = success_count / episode_count if episode_count else 0.0
        overview = AnalysisRunOverviewV2(
            total_play_time_s=total_play_time_s,
            success_rate=success_rate,
            collision_count=collision_count,
            episode_count=episode_count,
            display=AnalysisRunOverviewDisplayV2(
                total_play_time=self._format_total_play_time(total_play_time_s),
                success_rate=self._format_success_rate(success_count=success_count, episode_count=episode_count),
                collision_count=f"{collision_count}회",
            ),
        )
        return SummaryRowsPublicData(
            run_overview=overview,
            episodes=episodes,
            metrics=self._metrics(
                episode_count=episode_count,
                success_count=success_count,
                collision_count=collision_count,
                detail_counts=detail_counts,
            ),
            has_rows=True,
        )

    def _metrics(
        self,
        *,
        episode_count: int,
        success_count: int,
        collision_count: int,
        detail_counts: dict[str, int],
    ) -> AnalysisMetricsV2:
        """Create the public metrics object from UE-compatible aggregate counts."""
        return AnalysisMetricsV2(
            success_count=success_count,
            failure_count=max(0, episode_count - success_count),
            collision_count=collision_count,
            static_obstacle_collision_count=max(0, detail_counts.get("static_obstacle_collision_count", 0)),
            pedestrian_collision_count=max(0, detail_counts.get("pedestrian_collision_count", 0)),
            near_miss_count=max(0, detail_counts.get("near_miss_count", 0)),
            repath_count=max(0, detail_counts.get("repath_count", 0)),
            robot_tip_over_count=max(0, detail_counts.get("robot_tip_over_count", 0)),
            blocked_region_violation_count=max(0, detail_counts.get("blocked_region_violation_count", 0)),
            penalty_region_violation_count=max(0, detail_counts.get("penalty_region_violation_count", 0)),
        )

    def _is_success_row(self, *, row: dict[str, Any], duration_s: float) -> bool:
        """Return the UE IsSuccessRow result for one summary row."""
        terminal_reason = str(row.get("terminal_reason") or "")
        if self._is_immediate_goal_reached_row(row=row, terminal_reason=terminal_reason, duration_s=duration_s):
            return False
        outcome = str(row.get("outcome") or "")
        if outcome.casefold() == "success".casefold():
            return True
        return self._metric_number(row, "goal_reached") > 0.0

    def _is_immediate_goal_reached_row(
        self,
        *,
        row: dict[str, Any],
        terminal_reason: str,
        duration_s: float,
    ) -> bool:
        """Detect setup-time GoalReached rows that the UE dashboard excludes from success."""
        if terminal_reason.casefold() != "GoalReached".casefold():
            return False
        return duration_s <= INSTANT_GOAL_DURATION_TOLERANCE_S or self._start_inside_goal_radius(row)

    def _start_inside_goal_radius(self, row: dict[str, Any]) -> bool:
        """Return whether summary semantic data places the start inside the goal radius."""
        goal_threshold_m = self._metric_number(row, "goal_threshold_m", default=-1.0)
        if goal_threshold_m <= 0.0:
            return False
        semantic = row.get("scenario_semantic")
        robot = semantic.get("robot") if isinstance(semantic, dict) else None
        start = robot.get("start") if isinstance(robot, dict) else None
        goal = robot.get("goal") if isinstance(robot, dict) else None
        if not isinstance(start, dict) or not isinstance(goal, dict):
            return False
        start_segment = str(start.get("segment") or "")
        goal_segment = str(goal.get("segment") or "")
        if not start_segment or start_segment != goal_segment:
            return False
        delta_along_m = self._number(start.get("along_m")) - self._number(goal.get("along_m"))
        delta_offset_m = self._number(start.get("offset_m")) - self._number(goal.get("offset_m"))
        return sqrt(delta_along_m * delta_along_m + delta_offset_m * delta_offset_m) <= (
            goal_threshold_m + GOAL_DISTANCE_TOLERANCE_M
        )

    def _primary_collision_count(self, row: dict[str, Any]) -> int:
        """Return the UE primary collision count for one summary row."""
        return (
            self._metric_int(row, "blocked_region_collision_count")
            + self._metric_int(row, "pedestrian_collision_count")
            + self._metric_int(row, "static_obstacle_collision_count")
        )

    def _duration_s(self, row: dict[str, Any]) -> float:
        """Read row duration using the UE summary-row fallback order."""
        if isinstance(row.get("duration_s"), int | float):
            return float(row["duration_s"])
        return self._metric_number(row, "duration_s")

    def _metric_int(self, row: dict[str, Any], key: str) -> int:
        """Read a non-negative integer metric using UE-style positive rounding."""
        return max(0, self._round_half_up(self._metric_number(row, key)))

    def _metric_number(self, row: dict[str, Any], key: str, *, default: float = 0.0) -> float:
        """Read a numeric value from row.metrics."""
        metrics = row.get("metrics")
        if not isinstance(metrics, dict):
            return default
        return self._number(metrics.get(key), default=default)

    def _number(self, value: Any, *, default: float = 0.0) -> float:
        """Convert JSON numeric and boolean values to a float."""
        if isinstance(value, bool):
            return 1.0 if value else 0.0
        if isinstance(value, int | float):
            return float(value)
        return default

    def _format_total_play_time(self, duration_s: float) -> str:
        """Format total play time with the UE menu duration rules."""
        rounded_seconds = max(0, self._round_half_up(duration_s))
        hours = rounded_seconds // 3600
        minutes = (rounded_seconds % 3600) // 60
        seconds = rounded_seconds % 60
        if hours > 0:
            return f"{hours}:{minutes:02d}:{seconds:02d}"
        return f"{minutes:02d}:{seconds:02d}"

    def _format_success_rate(self, *, success_count: int, episode_count: int) -> str:
        """Format success rate as a percent string with UE-style rounding."""
        if episode_count <= 0:
            return "0%"
        percent = self._round_half_up(max(0.0, success_count * 100.0 / episode_count))
        percent = min(100, max(0, percent))
        return f"{percent}%"

    def _round_half_up(self, value: float) -> int:
        """Round non-negative values the same way UE displays dashboard integers."""
        return int(floor(value + 0.5))
