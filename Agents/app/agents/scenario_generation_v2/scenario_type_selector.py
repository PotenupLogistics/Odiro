from __future__ import annotations

from app.agents.scenario_generation_v2.intent_parser import ScenarioIntent


SUPPORTED_SCENARIO_TYPES = {
    "corridor_pose_navigation",
    "narrow_sidewalk_cross_path",
    "pinch_oncoming_pass",
    "static_obstacle_ahead",
}


class ScenarioTypeSelector:
    """Selects one alpha scenario pattern from parsed natural-language intent."""

    def select(self, intent: ScenarioIntent) -> str:
        if intent.robot_anchor_only:
            return "corridor_pose_navigation"
        if "oncoming_pass" in intent.risk_factors:
            return "pinch_oncoming_pass"
        if "static_obstacle_ahead" in intent.risk_factors and "cross_path" not in intent.risk_factors:
            return "static_obstacle_ahead"
        return "narrow_sidewalk_cross_path"
