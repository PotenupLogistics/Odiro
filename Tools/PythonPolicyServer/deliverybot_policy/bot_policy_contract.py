from __future__ import annotations

from dataclasses import asdict, dataclass, field, is_dataclass
from typing import Any, Protocol


STATUS_OK = "ok"
STATUS_ERROR = "error"

CONTEXT_ROBOT_STATE = "ROBOT_STATE"
CONTEXT_LIDAR_SCAN = "LIDAR_SCAN"
CONTEXT_OBSERVED_OBJECTS = "OBSERVED_OBJECTS"
CONTEXT_CAMERA_FRAME = "CAMERA_FRAME"
CONTEXT_GRID_MAP = "GRID_MAP"
CONTEXT_PERCEPTION_RESULT = "PERCEPTION_RESULT"
CONTEXT_COMPUTE_ARTIFACT = "COMPUTE_ARTIFACT"

MERGE_MODE_MERGE = "Merge"
MERGE_MODE_REPLACE_ALL = "ReplaceAll"
MERGE_MODE_REPLACE_KIND = "ReplaceKind"

DIRECTION_FORWARD = "Forward"
DIRECTION_REVERSE = "Reverse"


@dataclass(slots=True)
class PolicyError:
    code: str
    message: str
    retryable: bool = False
    details: dict[str, Any] = field(default_factory=dict)


@dataclass(slots=True)
class Vector3:
    x: float
    y: float
    z: float


@dataclass(slots=True)
class Pose:
    x: float
    y: float
    z: float
    yawDegree: float = 0.0


@dataclass(slots=True)
class Goal:
    hasGoal: bool
    x: float
    y: float
    z: float
    acceptanceRadiusCm: float | None = None


@dataclass(slots=True)
class DriveConfig:
    maxSpeedKmh: float = 0.0
    maxReverseSpeedKmh: float = 0.0
    slowdownSpeedRangeKmh: float = 0.0
    stopBrakeInput: float = 1.0
    throttleInputRatePerSecond: float = 0.0
    brakeInputRatePerSecond: float = 0.0
    steeringInputRatePerSecond: float = 0.0
    accelerationRateKmhPerSecond: float = 0.0
    decelerationRateKmhPerSecond: float = 0.0
    maxTorque: float = 0.0
    maxRPM: float = 0.0


@dataclass(slots=True)
class LidarConfig:
    scanRangeM: float = 0.0
    angleStepDegree: float = 0.0
    sensorHeightM: float = 0.0
    frontHalfAngleDegree: float = 0.0
    storeMissedRays: bool = False
    stopDistanceM: float = 0.0
    slowDownDistanceM: float = 0.0
    lidarModeType: str = "Unknown"
    ignoreTags: list[str] = field(default_factory=list)


@dataclass(slots=True)
class MotionControlConfig:
    drawDebug: bool = False
    lookAheadDistanceM: float = 1.0
    pathPointAcceptanceDistanceM: float = 0.0
    goalAcceptanceDistanceM: float = 0.8
    steeringSensitivity: float = 0.8
    minTurnSpeedKmh: float = 1.0
    obstacleSlowSpeedKmh: float = 1.0


@dataclass(slots=True)
class ControlConfig:
    mode: str = "TargetSpeed"


@dataclass(slots=True)
class EnabledPolicy:
    policyId: str
    priority: int
    pathfinding: dict[str, Any] | None = None
    dynamicObstacles: dict[str, Any] | None = None
    recovery: dict[str, Any] | None = None
    parameters: dict[str, Any] = field(default_factory=dict)


@dataclass(slots=True)
class PolicySpec:
    catalogId: str = ""
    catalogVersion: int = 0
    enabledPolicies: list[EnabledPolicy] = field(default_factory=list)


@dataclass(slots=True)
class Config:
    schemaVersion: int = 1
    configId: str | None = None
    drive: DriveConfig | None = None
    lidar: LidarConfig | None = None
    motionControl: MotionControlConfig | None = None
    control: ControlConfig | None = None
    policy: PolicySpec | None = None
    parameters: dict[str, Any] = field(default_factory=dict)


@dataclass(slots=True)
class ContextRequirement:
    name: str
    kind: str
    required: bool
    maxAgeSeconds: float | None = None
    mode: str | None = None
    updateMode: str = "OnChange"


@dataclass(slots=True)
class ContextItemRef:
    name: str
    kind: str
    version: int
    capturedWorldTimeSeconds: float | None = None
    ageSeconds: float | None = None


@dataclass(slots=True)
class RejectedContextItem:
    name: str
    kind: str
    version: int
    reason: str
    error: PolicyError | None = None


@dataclass(slots=True)
class ComputeRequest:
    mode: str = "Skip"
    tasks: list[str] = field(default_factory=list)
    reason: str = ""
    deadlineMs: int | None = None


@dataclass(slots=True)
class ComputeResult:
    task: str
    status: str
    artifact: ContextItemRef | None = None
    elapsedMs: float = 0.0
    debug: dict[str, Any] = field(default_factory=dict)


@dataclass(slots=True)
class ComputeJob:
    jobId: str
    task: str
    status: str
    progress: float | None = None


@dataclass(slots=True)
class SetConfigResult:
    status: str
    accepted: bool
    configVersion: int
    requiredContext: list[ContextRequirement] = field(default_factory=list)
    computeResults: list[ComputeResult] = field(default_factory=list)
    pendingJobs: list[ComputeJob] = field(default_factory=list)
    warnings: list[str] = field(default_factory=list)
    error: PolicyError | None = None


