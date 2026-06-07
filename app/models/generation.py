from __future__ import annotations

from typing import Any, Literal

from pydantic import BaseModel, ConfigDict, Field

from app.models.scenario import (
    ScenarioIntent,
    ScenarioPostProcessResult,
    ScenarioReflectionResult,
    ScenarioRequirement,
)


class WorldConfigGenerationConstraints(BaseModel):
    model_config = ConfigDict(extra="forbid")

    unitSystem: str
    allowedMapTypes: list[str] = Field(default_factory=list)
    allowedObjectTypes: list[str] = Field(default_factory=list)
    fixedPolicyId: str | None = None
    defaultSeed: int | None = None
    requireValidation: bool = True
    environmentSampling: dict[str, Any] | None = None
    semanticFixedConstraints: dict[str, Any] | None = None


class WorldConfigGenerationRequest(BaseModel):
    model_config = ConfigDict(extra="forbid")

    schemaVersion: str
    requestId: str
    generationType: Literal["world_config"]
    prompt: str
    targetContractType: Literal["world_config"]
    policyId: str
    constraints: WorldConfigGenerationConstraints
    maxRepairAttempts: int = Field(default=2, ge=0, le=5)


class RetrievedPolicyContext(BaseModel):
    model_config = ConfigDict(extra="forbid")

    chunkId: str
    cardId: str
    category: str
    evidenceLocation: str
    relatedActions: list[str]
    relatedPolicyParams: list[str]
    shortText: str
    score: float


class WorldConfigPromptPackage(BaseModel):
    model_config = ConfigDict(extra="forbid")

    requestId: str
    systemPrompt: str
    userPrompt: str
    retrievedContexts: list[RetrievedPolicyContext]
    schemaSummary: dict[str, Any]
    validationPolicy: str
    scenarioIntent: ScenarioIntent | None = None
    scenarioRequirements: list[ScenarioRequirement] = Field(default_factory=list)
    environmentSampling: dict[str, Any] | None = None
    warnings: list[str] = Field(default_factory=list)


class WorldConfigRepairPromptPackage(BaseModel):
    model_config = ConfigDict(extra="forbid")

    requestId: str
    systemPrompt: str
    repairPrompt: str
    invalidPayload: dict[str, Any]
    validationErrors: list[str]
    scenarioRequirements: list[ScenarioRequirement] = Field(default_factory=list)
    validationPolicy: str
    warnings: list[str] = Field(default_factory=list)


class WorldConfigGenerationPlan(BaseModel):
    model_config = ConfigDict(extra="forbid")

    requestId: str
    normalizedPrompt: str
    retrievalQueries: list[str]
    retrievedContexts: list[RetrievedPolicyContext]
    promptPackage: WorldConfigPromptPackage
    maxRepairAttempts: int


class WorldConfigGenerationError(BaseModel):
    model_config = ConfigDict(extra="forbid")

    code: str
    message: str


class WorldConfigValidationSummary(BaseModel):
    model_config = ConfigDict(extra="forbid")

    status: Literal["skipped", "passed", "failed"]
    errors: list[str] = Field(default_factory=list)
    warnings: list[str] = Field(default_factory=list)


class WorldConfigFallbackTrace(BaseModel):
    model_config = ConfigDict(extra="forbid")

    fromProvider: str
    toProvider: str
    reason: str
    errorCode: str
    success: bool | None = None


class WorldConfigGenerationAttempt(BaseModel):
    model_config = ConfigDict(extra="forbid")

    attemptNumber: int
    promptType: Literal["initial", "schema_repair", "repair", "scenario_repair"]
    llmSuccess: bool
    rawContent: str | None = None
    rawContentPreview: str | None = None
    rawContentLength: int = 0
    extractedJson: dict[str, Any] | None = None
    extractedJsonPreview: dict[str, Any] | None = None
    extractedJsonKeys: list[str] = Field(default_factory=list)
    jsonExtractionSuccess: bool = False
    validationPassed: bool
    validationErrors: list[str] = Field(default_factory=list)
    validationErrorSummary: dict[str, Any] = Field(default_factory=dict)
    repairPromptPreview: str | None = None
    scenarioReflectionPassed: bool | None = None
    scenarioReflectionIssues: list[dict[str, Any]] = Field(default_factory=list)
    scenarioRepairPromptPreview: str | None = None
    scenarioPostProcessingApplied: bool | None = None
    scenarioPostProcessingPatches: list[dict[str, Any]] = Field(default_factory=list)
    providerErrorCode: str | None = None


class WorldConfigGenerationResult(BaseModel):
    model_config = ConfigDict(extra="forbid")

    requestId: str
    generationType: Literal["world_config"]
    targetContractType: Literal["world_config"]
    success: bool
    generatedPayload: dict[str, Any] | None = None
    validation: WorldConfigValidationSummary
    attempts: list[WorldConfigGenerationAttempt] = Field(default_factory=list)
    retrievedContexts: list[RetrievedPolicyContext] = Field(default_factory=list)
    scenarioReflection: ScenarioReflectionResult | None = None
    scenarioPostProcessing: ScenarioPostProcessResult | None = None
    environmentSampling: dict[str, Any] | None = None
    assumptions: list[str] = Field(default_factory=list)
    warnings: list[str] = Field(default_factory=list)
    error: WorldConfigGenerationError | None = None
    fallbackTrace: list[WorldConfigFallbackTrace] = Field(default_factory=list)
