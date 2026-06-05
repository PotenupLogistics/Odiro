from __future__ import annotations

from app.models.evaluation_report import EvaluationReport
from app.models.recommendation import EvaluationReportStatistics


def extract_statistics(report: EvaluationReport) -> EvaluationReportStatistics:
    m = report.metrics
    s = report.summary
    return EvaluationReportStatistics(
        outcome=s.outcome,
        terminal_reason=s.terminal_reason,
        duration_s=s.duration_s,
        score=s.score,
        goal_reached=m.goal_reached > 0,
        static_obstacle_collision_count=m.static_obstacle_collision_count,
        blocked_region_collision_count=m.blocked_region_collision_count,
        pedestrian_collision_count=m.pedestrian_collision_count,
        near_miss_count=m.near_miss_count,
        near_miss_min_distance_m=m.near_miss_min_distance_m,
        robot_tip_over_count=m.robot_tip_over_count,
        failure_type=m.delivery_bot_failure_type,
        failure_speed_kmh=m.delivery_bot_failure_speed_kmh,
        event_counts_by_type=dict(report.event_summary.by_type),
        diagnostics=list(report.pipeline.diagnostics),
        usable_for_llm_tuning=s.usable_for_llm_tuning,
    )
