from __future__ import annotations

from pydantic import BaseModel, ConfigDict, Field

from app.models.decision import Location


class MapConfig(BaseModel):
    model_config = ConfigDict(extra="forbid")

    type: str
    lengthCm: float = Field(ge=0)
    sidewalkWidthCm: float = Field(ge=0)
    surfaceCondition: str
    slopeDegree: float


class RobotConfig(BaseModel):
    model_config = ConfigDict(extra="forbid")

    botId: str
    spawn: Location
    goal: Location
    policyId: str


class ObstacleConfig(BaseModel):
    model_config = ConfigDict(extra="forbid")

    objectId: str
    type: str
    position: Location
    blockingRatio: float = Field(ge=0, le=1)


class PedestrianConfig(BaseModel):
    model_config = ConfigDict(extra="forbid")

    objectId: str
    spawn: Location
    goal: Location
    speedKmh: float = Field(ge=0)
    behavior: str


class EnvironmentObjectConfig(BaseModel):
    model_config = ConfigDict(extra="forbid")

    objectId: str
    type: str
    position: Location


class RuntimeConfig(BaseModel):
    model_config = ConfigDict(extra="forbid")

    maxDurationSec: float = Field(ge=0)
    captureReplay: bool
    emitEventLog: bool


class WorldConfig(BaseModel):
    model_config = ConfigDict(extra="forbid")

    schemaVersion: str
    worldId: str
    scenarioId: str
    seed: int
    map: MapConfig
    robot: RobotConfig
    runtime: RuntimeConfig
    obstacles: list[ObstacleConfig] = Field(default_factory=list)
    pedestrians: list[PedestrianConfig] = Field(default_factory=list)
    environmentObjects: list[EnvironmentObjectConfig] = Field(default_factory=list)
