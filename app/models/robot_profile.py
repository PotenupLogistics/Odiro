from __future__ import annotations

from typing import Literal

from pydantic import BaseModel, ConfigDict, Field, model_validator


class RobotProfile(BaseModel):
    model_config = ConfigDict(extra="forbid")

    profile_id: str = "delivery_bot_alpha"
    width_m: float = Field(default=0.44, gt=0)
    depth_m: float = Field(default=1.0, gt=0)
    height_m: float = Field(default=0.64, gt=0)
    footprint_shape: Literal["box"] = "box"
    safety_margin_m: float = Field(default=0.2, ge=0)
    min_passable_width_m: float = Field(default=0.84, gt=0)

    @model_validator(mode="after")
    def validate_min_passable_width(self) -> "RobotProfile":
        expected = round(self.width_m + self.safety_margin_m * 2.0, 6)
        if abs(self.min_passable_width_m - expected) > 0.001:
            raise ValueError("min_passable_width_m must equal width_m + safety_margin_m * 2.")
        return self

    @property
    def half_width_with_margin_m(self) -> float:
        return self.width_m / 2.0 + self.safety_margin_m

    @property
    def half_depth_with_margin_m(self) -> float:
        return self.depth_m / 2.0 + self.safety_margin_m


DEFAULT_ROBOT_PROFILE = RobotProfile()


def default_robot_profile() -> RobotProfile:
    return DEFAULT_ROBOT_PROFILE.model_copy(deep=True)


def robot_physical_constraints_prompt_section(profile: RobotProfile | None = None) -> str:
    robot_profile = profile or DEFAULT_ROBOT_PROFILE
    return "\n".join(
        [
            "Robot physical size:",
            f"- width: {robot_profile.width_m:.2f} m",
            f"- depth/length: {robot_profile.depth_m:.2f} m",
            f"- height: {robot_profile.height_m:.2f} m",
            f"- footprint shape: {robot_profile.footprint_shape}",
            f"- safety margin: {robot_profile.safety_margin_m:.2f} m on each side",
            f"- minimum passable width: {robot_profile.min_passable_width_m:.2f} m",
            "",
            "Generation constraints:",
            "- Do not create passable corridors narrower than robot width plus safety margin.",
            "- Treat narrow gaps as blocked if the robot footprint cannot pass safely.",
            "- Keep robot spawn and goal positions away from blocked regions by at least half of the robot footprint plus safety margin.",
            "- Use the robot profile as a fixed server-side constraint, not as a user-controlled prompt value.",
        ]
    )
