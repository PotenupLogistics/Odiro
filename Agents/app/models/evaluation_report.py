from __future__ import annotations

from typing import Any

from pydantic import BaseModel, Field


class EvaluationReportSetupRef(BaseModel):
    path: str = ""
    hash: str = ""


class EvaluationReportRun(BaseModel):
    run_id: str = ""
    run_index: int = 0
    episode_id: str = ""
    pair_id: str = ""
    episode_setup: EvaluationReportSetupRef = Field(default_factory=EvaluationReportSetupRef)
    delivery_bot_setup: EvaluationReportSetupRef = Field(default_factory=EvaluationReportSetupRef)
    pair_hash: str = ""


class EvaluationReportSummary(BaseModel):
    completed: bool = False
    success: bool = False
    outcome: str = ""
    terminal_reason: str = ""
    duration_s: float = 0.0
    score: float = 0.0
    usable_for_llm_tuning: bool = False


class EvaluationReportPipeline(BaseModel):
    episode_setup_compiled: bool = False
    delivery_bot_setup_compiled: bool = False
    world_setup_succeeded: bool = False
    evaluation_completed: bool = False
    diagnostics: list[str] = Field(default_factory=list)


class EvaluationReportMetrics(BaseModel):
    goal_reached: int = 0
    score: float = 0.0
    duration_s: float = 0.0
    goal_distance_m: float = 0.0
    static_obstacle_collision_count: int = 0
    blocked_region_collision_count: int = 0
    penalty_region_violation_count: int = 0
    pedestrian_collision_count: int = 0
    near_miss_count: int = 0
    near_miss_total_duration_s: float = 0.0
    near_miss_min_distance_m: float | None = None
    robot_tip_over_count: int = 0
    delivery_bot_failure_type: str | None = None
    delivery_bot_failure_message: str = ""
    delivery_bot_failure_xy_m: list[float] | None = None
    delivery_bot_failure_time_s: float | None = None
    delivery_bot_failure_speed_kmh: float | None = None
    delivery_bot_failure_target_actor_name: str = ""


class EvaluationReportEventSummary(BaseModel):
    total: int = 0
    by_type: dict[str, int] = Field(default_factory=dict)
    by_severity: dict[str, int] = Field(default_factory=dict)
    first_failure_event_index: int | None = None


class EvaluationReportEvent(BaseModel):
    i: int
    t_s: float
    type: str
    severity: str
    subject: str = ""
    target: str = ""
    xy_m: list[float] = Field(default_factory=list)
    message: str = ""
    properties: dict[str, Any] = Field(default_factory=dict)


class EvaluationReport(BaseModel):
    schema_: str = Field(alias="schema", default="episode_evaluation_report")
    version: int = 1
    run: EvaluationReportRun = Field(default_factory=EvaluationReportRun)
    summary: EvaluationReportSummary = Field(default_factory=EvaluationReportSummary)
    pipeline: EvaluationReportPipeline = Field(default_factory=EvaluationReportPipeline)
    metrics: EvaluationReportMetrics = Field(default_factory=EvaluationReportMetrics)
    event_summary: EvaluationReportEventSummary = Field(default_factory=EvaluationReportEventSummary)
    events: list[EvaluationReportEvent] = Field(default_factory=list)

    model_config = {"populate_by_name": True}
