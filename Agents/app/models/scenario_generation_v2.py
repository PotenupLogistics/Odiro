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


class ScenarioGenerateV2Response(BaseModel):
    model_config = ConfigDict(extra="forbid", populate_by_name=True)

    schema_: Literal["scenario_generate_response_v2"] = Field(
        default="scenario_generate_response_v2",
        alias="schema",
    )
    version: Literal[2] = 2
    status: Literal["success", "failed"]
    scenario_id: str | None = None
    summary: str
    scenario_template: dict[str, Any] | None = None
    validation: V2ValidationResult
    assumptions: list[str] = Field(default_factory=list)
    generation_mode: Literal["deterministic", "llm", "llm_repaired", "fallback"] = "deterministic"
