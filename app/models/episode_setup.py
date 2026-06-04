from __future__ import annotations

from typing import Any, Literal

from pydantic import BaseModel, ConfigDict, Field


EpisodePropertyValue = str | int | float | bool | list[float] | None


class EpisodeRunConfig(BaseModel):
    model_config = ConfigDict(extra="forbid")

    base_seed: int = 0
    iteration_index: int = 0
    time_limit_s: float = Field(default=60.0, ge=0)


class EpisodeEvaluationConfig(BaseModel):
    model_config = ConfigDict(extra="forbid")

    goal_acceptance_radius_m: float = Field(default=1.0, ge=0)
    tip_over_angle_deg: float = Field(default=60.0, ge=0)
    near_miss: dict[str, Any] = Field(default_factory=lambda: {"distance_m": 0.5})
    scoring: dict[str, float] = Field(
        default_factory=lambda: {
            "static_obstacle_collision": -1,
            "blocked_region_collision": -1,
            "penalty_region_violation": -3,
            "pedestrian_near_miss": -3,
            "pedestrian_collision": -10,
        }
    )


class RegionShape(BaseModel):
    model_config = ConfigDict(extra="forbid")

    type: Literal["rectangle"] = "rectangle"
    center_xy_m: list[float] = Field(min_length=2, max_length=2)
    size_m: list[float] = Field(min_length=2, max_length=2)
    yaw_deg: float = 0.0


class GroundRegion(BaseModel):
    model_config = ConfigDict(extra="forbid")

    region_id: str
    region_type: str = "walkable"
    shape: RegionShape
    traversability_score: float = Field(default=1.0, ge=0, le=1)
    penalty: dict[str, Any] | None = None
    collision_tag: str | None = None


class GroundModel(BaseModel):
    model_config = ConfigDict(extra="forbid")

    default_region_type: str = "walkable"
    regions: list[GroundRegion]


class EpisodePath(BaseModel):
    model_config = ConfigDict(extra="forbid")

    path_id: str
    points_xy_m: list[list[float]]
    closed_loop: bool = False


class PedestrianMovement(BaseModel):
    model_config = ConfigDict(extra="forbid")

    model: str = "straight_line"
    speed_mps: float = Field(default=1.0, ge=0)
    initial_distance_m: float = Field(default=0.0, ge=0)
    auto_start: bool = True


class EpisodeStaticObstacle(BaseModel):
    model_config = ConfigDict(extra="forbid")

    instance_id: str
    prop_id: str
    xy_m: list[float] = Field(default_factory=lambda: [0.0, 0.0], min_length=2, max_length=2)
    yaw_deg: float = 0.0
    properties: dict[str, EpisodePropertyValue] = Field(default_factory=dict)


class EpisodePedestrian(BaseModel):
    model_config = ConfigDict(extra="forbid")

    instance_id: str
    archetype_id: str = "adult_pedestrian"
    path_id: str
    spawn_time_s: float = 0.0
    xy_m: list[float] = Field(default_factory=lambda: [0.0, 0.0], min_length=2, max_length=2)
    yaw_deg: float = 0.0
    movement: PedestrianMovement = Field(default_factory=PedestrianMovement)
    properties: dict[str, EpisodePropertyValue] = Field(default_factory=dict)


class EpisodeRobotRoute(BaseModel):
    model_config = ConfigDict(extra="forbid")

    goal_xy_m: list[float] = Field(min_length=2, max_length=2)
    auto_start: bool = True


class EpisodeRobot(BaseModel):
    model_config = ConfigDict(extra="forbid")

    instance_id: str = "robot_01"
    asset_id: str = "delivery_bot"
    spawn_only: bool = False
    xy_m: list[float] = Field(default_factory=lambda: [0.0, 0.0], min_length=2, max_length=2)
    yaw_deg: float = 0.0
    route: EpisodeRobotRoute | None = None
    properties: dict[str, EpisodePropertyValue] = Field(default_factory=dict)


class EpisodeActors(BaseModel):
    model_config = ConfigDict(extra="forbid")

    robot: EpisodeRobot
    static_obstacles: list[EpisodeStaticObstacle] = Field(default_factory=list)
    pedestrians: list[EpisodePedestrian] = Field(default_factory=list)


class EpisodeSetup(BaseModel):
    model_config = ConfigDict(extra="forbid", populate_by_name=True)

    schema_: Literal["episode_actor_spawn_mvp"] = Field(default="episode_actor_spawn_mvp", alias="schema")
    version: int = 1
    scenario_id: str
    map_id: str = "EpisodeSandbox"
    run: EpisodeRunConfig = Field(default_factory=EpisodeRunConfig)
    evaluation: EpisodeEvaluationConfig = Field(default_factory=EpisodeEvaluationConfig)
    ground_model: GroundModel
    paths: list[EpisodePath] = Field(default_factory=list)
    actors: EpisodeActors

    @property
    def schema(self) -> str:
        return self.schema_


class SetupValidationError(BaseModel):
    model_config = ConfigDict(extra="forbid")

    code: str
    message: str
    path: str | None = None


class SetupValidationWarning(BaseModel):
    model_config = ConfigDict(extra="forbid")

    code: str
    message: str
    path: str | None = None


class SetupValidationResult(BaseModel):
    model_config = ConfigDict(extra="forbid")

    valid: bool
    errors: list[SetupValidationError] = Field(default_factory=list)
    warnings: list[SetupValidationWarning] = Field(default_factory=list)
