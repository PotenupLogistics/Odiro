from __future__ import annotations

from typing import Literal

from pydantic import BaseModel, ConfigDict, Field


EpisodePropertyValue = str | int | float | bool | list[float] | None


class EpisodeUnits(BaseModel):
    model_config = ConfigDict(extra="forbid")

    distance: Literal["m"] = "m"
    angle: Literal["deg"] = "deg"


class EpisodeRun(BaseModel):
    model_config = ConfigDict(extra="forbid")

    base_seed: int
    iteration_index: int = 0
    time_limit_s: float = Field(ge=0)


class GroundShape(BaseModel):
    model_config = ConfigDict(extra="forbid")

    type: Literal["rectangle"]
    center_m: list[float] = Field(min_length=3, max_length=3)
    size_m: list[float] = Field(min_length=2, max_length=2)
    yaw_deg: float = 0.0


class GroundPenalty(BaseModel):
    model_config = ConfigDict(extra="forbid")

    kind: str = ""
    cost: float = 0.0
    violation_after_s: float = 0.0


class GroundRegion(BaseModel):
    model_config = ConfigDict(extra="forbid")

    region_id: str
    region_type: str
    shape: GroundShape
    traversability_score: float = Field(ge=0, le=1)
    penalty: GroundPenalty | None = None
    collision_tag: str | None = None


class GroundModel(BaseModel):
    model_config = ConfigDict(extra="forbid")

    default_region_type: str = "walkable"
    regions: list[GroundRegion]


class EpisodeRotation(BaseModel):
    model_config = ConfigDict(extra="forbid")

    yaw: float = 0.0
    pitch: float = 0.0
    roll: float = 0.0


class EpisodeTransform(BaseModel):
    model_config = ConfigDict(extra="forbid")

    location_m: list[float] = Field(min_length=3, max_length=3)
    rotation_deg: EpisodeRotation = Field(default_factory=EpisodeRotation)
    scale: list[float] = Field(default_factory=lambda: [1.0, 1.0, 1.0], min_length=3, max_length=3)


class RobotRoute(BaseModel):
    model_config = ConfigDict(extra="forbid")

    goal_m: list[float] = Field(min_length=3, max_length=3)
    auto_start: bool = True


class RobotActor(BaseModel):
    model_config = ConfigDict(extra="forbid")

    instance_id: str
    asset_id: str = "delivery_bot"
    spawn_only: bool = False
    transform: EpisodeTransform
    route: RobotRoute | None = None
    properties: dict[str, EpisodePropertyValue] = Field(default_factory=dict)


class PedestrianMovement(BaseModel):
    model_config = ConfigDict(extra="forbid")

    model: str = "straight_line"
    speed_mps: float = Field(ge=0)
    initial_distance_m: float = 0.0
    auto_start: bool = True


class PedestrianActor(BaseModel):
    model_config = ConfigDict(extra="forbid")

    instance_id: str
    archetype_id: str = "adult_pedestrian"
    path_id: str
    spawn_time_s: float = 0.0
    transform: EpisodeTransform
    movement: PedestrianMovement
    properties: dict[str, EpisodePropertyValue] = Field(default_factory=dict)


class StaticObstacleActor(BaseModel):
    model_config = ConfigDict(extra="forbid")

    instance_id: str
    prop_id: str
    transform: EpisodeTransform
    properties: dict[str, EpisodePropertyValue] = Field(default_factory=dict)


class EpisodePath(BaseModel):
    model_config = ConfigDict(extra="forbid")

    path_id: str
    role: str
    type: Literal["spline"] = "spline"
    points_m: list[list[float]]
    closed_loop: bool = False


class EpisodeActors(BaseModel):
    model_config = ConfigDict(extra="forbid")

    robot: RobotActor
    pedestrians: list[PedestrianActor] = Field(default_factory=list)
    static_obstacles: list[StaticObstacleActor] = Field(default_factory=list)


class EpisodeSpec(BaseModel):
    model_config = ConfigDict(extra="forbid", populate_by_name=True)

    schema_: Literal["episode_actor_spawn_mvp"] = Field(
        default="episode_actor_spawn_mvp",
        alias="schema",
    )
    version: int = 1
    scenario_id: str
    map_id: str = "EpisodeSandbox"
    units: EpisodeUnits = Field(default_factory=EpisodeUnits)
    run: EpisodeRun
    ground_model: GroundModel
    paths: list[EpisodePath] = Field(default_factory=list)
    actors: EpisodeActors

    @property
    def schema(self) -> str:
        return self.schema_


class EpisodeValidationError(BaseModel):
    model_config = ConfigDict(extra="forbid")

    code: str
    message: str
    path: str | None = None


class EpisodeValidationWarning(BaseModel):
    model_config = ConfigDict(extra="forbid")

    code: str
    message: str
    path: str | None = None


class EpisodeValidationResult(BaseModel):
    model_config = ConfigDict(extra="forbid")

    valid: bool
    errors: list[EpisodeValidationError] = Field(default_factory=list)
    warnings: list[EpisodeValidationWarning] = Field(default_factory=list)


class EpisodeConversionWarning(BaseModel):
    model_config = ConfigDict(extra="forbid")

    code: str
    message: str
    sourcePath: str | None = None


class EpisodeScenarioReflectionIssue(BaseModel):
    model_config = ConfigDict(extra="forbid")

    issueType: str
    expectedPath: str
    expectedValueHint: str
    actualValueSummary: str
    message: str


class EpisodeScenarioReflectionResult(BaseModel):
    model_config = ConfigDict(extra="forbid")

    passed: bool
    issues: list[EpisodeScenarioReflectionIssue] = Field(default_factory=list)
    staticObstacleCount: int = 0
    hasKickboardSemantic: bool = False
    hasBlockingRatio: bool = False
    pedestrianCount: int = 0
    pathCount: int = 0
    pedestrianPathLinked: bool = False
    hasCrossingPedestrian: bool = False
    sidewalkWidthM: float | None = None
    ueCompilerReadiness: bool = False
