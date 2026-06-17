from __future__ import annotations

from typing import Any, Literal

from pydantic import BaseModel, ConfigDict, Field, field_validator


class ScenarioGenerateV2Request(BaseModel):
    model_config = ConfigDict(extra="forbid")

    prompt: str = Field(min_length=1)

    @field_validator("prompt")
    @classmethod
    def reject_blank_prompt(cls, value: str) -> str:
        if not value.strip():
            raise ValueError("prompt must not be empty.")
        return value


class V2ValidationIssue(BaseModel):
    model_config = ConfigDict(extra="forbid")

    field: str
    message: str


class V2ValidationResult(BaseModel):
    model_config = ConfigDict(extra="forbid")

    valid: bool
    errors: list[V2ValidationIssue] = Field(default_factory=list)
    warnings: list[V2ValidationIssue] = Field(default_factory=list)


class ProjectScenarioV1Response(BaseModel):
    """External Project Scenario v1 response body returned by the v2 generation endpoint."""

    model_config = ConfigDict(extra="forbid", populate_by_name=True)

    schema_: Literal["scenario"] = Field(alias="schema")
    version: Literal[1]
    scenario_id: str
    intent: str
    corridor: dict[str, Any]
    obstacles: dict[str, Any]
    pedestrians: dict[str, Any]
    robot: dict[str, Any]


class ScenarioGenerateV2Response(BaseModel):
    """Internal scenario generation response returned without file ownership decisions."""

    model_config = ConfigDict(extra="forbid", populate_by_name=True)

    status: Literal["success", "failed"]
    scenario_id: str | None = None
    summary: str
    scenario: dict[str, Any] | None = None
    validation: V2ValidationResult
    assumptions: list[str] = Field(default_factory=list)
    generation_mode: Literal["deterministic", "llm", "llm_repaired", "fallback", "langgraph"] = "deterministic"
