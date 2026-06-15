from __future__ import annotations

from dataclasses import dataclass

from app.agents.scenario_generation_v2.intent_parser import ScenarioIntent


@dataclass(frozen=True)
class TemplatePlan:
    """Normalized generation plan shared by template planners and response builders."""

    template_id: str
    pattern: str
    summary: str
    intent: str
    encounter_type: str
    persona: str
    include_obstacle: bool
    pedestrian_speed_mps: dict[str, float]
    risk_factors: list[str]
    assumptions: list[str]


class TemplatePlanner:
    """Builds a deterministic template plan from a selected alpha pattern."""

    def plan(self, intent: ScenarioIntent, scenario_type: str) -> TemplatePlan:
        encounter_type = "oncoming_pass" if scenario_type == "pinch_oncoming_pass" else "cross_path"
        include_obstacle = scenario_type in {"narrow_sidewalk_cross_path", "static_obstacle_ahead"}
        return TemplatePlan(
            template_id=scenario_type,
            pattern=scenario_type,
            summary=self._summary(scenario_type),
            intent=self._intent(scenario_type),
            encounter_type=encounter_type,
            persona="assertive" if scenario_type == "pinch_oncoming_pass" else "normal",
            include_obstacle=include_obstacle,
            pedestrian_speed_mps={"min": 0.8, "max": 1.4},
            risk_factors=intent.risk_factors,
            assumptions=[
                "template은 파일 저장 경로와 신규/수정 판단을 포함하지 않습니다.",
                "알파 단계 지원 패턴 중 가장 가까운 패턴으로 해석했습니다.",
            ],
        )

    def _summary(self, scenario_type: str) -> str:
        """Return a Korean user-facing summary for the selected pattern."""
        labels = {
            "narrow_sidewalk_cross_path": "좁은 보도에서 전방 장애물과 횡단 보행자가 함께 발생하는 scenario_template JSON을 생성했습니다.",
            "pinch_oncoming_pass": "협폭 구간에서 대향 보행자와 마주치는 scenario_template JSON을 생성했습니다.",
            "static_obstacle_ahead": "로봇 전방 경로 중앙의 정적 장애물을 회피하는 scenario_template JSON을 생성했습니다.",
        }
        return labels[scenario_type]

    def _intent(self, scenario_type: str) -> str:
        """Return the template intent text stored in the scenario_template root."""
        labels = {
            "narrow_sidewalk_cross_path": "좁은 보도에서 로봇 전방 장애물과 횡단 보행자가 동시에 발생할 때 로봇의 감속, 회피, 양보 판단을 검증한다.",
            "pinch_oncoming_pass": "협폭 구간에서 마주 오는 보행자와 조우할 때 로봇이 안전하게 감속, 양보, 통과하는지 검증한다.",
            "static_obstacle_ahead": "로봇 진행 경로 중앙의 정적 장애물 앞에서 로봇이 안전하게 감속하고 우회하는지 검증한다.",
        }
        return labels[scenario_type]
