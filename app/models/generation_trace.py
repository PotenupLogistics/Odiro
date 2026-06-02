from __future__ import annotations

from datetime import UTC, datetime
from enum import Enum
from typing import Any, Literal
from uuid import uuid4

from pydantic import BaseModel, ConfigDict, Field


class TraceSourceType(str, Enum):
    user_prompt = "user_prompt"
    scenario_intent = "scenario_intent"
    environment_sampling = "environment_sampling"
    policy_rag = "policy_rag"
    placement_rule = "placement_rule"
    world_config_schema = "world_config_schema"
    post_processing = "post_processing"
    episode_spec_adapter = "episode_spec_adapter"
    validation = "validation"
    scenario_reflection = "scenario_reflection"


class GenerationTraceItem(BaseModel):
    model_config = ConfigDict(extra="forbid")

    sourceType: TraceSourceType
    fieldPath: str
    valueSummary: str | int | float | bool | None = None
    evidence: str | None = None
    rule: str | None = None
    reason: str | None = None
    priority: int = Field(default=0, ge=0, le=10)
    inputs: dict[str, Any] = Field(default_factory=dict)
    calculation: str | None = None


class GenerationTrace(BaseModel):
    model_config = ConfigDict(extra="forbid")

    traceId: str = Field(default_factory=lambda: f"TRACE-{uuid4().hex[:12]}")
    requestId: str
    createdAt: str = Field(default_factory=lambda: datetime.now(UTC).isoformat())
    summary: dict[str, Any] | str
    evidenceItems: list[GenerationTraceItem] = Field(default_factory=list)
    warnings: list[str] = Field(default_factory=list)


class GenerationTraceSummary(BaseModel):
    model_config = ConfigDict(extra="forbid")

    message: str
    status: Literal["success", "failed", "partial"] = "partial"
    failureStage: str | None = None
    errorSummary: str | None = None
    coordinateSource: str | None = None
    policyRagUsedFor: str | None = None
