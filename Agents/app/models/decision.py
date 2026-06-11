from __future__ import annotations

from enum import Enum

from pydantic import BaseModel, ConfigDict, Field


class ActionName(str, Enum):
    Continue = "Continue"
    SlowDown = "SlowDown"
    Stop = "Stop"
    EmergencyStop = "EmergencyStop"
    LocalAvoidance = "LocalAvoidance"
    ReplanPath = "ReplanPath"
    YieldWait = "YieldWait"
    RequestOperator = "RequestOperator"


class EventType(str, Enum):
    PedestrianAhead = "PedestrianAhead"
    ObstacleAhead = "ObstacleAhead"
    FallOrTilt = "FallOrTilt"
    TerrainRisk = "TerrainRisk"
    ApproachingObject = "ApproachingObject"
    CrosswalkApproach = "CrosswalkApproach"
    CommunicationIssue = "CommunicationIssue"
    CrowdedPath = "CrowdedPath"
    Unknown = "Unknown"


class ObjectType(str, Enum):
    Pedestrian = "Pedestrian"
    Obstacle = "Obstacle"
    Animal = "Animal"
    Bicycle = "Bicycle"
    Kickboard = "Kickboard"
    Vehicle = "Vehicle"
    Unknown = "Unknown"


class Location(BaseModel):
    model_config = ConfigDict(extra="forbid")

    x: float
    y: float
    z: float


class Event(BaseModel):
    model_config = ConfigDict(extra="forbid")

    eventId: str
    type: EventType
    severity: str
    confidence: float = Field(ge=0, le=1)
    source: str
    description: str = ""


class BotState(BaseModel):
    model_config = ConfigDict(extra="forbid")

    location: Location
    yawDegree: float
    pitchDegree: float
    rollDegree: float
    speedKmh: float = Field(ge=0)


class DetectedObject(BaseModel):
    model_config = ConfigDict(extra="forbid")

    objectId: str
    type: ObjectType
    location: Location
    relativeLocation: Location
    distanceCm: float = Field(ge=0)
    relativeSpeedKmh: float
    relativeDirection: str
    isOnPath: bool
    pathOverlapDistanceCm: float = Field(ge=0)
    timeToCollisionSec: float | None = Field(default=None, ge=0)


class PathContext(BaseModel):
    model_config = ConfigDict(extra="forbid")

    pathBlocked: bool
    leftClearanceCm: float = Field(ge=0)
    rightClearanceCm: float = Field(ge=0)
    leftTraversable: bool
    rightTraversable: bool
    pathOverlapRatio: float = Field(ge=0, le=1)


class Terrain(BaseModel):
    model_config = ConfigDict(extra="forbid")

    traversabilityScore: float = Field(ge=0, le=1)
    slopeDegree: float
    curbHeightCm: float = Field(ge=0)
    surfaceType: str


class EnvironmentState(BaseModel):
    model_config = ConfigDict(extra="forbid")

    type: str
    state: str


class RemainTarget(BaseModel):
    model_config = ConfigDict(extra="forbid")

    location: Location
    distanceCm: float = Field(ge=0)


class DecisionRequest(BaseModel):
    model_config = ConfigDict(extra="forbid")

    schemaVersion: str
    requestId: str
    simulationId: str
    timestampSec: float = Field(ge=0)
    policyId: str
    event: Event
    botState: BotState
    remainTarget: RemainTarget
    detectedObjects: list[DetectedObject]
    environments: list[EnvironmentState]
    pathContext: PathContext | None = None
    terrain: Terrain | None = None
    communicationStatus: str | None = None


class DecisionCommand(BaseModel):
    model_config = ConfigDict(extra="forbid")

    targetSpeedKmh: float | None = Field(default=None, ge=0)
    durationSec: float | None = Field(default=None, ge=0)
    allowRecheck: bool = False
    recheckAfterSec: float | None = Field(default=None, ge=0)
    replanRequired: bool = False
    avoidanceDirection: str | None = None
    requestOperatorReason: str | None = None


class SafetyConstraints(BaseModel):
    model_config = ConfigDict(extra="forbid")

    mustStopIfDistanceUnderCm: float | None = Field(default=None, ge=0)
    maxAllowedSpeedKmh: float | None = Field(default=None, ge=0)
    requiresHumanApproval: bool = False


class AppliedRule(BaseModel):
    model_config = ConfigDict(extra="forbid")

    ruleId: str
    ruleName: str
    matchedCondition: str


class DecisionResponse(BaseModel):
    model_config = ConfigDict(extra="forbid")

    schemaVersion: str
    requestId: str
    decisionId: str
    selectedAction: ActionName
    priority: int = Field(ge=0)
    confidence: float = Field(ge=0, le=1)
    command: DecisionCommand
    reason: str
    recordTags: list[str]
    safetyConstraints: SafetyConstraints | None = None
    appliedRules: list[AppliedRule] = Field(default_factory=list)
