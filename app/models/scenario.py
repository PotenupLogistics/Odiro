from __future__ import annotations

from typing import Any

from pydantic import BaseModel, ConfigDict, Field


class ScenarioIntent(BaseModel):
    model_config = ConfigDict(extra="forbid")

    mapHints: list[str] = Field(default_factory=list)
    obstacleHints: list[str] = Field(default_factory=list)
    obstaclePositionHint: dict[str, float] | None = None
    obstaclePlacementHint: str | None = None
    obstacleBlockingRatio: float | None = None
    pedestrianHints: list[str] = Field(default_factory=list)
    explicitNoPedestrian: bool = False
    crossingHints: list[str] = Field(default_factory=list)
    pathBlockingHints: bool = False
    sidewalkWidthCm: float | None = None
    terrainHints: list[str] = Field(default_factory=list)
    trafficSignalHints: list[str] = Field(default_factory=list)
    suggestedCategories: list[str] = Field(default_factory=list)
    suggestedActions: list[str] = Field(default_factory=list)
    suggestedPolicyParams: list[str] = Field(default_factory=list)


class ScenarioRequirement(BaseModel):
    model_config = ConfigDict(extra="forbid")

    requirementId: str
    type: str
    description: str
    requiredInWorldConfig: bool
    expectedPath: str
    expectedValueHint: str


class ScenarioReflectionIssue(BaseModel):
    model_config = ConfigDict(extra="forbid")

    requirementId: str
    issueType: str = "weak_scenario_binding"
    severity: str
    message: str
    expectedPath: str
    expectedValueHint: str = ""
    actualValueSummary: str = ""
    repairInstruction: str = ""


class ScenarioReflectionResult(BaseModel):
    model_config = ConfigDict(extra="forbid")

    passed: bool
    checkedRequirements: list[ScenarioRequirement] = Field(default_factory=list)
    missingRequirements: list[ScenarioRequirement] = Field(default_factory=list)
    partiallySatisfiedRequirements: list[ScenarioRequirement] = Field(default_factory=list)
    issues: list[ScenarioReflectionIssue] = Field(default_factory=list)
    summary: str


class ScenarioPostProcessPatch(BaseModel):
    model_config = ConfigDict(extra="forbid")

    patchId: str
    patchType: str
    targetPath: str
    beforeValue: Any = None
    afterValue: Any = None
    reason: str


class ScenarioPostProcessResult(BaseModel):
    model_config = ConfigDict(extra="forbid")

    applied: bool
    patches: list[ScenarioPostProcessPatch] = Field(default_factory=list)
    patchedPayload: dict[str, Any]
    warnings: list[str] = Field(default_factory=list)
