from __future__ import annotations

from pydantic import BaseModel, Field, model_validator


class BotDriveSetup(BaseModel):
    max_speed_kmh: float = Field(default=10.0, ge=0)
    slowdown_speed_range_kmh: float = Field(default=4.0, ge=0.1)
    speed_limit_tolerance_kmh: float = Field(default=0.5, ge=0)
    speed_limit_brake: float = Field(default=0.08, ge=0, le=1)
    stop_brake_input: float = Field(default=0.2, ge=0, le=1)
    throttle_input_rate_per_second: float = Field(default=0.35, ge=0)
    brake_input_rate_per_second: float = Field(default=0.5, ge=0)
    steering_input_rate_per_second: float = Field(default=3.0, ge=0)
    acceleration_rate_kmh_per_second: float = Field(default=2.0, ge=0)
    deceleration_rate_kmh_per_second: float = Field(default=3.0, ge=0)
    use_handbrake_when_brake: bool = False
    max_torque: float = Field(default=220.0, ge=0)
    max_rpm: float = Field(default=4000.0, ge=0)
    engine_idle_rpm: float = Field(default=600.0, ge=0)
    engine_brake_effect: float = Field(default=0.04, ge=0)
    engine_rev_up_moi: float = Field(default=5.0, ge=0)
    engine_rev_down_rate: float = Field(default=600.0, ge=0)


class BotPathFollowSetup(BaseModel):
    target_speed_kmh: float = Field(default=10.0, ge=0)
    look_ahead_distance_m: float = Field(default=1.0, ge=0.1)
    obstacle_slow_speed_kmh: float = Field(default=1.5, ge=0)
    draw_debug: bool = True
    path_point_acceptance_distance_m: float = Field(default=0.4, ge=0)
    goal_acceptance_distance_m: float = Field(default=0.8, ge=0)
    steering_sensitivity: float = Field(default=0.8, ge=0)
    min_turn_speed_kmh: float = Field(default=1.5, ge=0)


class BotLidarSetup(BaseModel):
    scan_range_m: float = Field(default=5.0, ge=0)
    angle_step_degree: float = Field(default=2.0, ge=1.0)
    stop_distance_m: float = Field(default=1.2, ge=0)
    slow_down_distance_m: float = Field(default=3.5, ge=0)
    draw_debug: bool = True
    sensor_height_m: float = Field(default=0.07, ge=0)
    front_half_angle_degree: float = Field(default=20.0, ge=0, le=180)
    store_missed_rays: bool = False
    trace_channel: str = "visibility"
    ignore_tags: list[str] = Field(default_factory=lambda: ["NoCollision"])

    @model_validator(mode="after")
    def slow_down_must_exceed_stop(self) -> "BotLidarSetup":
        if self.slow_down_distance_m < self.stop_distance_m + 0.1:
            self.slow_down_distance_m = round(self.stop_distance_m + 0.1, 4)
        return self


class DeliveryBotSetup(BaseModel):
    schema_: str = Field(alias="schema", default="delivery_bot_setup")
    version: int = 1
    drive: BotDriveSetup = Field(default_factory=BotDriveSetup)
    path_follow: BotPathFollowSetup = Field(default_factory=BotPathFollowSetup)
    lidar: BotLidarSetup = Field(default_factory=BotLidarSetup)

    model_config = {"populate_by_name": True}

    def to_json_dict(self) -> dict:
        return {
            "schema": "delivery_bot_setup",
            "version": self.version,
            "robot": {
                "drive": self.drive.model_dump(exclude_none=True),
                "path_follow": self.path_follow.model_dump(exclude_none=True),
                "lidar": self.lidar.model_dump(exclude_none=True),
            },
        }
