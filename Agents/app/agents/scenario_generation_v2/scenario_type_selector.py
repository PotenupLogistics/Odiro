from __future__ import annotations

from app.agents.scenario_generation_v2.intent_parser import ScenarioIntent


SUPPORTED_SCENARIO_TYPES = {
    "straight_sidewalk",
    "narrow_sidewalk",
    "crosswalk",
    "t_junction",
    "obstacle_corridor",
}


class ScenarioTypeSelector:
    def select(self, intent: ScenarioIntent) -> str:
        if intent.environment_type == "crosswalk":
            return "crosswalk"
        if intent.environment_type == "t_junction":
            return "t_junction"
        if "narrow_sidewalk" in intent.risk_factors:
            return "narrow_sidewalk"
        if "static_obstacle_ahead" in intent.risk_factors:
            return "obstacle_corridor"
        return "straight_sidewalk"
