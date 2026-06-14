from __future__ import annotations

from dataclasses import dataclass

from app.agents.scenario_generation_v2.intent_parser import ScenarioIntent


@dataclass(frozen=True)
class TemplatePlan:
    scenario_id: str
    scenario_type: str
    summary: str
    sidewalk_width_m: dict[str, float]
    obstacle_count: dict[str, int]
    pedestrian_count: dict[str, int]
    pedestrian_speed_mps: dict[str, float]
    risk_factors: list[str]
    assumptions: list[str]


class TemplatePlanner:
    def plan(self, intent: ScenarioIntent, scenario_type: str) -> TemplatePlan:
        narrow = scenario_type == "narrow_sidewalk"
        has_obstacle = "static_obstacle_ahead" in intent.risk_factors
        has_pedestrian = "pedestrian_crossing" in intent.risk_factors
        scenario_id = self._scenario_id(scenario_type, intent.risk_factors)

        return TemplatePlan(
            scenario_id=scenario_id,
            scenario_type=scenario_type,
            summary=self._summary(scenario_type, has_obstacle, has_pedestrian),
            sidewalk_width_m={"min": 1.0, "max": 1.5} if narrow else {"min": 1.5, "max": 2.2},
            obstacle_count={"min": 1, "max": 2} if has_obstacle else {"min": 0, "max": 0},
            pedestrian_count={"min": 1, "max": 3} if has_pedestrian else {"min": 0, "max": 0},
            pedestrian_speed_mps={"min": 0.8, "max": 1.4},
            risk_factors=intent.risk_factors,
            assumptions=[
                "좌표 단위는 meter로 가정했습니다.",
                "각도 단위는 degree로 가정했습니다.",
            ],
        )

    def _scenario_id(self, scenario_type: str, risk_factors: list[str]) -> str:
        suffix = "_".join(factor.replace("_ahead", "") for factor in risk_factors if factor != "baseline_navigation")
        return f"{scenario_type}_{suffix}" if suffix else scenario_type

    def _summary(self, scenario_type: str, has_obstacle: bool, has_pedestrian: bool) -> str:
        parts = [scenario_type.replace("_", " ")]
        if has_obstacle:
            parts.append("정적 장애물")
        if has_pedestrian:
            parts.append("보행자 횡단")
        return " / ".join(parts) + " 시나리오 템플릿을 생성했습니다."
