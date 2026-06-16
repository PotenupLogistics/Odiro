from __future__ import annotations

from enum import Enum
from typing import Any, Literal

from pydantic import BaseModel, ConfigDict, Field


class LlmProvider(str, Enum):
    disabled = "disabled"
    openai = "openai"
    gemini = "gemini"
    ollama = "ollama"
    custom = "custom"


class LlmUsage(BaseModel):
    model_config = ConfigDict(extra="forbid")

    promptTokens: int | None = None
    completionTokens: int | None = None
    totalTokens: int | None = None


class LlmError(BaseModel):
    model_config = ConfigDict(extra="forbid")

    code: str
    message: str


class LlmGenerationRequest(BaseModel):
    model_config = ConfigDict(extra="forbid")

    provider: LlmProvider
    model: str
    systemPrompt: str
    userPrompt: str
    temperature: float = Field(default=0.0, ge=0.0, le=2.0)
    maxTokens: int = Field(default=2048, ge=1)
    responseFormat: Literal["json_object", "text"] = "json_object"
    responseJsonSchema: dict[str, Any] | None = None
    responseSchemaName: str | None = None
    responseSchemaStrict: bool = False
    requestId: str
    timeoutSec: int | None = Field(default=None, ge=1)


class LlmGenerationResponse(BaseModel):
    model_config = ConfigDict(extra="forbid")

    requestId: str
    provider: LlmProvider
    model: str
    success: bool
    content: str | None = None
    rawContent: str | None = None
    usage: LlmUsage | None = None
    error: LlmError | None = None
    warnings: list[str] = Field(default_factory=list)


class LlmProviderStatus(BaseModel):
    model_config = ConfigDict(extra="forbid")

    provider: LlmProvider
    available: bool
    reason: str
    model: str | None = None


class LlmFallbackDecision(BaseModel):
    model_config = ConfigDict(extra="forbid")

    shouldFallback: bool
    fromProvider: LlmProvider | None = None
    toProvider: LlmProvider | None = None
    reason: str
    costSensitive: bool = True


class LlmProviderChainPlan(BaseModel):
    model_config = ConfigDict(extra="forbid")

    providers: list[LlmProvider]
    statuses: list[LlmProviderStatus]
    allowOpenaiFallback: bool
    maxTotalAttempts: int
