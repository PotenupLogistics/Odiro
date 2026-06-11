from __future__ import annotations

from typing import Any, Literal

from pydantic import BaseModel, ConfigDict, Field


ScenarioType = Literal[
    "narrow_sidewalk_kickboard_crossing",
    "obstacle_ahead",
    "pedestrian_crossing",
    "terrain_risk",
    "generic_sidewalk",
]


class EnvironmentParameterCatalog(BaseModel):
    model_config = ConfigDict(extra="forbid")

    schemaVersion: str = "1.0.0"
    allowedValues: dict[str, list[int | float]]


class EnvironmentSamplingRequest(BaseModel):
    model_config = ConfigDict(extra="forbid")

    schemaVersion: str = "1.0.0"
    requestId: str
    seed: int
    scenarioType: ScenarioType = "generic_sidewalk"
    fixedParameters: dict[str, Any] = Field(default_factory=dict)
    includeLabels: bool = True


class EnvironmentParameterSet(BaseModel):
    model_config = ConfigDict(extra="forbid")

    sidewalkWidthCm: int
    pedestrianCount: int
    pedestrianSpeedMps: float
    obstacleBlockingRatio: float
    obstacleLateralOffsetM: float
    crossingAngleDeg: int
    robotSpeedKmh: int
    slopeDegree: int
    curbHeightCm: int
    timeLimitSec: int


class EnvironmentSamplingWarning(BaseModel):
    model_config = ConfigDict(extra="forbid")

    code: str
    message: str
    parameterName: str | None = None


class EnvironmentSamplingResult(BaseModel):
    model_config = ConfigDict(extra="forbid")

    requestId: str
    seed: int
    scenarioType: ScenarioType
    parameters: EnvironmentParameterSet
    labelHints: dict[str, str] = Field(default_factory=dict)
    warnings: list[EnvironmentSamplingWarning] = Field(default_factory=list)


class EnvironmentSamplingContext(BaseModel):
    model_config = ConfigDict(extra="forbid")

    enabled: bool = False
    seed: int | None = None
    scenarioType: ScenarioType | None = None
    parameters: EnvironmentParameterSet | None = None
    fixedParameters: dict[str, Any] = Field(default_factory=dict)
    warnings: list[EnvironmentSamplingWarning] = Field(default_factory=list)