@dataclass(slots=True)
class ContextItem:
    kind: str
    name: str
    version: int
    capturedWorldTimeSeconds: float | None = None
    maxAgeSeconds: float | None = None
    payload: dict[str, Any] | None = None
    dataRef: str | None = None
    metadata: dict[str, Any] = field(default_factory=dict)


@dataclass(slots=True)
class Context:
    updateId: str
    worldTimeSeconds: float
    mergeMode: str = MERGE_MODE_MERGE
    items: list[ContextItem] = field(default_factory=list)
    compute: ComputeRequest | None = None


@dataclass(slots=True)
class RobotState:
    x: float
    y: float
    z: float
    yawDegree: float
    speedKmh: float


@dataclass(slots=True)
class LidarRay:
    hit: bool
    rayIndex: int = 0
    rayYawDegree: float = 0.0
    distanceM: float = 0.0
    actorName: str = ""
    actorTags: list[str] = field(default_factory=list)


@dataclass(slots=True)
class LidarScan:
    mode: str = "Unknown"
    sensorSequence: int = 0
    rays: list[LidarRay] = field(default_factory=list)


@dataclass(slots=True)
class ObservedObject:
    actorName: str = ""
    actorTags: list[str] = field(default_factory=list)
    closestDistanceM: float = 0.0
    closestRayYawDegree: float = 0.0
    totalHitRayCount: int = 0
    frontHitRayCount: int = 0
    inFront: bool = False


@dataclass(slots=True)
class ObservedObjects:
    sensorSequence: int = 0
    objects: list[ObservedObject] = field(default_factory=list)


@dataclass(slots=True)
class CameraFrame:
    cameraName: str
    frameSequence: int
    capturedWorldTimeSeconds: float
    width: int
    height: int
    encoding: str
    dataRef: str
    hash: str | None = None
    pose: Pose | None = None


@dataclass(slots=True)
class GridCell:
    x: int
    y: int
    worldX: float
    worldY: float
    worldZ: float
    areaType: str = "Walkable"
    cost: float = 1.0
    blocked: bool = False
    sourceCollisionProfile: str = ""
    slopeDegree: float = 0.0


@dataclass(slots=True)
class GridMap:
    gridSizeX: int
    gridSizeY: int
    cellSizeCm: float
    cellCount: int
    originCm: Vector3
    cells: list[GridCell] = field(default_factory=list)


@dataclass(slots=True)
class SetContextResult:
    status: str
    accepted: bool
    contextVersion: int
    gridVersion: int
    acceptedItems: list[ContextItemRef] = field(default_factory=list)
    rejectedItems: list[RejectedContextItem] = field(default_factory=list)
    computeResults: list[ComputeResult] = field(default_factory=list)
    pendingJobs: list[ComputeJob] = field(default_factory=list)
    warnings: list[str] = field(default_factory=list)
    error: PolicyError | None = None


@dataclass(slots=True)
class EpisodeSetup:
    schemaVersion: int
    episodeId: str
    robotId: str
    startPose: Pose
    goal: Goal | None = None
    resetPolicyState: bool = True


@dataclass(slots=True)
class InitializeResult:
    status: str
    accepted: bool
    episodeVersion: int
    configVersion: int
    contextVersion: int
    gridVersion: int
    resetApplied: bool
    warnings: list[str] = field(default_factory=list)
    error: PolicyError | None = None


@dataclass(slots=True)
class DecisionRequest:
    sequence: int
    worldTimeSeconds: float
    inputRefs: dict[str, ContextItemRef] | None = None
    useLatestAcceptableContext: bool = True
    maxDecisionTimeMs: int | None = None


@dataclass(slots=True)
class Action:
    steering: float
    throttle: float
    brake: float
    targetSpeedKmh: float
    direction: str = DIRECTION_FORWARD


@dataclass(slots=True)
class DecisionDebug:
    policyName: str
    reason: str
    selectedPolicyId: str | None = None
    selectedPolicyPriority: int | None = None
    candidateCount: int | None = None
    usedInputs: dict[str, ContextItemRef] = field(default_factory=dict)
    values: dict[str, Any] = field(default_factory=dict)


@dataclass(slots=True)
class Decision:
    sequence: int
    status: str
    episodeVersion: int
    configVersion: int
    contextVersion: int
    gridVersion: int
    action: Action | None
    usedContext: dict[str, ContextItemRef] = field(default_factory=dict)
    debug: DecisionDebug = field(default_factory=lambda: DecisionDebug("unknown_policy", ""))
    error: PolicyError | None = None


class BotPolicy(Protocol):
    async def setConfig(self, config: Config) -> SetConfigResult:
        ...

    async def setContext(self, context: Context) -> SetContextResult:
        ...

    def initialize(self, setup: EpisodeSetup) -> InitializeResult:
        ...

    def decide(self, request: DecisionRequest) -> Decision:
        ...


def toPlainData(value: Any) -> Any:
    if is_dataclass(value):
        return {
            key: toPlainData(item)
            for key, item in asdict(value).items()
            if item is not None
        }
    if isinstance(value, dict):
        return {str(key): toPlainData(item) for key, item in value.items() if item is not None}
    if isinstance(value, list):
        return [toPlainData(item) for item in value]
    if isinstance(value, tuple):
        return [toPlainData(item) for item in value]

    return value

