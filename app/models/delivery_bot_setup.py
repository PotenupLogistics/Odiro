from __future__ import annotations

from typing import Literal

from pydantic import BaseModel, ConfigDict, Field

from app.models.episode_setup import SetupValidationError, SetupValidationResult, SetupValidationWarning


class DeliveryBotDriveConfig(BaseModel):
    model_config = ConfigDict(extra="forbid")

    max_speed_kmh: float = Field(default=10.0, ge=0)
    slowdown_speed_range_kmh: float = Field(default=2.0, ge=0.1)
    speed_limit_tolerance_kmh: float | None = None
    speed_limit_brake: float | None = Field(default=None, ge=0, le=1)
    stop_brake_input: float | None = Field(default=None, ge=0, le=1)
    throttle_input_rate_per_second: float | None = None
    brake_input_rate_per_second: float | None = None
    steering_input_rate_per_second: float | None = None
    acceleration_rate_kmh_per_second: float | None = None
    deceleration_rate_kmh_per_second: float | None = None
    use_handbrake_when_brake: bool | None = None
    max_torque: float | None = None
    max_rpm: float | None = None
    engine_idle_rpm: float | None = None
    engine_brake_effect: float | None = None
    engine_rev_up_moi: float | None = None
    engine_rev_down_rate: float | None = None


class DeliveryBotPathFollowConfig(BaseModel):
    model_config = ConfigDict(extra="forbid")

    target_speed_kmh: float = Field(default=10.0, ge=0)
    look_ahead_distance_m: float = Field(default=1.0, ge=0.1)
    obstacle_slow_speed_kmh: float = Field(default=2.0, ge=0)
    draw_debug: bool | None = None
    path_point_acceptance_distance_m: float | None = None
    goal_acceptance_distance_m: float | None = None
    steering_sensitivity: float | None = None
    min_turn_speed_kmh: float | None = None


class DeliveryBotLidarConfig(BaseModel):
    model_config = ConfigDict(extra="forbid")

    scan_range_m: float = Field(default=5.0, ge=0)
    angle_step_degree: float = Field(default=5.0, ge=1.0)
    stop_distance_m: float = Field(default=1.2, ge=0)
    slow_down_distance_m: float = Field(default=3.5)
    draw_debug: bool | None = None
    sensor_height_m: float | None = None
    front_half_angle_degree: float | None = Field(default=None, ge=0, le=180)
    store_missed_rays: bool | None = None
    trace_channel: str | int | None = None
    ignore_tags: list[str] | None = None


class DeliveryBotSetupRobot(BaseModel):
    model_config = ConfigDict(extra="forbid")

    drive: DeliveryBotDriveConfig = Field(default_factory=DeliveryBotDriveConfig)
    path_follow: DeliveryBotPathFollowConfig = Field(default_factory=DeliveryBotPathFollowConfig)
    lidar: DeliveryBotLidarConfig = Field(default_factory=DeliveryBotLidarConfig)


class DeliveryBotSetup(BaseModel):
    model_config = ConfigDict(extra="forbid", populate_by_name=True)

    schema_: Literal["delivery_bot_setup"] = Field(default="delivery_bot_setup", alias="schema")
    version: int = 1
    robot: DeliveryBotSetupRobot = Field(default_factory=DeliveryBotSetupRobot)

    @property
    def schema(self) -> str:
        return self.schema_


DeliveryBotValidationError = SetupValidationError
DeliveryBotValidationWarning = SetupValidationWarning
DeliveryBotValidationResult = SetupValidationResult
