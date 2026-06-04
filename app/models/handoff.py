from __future__ import annotations

from typing import Any, Literal

from pydantic import BaseModel, ConfigDict, Field

from app.models.generation import (
    WorldConfigGenerationError,
    WorldConfigGenerationRequest,
)
from app.models.episode_spec import (
    EpisodeConversionWarning,
    EpisodeScenarioReflectionResult,
    EpisodeValidationResult,
)
from app.models.episode_setup import SetupValidationResult
from app.models.scenario import ScenarioPostProcessResult, ScenarioReflectionResult


class UE5HandoffWarning(BaseModel):
    model_config = ConfigDict(extra="forbid")

    code: str
    message: str


class UE5HandoffMetadata(BaseModel):
    model_config = ConfigDict(extra="forbid")

    generatedAt: str
    provider: str
    model: str | None = None
    policyId: str
    source: Literal["proto-ai"] = "proto-ai"
    contractType: Literal["world_config"] = "world_config"
    handoffTarget: Literal["ue5"] = "ue5"
    units: dict[str, str] = Field(
        default_factory=lambda: {
            "distance": "cm",
            "speed": "kmh",
            "time": "sec",
            "angle": "degree",
            "coordinate": "ue5_world_coordinate",
        }
    )


class UE5HandoffValidationSummary(BaseModel):
    model_config = ConfigDict(extra="forbid")

    schemaValidationPassed: bool
    scenarioReflectionPassed: bool
    contractValidationPassed: bool


class UE5WorldConfigHandoffRequest(BaseModel):
    model_config = ConfigDict(extra="forbid")

    schemaVersion: str
    requestId: str
    generationRequest: WorldConfigGenerationRequest
    handoffTarget: Literal["ue5"]
    includeDiagnostics: bool = True
    responseFormat: Literal["world_config", "episode_spec", "both", "setup_pair"] = "episode_spec"


class UE5WorldConfigHandoffResponse(BaseModel):
    model_config = ConfigDict(extra="forbid")

    schemaVersion: str
    requestId: str
    handoffId: str
    handoffTarget: Literal["ue5"]
    success: bool
    worldConfig: dict[str, Any] | None = None
    episodeSpec: dict[str, Any] | None = None
    episodeSetup: dict[str, Any] | None = None
    deliveryBotSetup: dict[str, Any] | None = None
    episodeValidation: EpisodeValidationResult | None = None
    episodeScenarioReflection: EpisodeScenarioReflectionResult | None = None
    episodeSetupValidation: SetupValidationResult | None = None
    deliveryBotSetupValidation: SetupValidationResult | None = None
    conversionWarnings: list[EpisodeConversionWarning] = Field(default_factory=list)
    metadata: UE5HandoffMetadata
    validation: UE5HandoffValidationSummary
    scenarioReflection: ScenarioReflectionResult | None = None
    postProcessing: ScenarioPostProcessResult | None = None
    diagnostics: dict[str, Any] | None = None
    warnings: list[UE5HandoffWarning] = Field(default_factory=list)
    error: WorldConfigGenerationError | None = None
