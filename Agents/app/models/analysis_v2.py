from __future__ import annotations

from typing import Any, Literal

from pydantic import BaseModel, ConfigDict, Field, field_validator


RecommendationTypeV2 = Literal["environment_review", "policy_review", "none", "insufficient_data"]


class AnalysisRunV2Request(BaseModel):
    model_config = ConfigDict(extra="forbid")

    project_path: str = Field(min_length=1)
    run_id: str = Field(pattern=r"^\d{6}$")
    prompt: str | None = Field(default=None, max_length=2_000)

    @field_validator("project_path")
    @classmethod
    def reject_blank_project_path(cls, value: str) -> str:
        if not value.strip():
            raise ValueError("project_path must not be empty.")
        return value

    @field_validator("prompt")
    @classmethod
    def normalize_prompt(cls, value: str | None) -> str | None:
        """Treat blank prompt text as an absent optional analysis focus."""
        if value is None:
            return None
        stripped = value.strip()
        return stripped or None


class AnalysisScopeV2(BaseModel):
    model_config = ConfigDict(extra="forbid")

    experiments_count: int = Field(ge=0)
    runs_count: int = Field(ge=0)
    episodes_count: int = Field(ge=0)


class AnalysisSummaryV2(BaseModel):
    model_config = ConfigDict(extra="forbid")

    overall_judgement: Literal["change_recommended", "no_change_needed", "insufficient_data"]
    message: str


class AnalysisMetricsV2(BaseModel):
    model_config = ConfigDict(extra="forbid")

    success_count: int = Field(ge=0)
    failure_count: int = Field(ge=0)
    collision_count: int = Field(ge=0)
    static_obstacle_collision_count: int = Field(default=0, ge=0)
    pedestrian_collision_count: int = Field(default=0, ge=0)
    near_miss_count: int = Field(ge=0)
    repath_count: int = Field(default=0, ge=0)
    robot_tip_over_count: int = Field(default=0, ge=0)
    blocked_region_violation_count: int = Field(ge=0)
    penalty_region_violation_count: int = Field(ge=0)


class AnalysisRunOverviewDisplayV2(BaseModel):
    """UI-ready text for the run overview cards."""

    model_config = ConfigDict(extra="forbid")

    total_play_time: str
    success_rate: str
    collision_count: str


class AnalysisRunOverviewV2(BaseModel):
    """Run-level dashboard values calculated from summary.json rows."""

    model_config = ConfigDict(extra="forbid")

    total_play_time_s: float = Field(ge=0)
    success_rate: float = Field(ge=0, le=1)
    collision_count: int = Field(ge=0)
    episode_count: int = Field(ge=0)
    display: AnalysisRunOverviewDisplayV2


class AnalysisEpisodeDisplayV2(BaseModel):
    """UI-ready text for an episode replay row."""

    model_config = ConfigDict(extra="forbid")

    duration: str
    outcome: str


class AnalysisEpisodeV2(BaseModel):
    """Public episode summary generated from one summary.json row."""

    model_config = ConfigDict(extra="forbid")

    episode_id: str
    duration_s: float = Field(ge=0)
    outcome: Literal["success", "failure"]
    display: AnalysisEpisodeDisplayV2


class AnalysisInsightV2(BaseModel):
    """Compact public insight card for the UI."""

    model_config = ConfigDict(extra="forbid")

    severity: Literal["high", "medium", "low"]
    title: str
    description: str


class AnalysisErrorV2(BaseModel):
    """Failure details returned only when the API cannot build a valid response."""

    model_config = ConfigDict(extra="forbid")

    code: str
    message: str
    phase: str


class AnalysisRunV2Response(BaseModel):
    model_config = ConfigDict(extra="forbid", populate_by_name=True)

    schema_: Literal["analysis_run_response_v2"] = Field(default="analysis_run_response_v2", alias="schema")
    version: Literal[2] = 2
    status: Literal["success", "failed"] = "success"
    run_id: str | None = Field(default=None, description="Requested run id when the response is scoped to one user project run.")
    review_id: str | None = Field(default=None, description="Generated review id, or null when no review directory was created.")
    run_overview: AnalysisRunOverviewV2 | None = None
    episodes: list[AnalysisEpisodeV2] | None = None
    analysis_scope: AnalysisScopeV2 | None = None
    summary: AnalysisSummaryV2 | None = None
    metrics: AnalysisMetricsV2 | None = None
    recommendation_type: RecommendationTypeV2 | None = None
    insights: list[AnalysisInsightV2] | None = None
    patterns: list[dict[str, Any]] | None = None
    recommendations: list[dict[str, Any]] | None = None
    warnings: list[str] = Field(default_factory=list)
    error: AnalysisErrorV2 | None = None
