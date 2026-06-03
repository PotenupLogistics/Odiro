from __future__ import annotations

from typing import Literal

from pydantic import BaseModel, ConfigDict, Field

from app.models.run_result import RunMetrics


PolicyParamName = Literal[
    "stopDistanceM",
    "slowDownDistanceM",
    "maxSpeedKmh",
    "canRepath",
]


class AnalysisStatistics(BaseModel):
    model_config = ConfigDict(extra="forbid")

    totalTicks: int = Field(ge=0)
    deliveryTimeSec: float = Field(ge=0)
    closeReason: str
    diagnosticsWarningCount: int = Field(ge=0)

    avgFrontDistanceM: float = Field(ge=0)
    minFrontDistanceM: float = Field(ge=0)
    medianFrontDistanceM: float = Field(ge=0)
    frontObjectDetectionRate: float = Field(ge=0, le=1)

    actionReasonFreq: dict[str, int]
    brakeAppliedCount: int = Field(ge=0)
    brakeAppliedRatio: float = Field(ge=0, le=1)

    avgTargetSpeedKmh: float = Field(ge=0)
    targetSpeedVariance: float = Field(ge=0)
    avgActualSpeedKmh: float = Field(ge=0)

    stopActionRatio: float = Field(ge=0, le=1)
    slowdownActionRatio: float = Field(ge=0, le=1)
    repathActionRatio: float = Field(ge=0, le=1)


class ParamRecommendation(BaseModel):
    model_config = ConfigDict(extra="forbid")

    param: PolicyParamName
    current: float | bool
    suggested: float | bool
    reason: str
    citations: list[str] = Field(default_factory=list)


GenerationMethod = Literal["llm_rag", "fallback_rules", "fallback_after_llm_failure"]


class PolicyRecommendationResult(BaseModel):
    model_config = ConfigDict(extra="forbid")

    schemaVersion: str = "1.0.0"
    analysisId: str
    logPath: str
    generatedAt: str

    runMetrics: RunMetrics
    statistics: AnalysisStatistics
    recommendations: list[ParamRecommendation]
    summary: str

    generationMethod: GenerationMethod
    llmWarnings: list[str] = Field(default_factory=list)
