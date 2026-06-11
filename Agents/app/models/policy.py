from __future__ import annotations

from pydantic import BaseModel, ConfigDict, Field

from app.models.decision import ActionName


class PolicyParameters(BaseModel):
    model_config = ConfigDict(extra="forbid")

    maxSpeedKmh: float | None = Field(default=None, ge=0)
    lowSpeedZoneSpeedKmh: float | None = Field(default=None, ge=0)
    emergencyStopDistanceCm: float | None = Field(default=None, ge=0)
    ttcThresholdSec: float | None = Field(default=None, ge=0)
    safeDistanceCm: float | None = Field(default=None, ge=0)
    perceptionMinRangeM: float | None = Field(default=None, ge=0)
    traversabilityThreshold: float | None = Field(default=None, ge=0, le=1)
    rollPitchThresholdDeg: float | None = Field(default=None, ge=0)
    botMassKg: float | None = Field(default=None, ge=0)
    robotWidthCm: float | None = Field(default=None, ge=0)
    sidewalkWidthCm: float | None = Field(default=None, ge=0)
    waitTimeoutSec: float | None = Field(default=None, ge=0)
    maxRerouteAttempts: int | None = Field(default=None, ge=0)
    operatorOverrideEnabled: bool | None = None


class PolicyConfig(BaseModel):
    model_config = ConfigDict(extra="forbid")

    schemaVersion: str
    policyId: str
    policyName: str
    priority: int = Field(ge=0)
    parameters: PolicyParameters
    availableActions: list[ActionName]
