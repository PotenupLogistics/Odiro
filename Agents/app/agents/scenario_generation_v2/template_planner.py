from __future__ import annotations

from dataclasses import dataclass
from typing import Any

from app.agents.scenario_generation_v2.intent_parser import ScenarioIntent


@dataclass(frozen=True)
class TemplatePlan:
    """Normalized generation plan shared by scenario planners and response builders."""

    scenario_id: str
    pattern: str
    summary: str
    intent: str
    encounter_type: str
    persona: str
    include_obstacle: bool
    single_pedestrian: bool
    explicit_blocking: bool
    requested_gate_obstacle_count: int | None
    robot_anchor_only: bool
    robot_start_anchor: dict[str, Any] | None
    robot_goal_anchor: dict[str, Any] | None
    pedestrian_speed_mps: dict[str, float]
    risk_factors: list[str]
    assumptions: list[str]


class TemplatePlanner:
    """Builds a deterministic template plan from a selected alpha pattern."""

    def plan(self, intent: ScenarioIntent, scenario_type: str) -> TemplatePlan:
        encounter_type = "oncoming_pass" if scenario_type == "pinch_oncoming_pass" else "cross_path"
        include_obstacle = scenario_type in {"narrow_sidewalk_cross_path", "static_obstacle_ahead"}
        persona = intent.persona_hint or ("assertive" if scenario_type == "pinch_oncoming_pass" else "normal")
        return TemplatePlan(
            scenario_id=scenario_type,
            pattern=scenario_type,
            summary=self._summary(scenario_type),
            intent=self._intent(scenario_type),
            encounter_type=encounter_type,
            persona=persona,
            include_obstacle=include_obstacle,
            single_pedestrian=intent.single_pedestrian,
            explicit_blocking=intent.explicit_blocking,
            requested_gate_obstacle_count=intent.requested_gate_obstacle_count,
            robot_anchor_only=intent.robot_anchor_only,
            robot_start_anchor=intent.robot_start_anchor,
            robot_goal_anchor=intent.robot_goal_anchor,
            pedestrian_speed_mps={"min": 0.8, "max": 1.4},
            risk_factors=intent.risk_factors,
            assumptions=[
                "scenario는 파일 저장 경로와 신규/수정 판단을 포함하지 않습니다.",
                "알파 단계 지원 패턴 중 가장 가까운 패턴으로 해석했습니다.",
            ],
        )

    def _summary(self, scenario_type: str) -> str:
        """Return a Korean user-facing summary for the selected pattern."""
        labels = {
            "corridor_pose_navigation": "명시된 corridor_pose 시작/목표 anchor를 가진 최소 scenario JSON을 생성했습니다.",
            "narrow_sidewalk_cross_path": "좁은 보도에서 전방 장애물과 횡단 보행자가 함께 발생하는 scenario JSON을 생성했습니다.",
            "pinch_oncoming_pass": "협폭 구간에서 대향 보행자와 마주치는 scenario JSON을 생성했습니다.",
            "static_obstacle_ahead": "로봇 전방 경로 중앙의 정적 장애물을 회피하는 scenario JSON을 생성했습니다.",
        }
        return labels[scenario_type]

    def _intent(self, scenario_type: str) -> str:
        """Return the intent text stored in the scenario root."""
        labels = {
            "corridor_pose_navigation": "명시된 corridor_pose 시작점과 목표점을 사용해 로봇의 기본 보도 주행 anchor 표현을 검증한다.",
            "narrow_sidewalk_cross_path": "좁은 보도에서 로봇 전방 장애물과 횡단 보행자가 동시에 발생할 때 로봇의 감속, 회피, 양보 판단을 검증한다.",
            "pinch_oncoming_pass": "협폭 구간에서 마주 오는 보행자와 조우할 때 로봇이 안전하게 감속, 양보, 통과하는지 검증한다.",
            "static_obstacle_ahead": "로봇 진행 경로 중앙의 정적 장애물 앞에서 로봇이 안전하게 감속하고 우회하는지 검증한다.",
        }
        return labels[scenario_type]
