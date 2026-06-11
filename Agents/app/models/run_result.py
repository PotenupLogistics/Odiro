from __future__ import annotations

from pydantic import BaseModel, ConfigDict, Field

from app.models.decision import ActionName


class RunMetrics(BaseModel):
    model_config = ConfigDict(extra="forbid")

    collisionCount: int = Field(ge=0)
    nearMissCount: int = Field(ge=0)
    minPedestrianDistanceCm: float = Field(ge=0)
    rerouteCount: int = Field(ge=0)
    stopCount: int = Field(ge=0)
    deliveryTimeSec: float = Field(ge=0)
    manualOverrideRequired: bool
    fallDetected: bool
    sidewalkDepartureCount: int = Field(ge=0)


class EventTimelineItem(BaseModel):
    model_config = ConfigDict(extra="forbid")

    t: float = Field(ge=0)
    event: str
    objectId: str | None = None
    distanceCm: float | None = Field(default=None, ge=0)
    action: ActionName | None = None
    note: str = ""


class RunResult(BaseModel):
    model_config = ConfigDict(extra="forbid")

    schemaVersion: str
    runId: str
    worldId: str
    policyId: str
    success: bool
    metrics: RunMetrics
    eventTimeline: list[EventTimelineItem]
