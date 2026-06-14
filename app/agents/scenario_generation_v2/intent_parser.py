from __future__ import annotations

from dataclasses import dataclass, field


@dataclass(frozen=True)
class ScenarioIntent:
    environment_type: str
    scenario_goal: str
    risk_factors: list[str] = field(default_factory=list)
    main_actor: str = "delivery_robot"
    difficulty: str = "medium"


class IntentParser:
    def parse(self, prompt: str) -> ScenarioIntent:
        lowered = prompt.lower()
        risk_factors: list[str] = []
        environment_type = "sidewalk"
        difficulty = "medium"

        if any(token in prompt for token in ("좁", "협소")) or "narrow" in lowered:
            risk_factors.append("narrow_sidewalk")
            difficulty = "medium_high"
        if any(token in prompt for token in ("장애물", "obstacle")):
            risk_factors.append("static_obstacle_ahead")
        if any(token in prompt for token in ("보행자", "pedestrian", "횡단", "cross")):
            risk_factors.append("pedestrian_crossing")
        if any(token in prompt for token in ("횡단보도", "crosswalk")):
            environment_type = "crosswalk"
        elif any(token in prompt for token in ("교차", "junction", "t자", "t-junction")):
            environment_type = "t_junction"

        if not risk_factors:
            risk_factors.append("baseline_navigation")

        return ScenarioIntent(
            environment_type=environment_type,
            scenario_goal="risk_evaluation",
            risk_factors=risk_factors,
            difficulty=difficulty,
        )

