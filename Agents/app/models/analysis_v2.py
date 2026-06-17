from __future__ import annotations

from typing import Any, Literal

from pydantic import BaseModel, ConfigDict, Field, field_validator


class AnalysisRunV2Request(BaseModel):
    model_config = ConfigDict(extra="forbid")

    project_path: str = Field(min_length=1)
    run_id: str = Field(pattern=r"^\d{6}$")

    @field_validator("project_path")
    @classmethod
    def reject_blank_project_path(cls, value: str) -> str:
        if not value.strip():
            raise ValueError("project_path must not be empty.")
        return value


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
    near_miss_count: int = Field(ge=0)
    blocked_region_violation_count: int = Field(ge=0)
    penalty_region_violation_count: int = Field(ge=0)


class AnalysisRunV2Response(BaseModel):
    model_config = ConfigDict(extra="forbid", populate_by_name=True)

    schema_: Literal["analysis_run_response_v2"] = Field(default="analysis_run_response_v2", alias="schema")
    version: Literal[2] = 2
    status: Literal["success", "failed"] = "success"
    analysis_scope: AnalysisScopeV2
    summary: AnalysisSummaryV2
    metrics: AnalysisMetricsV2
    patterns: list[dict[str, Any]] = Field(default_factory=list)
    recommendations: list[dict[str, Any]] = Field(default_factory=list)
    modified_policy_json: list[dict[str, Any]] = Field(default_factory=list)
    modified_environment_json: list[dict[str, Any]] = Field(default_factory=list)
    warnings: list[str] = Field(default_factory=list)
    analysis_mode: Literal["rule_based", "llm", "fallback"] = "rule_based"
