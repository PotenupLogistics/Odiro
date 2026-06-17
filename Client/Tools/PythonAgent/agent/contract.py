from dataclasses import dataclass, field
from typing import Any


# Policy error payload returned to Unreal.
@dataclass
class PolicyError:
    code: str
    message: str
    retryable: bool = False
    details: dict[str, Any] = field(default_factory=dict)


# Unreal world location and yaw.
@dataclass
class StartLocation:
    x: float
    y: float
    z: float = 0.0
    yawDegree: float = 0.0


# Goal location for route planning.
@dataclass
class GoalLocation:
    hasGoal: bool
    x: float
    y: float
    z: float = 0.0


# Navigation grid cell state.
@dataclass
class GridCell:
    x: int
    y: int
    areaType: str = "Walkable"
    cost: float = 1.0
    blocked: bool = False
    sourceCollisionProfile: str = ""


# Navigation grid payload.
@dataclass
class GridMap:
    gridSizeX: int
    gridSizeY: int
    cellSizeCm: float
    cellCount: int
    originCm: StartLocation
    cells: list[GridCell]


# Robot state sent with every decide request.
@dataclass
class RobotState:
    x: float
    y: float
    z: float = 0.0
    yawDegree: float = 0.0
    speedKmh: float = 0.0
    bColliding: bool = False
    collisionActorName: str = ""
    collisionActorTags: list[str] = field(default_factory=list)


# Legacy 2D LiDAR ray used by current driving policies.
@dataclass
class LidarRay:
    hit: bool
    distanceM: float
    rayIndex: int | None = None
    rayYawDegree: float = 0.0
    actorName: str | None = None
    actorTags: list[str] = field(default_factory=list)


# 1D LiDAR ray input.
@dataclass
class LidarRay1D:
    hit: bool
    distanceM: float
    rayIndex: int | None = None
    actorName: str | None = None
    actorTags: list[str] = field(default_factory=list)


# 2D LiDAR ray input.
@dataclass
class LidarRay2D:
    hit: bool
    distanceM: float
    yawDegree: float = 0.0
    rayIndex: int | None = None
    actorName: str | None = None
    actorTags: list[str] = field(default_factory=list)


# 3D LiDAR ray input.
@dataclass
class LidarRay3D:
    hit: bool
    distanceM: float
    yawDegree: float = 0.0
    pitchDegree: float = 0.0
    rayIndex: int | None = None
    actorName: str | None = None
    actorTags: list[str] = field(default_factory=list)


# Typed LiDAR observation grouped by scan dimension.
@dataclass
class LidarObservation:
    mode: str = ""
    sensorSequence: int = 0
    sensorTimeSeconds: float = 0.0
    rays1d: list[LidarRay1D] = field(default_factory=list)
    rays2d: list[LidarRay2D] = field(default_factory=list)
    rays3d: list[LidarRay3D] = field(default_factory=list)


# Scenario start request.
@dataclass
class ScenarioStartRequest:
    robotInstanceId: str
    start: StartLocation
    goal: GoalLocation
    grid: GridMap
    robotSpec: dict[str, Any] = field(default_factory=dict)
    driveSpec: dict[str, Any] = field(default_factory=dict)
    lidarSpec: dict[str, Any] = field(default_factory=dict)
    artifactSpec: dict[str, Any] = field(default_factory=dict)  # Python capture artifact 저장 위치와 참조 기준
    vehicleSpec: dict[str, Any] = field(default_factory=dict)
    controlSpec: dict[str, Any] = field(default_factory=dict)


# Scenario decide request.
@dataclass
class ScenarioDecideRequest:
    sequence: int
    runTimeSeconds: float
    robotState: RobotState
    sensorSequence: int = 0
    sensorTimeSeconds: float = 0.0
    lidar: LidarObservation = field(default_factory=LidarObservation)
    lidarRays: list[LidarRay] = field(default_factory=list)
    observedObjects: list[dict[str, Any]] = field(default_factory=list)


# Scenario end request.
@dataclass
class ScenarioEndRequest:
    robotInstanceId: str
    sequence: int
    status: str
    error: PolicyError | None = None
    metrics: dict[str, Any] = field(default_factory=dict)
    debug: dict[str, Any] = field(default_factory=dict)
