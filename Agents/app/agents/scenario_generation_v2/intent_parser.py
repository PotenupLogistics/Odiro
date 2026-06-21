from __future__ import annotations

import re
from dataclasses import dataclass, field
from typing import Any


@dataclass(frozen=True)
class ScenarioIntent:
    """Coarse prompt signals that guide deterministic Project Scenario generation."""

    environment_type: str
    scenario_goal: str
    risk_factors: list[str] = field(default_factory=list)
    main_actor: str = "delivery_robot"
    difficulty: str = "medium"
    single_pedestrian: bool = False
    persona_hint: str | None = None
    explicit_blocking: bool = False
    requested_gate_obstacle_count: int | None = None
    robot_anchor_only: bool = False
    robot_start_anchor: dict[str, Any] | None = None
    robot_goal_anchor: dict[str, Any] | None = None
    corridor_profile: str = "default"


class IntentParser:
    """Extracts coarse scenario intent signals from the prompt without owning generation output."""

    def parse(self, prompt: str) -> ScenarioIntent:
        lowered = prompt.lower()
        risk_factors: list[str] = []
        environment_type = "sidewalk"
        difficulty = "medium"
        single_pedestrian = self._is_single_pedestrian_prompt(prompt, lowered)
        persona_hint = "vulnerable" if any(token in lowered for token in ("취약", "vulnerable")) else None
        explicit_blocking = self._is_explicit_blocking_prompt(prompt, lowered)
        requested_gate_obstacle_count = self._requested_gate_obstacle_count(prompt, lowered)
        corridor_profile = "curved-road" if self._is_curved_road_prompt(prompt, lowered) else "default"
        robot_start_anchor = self._corridor_pose_anchor(prompt, "approach", default_along=1.0)
        robot_goal_anchor = self._corridor_pose_anchor(prompt, "exit", default_along=16.0)
        robot_anchor_only = bool(
            ("corridor_pose" in lowered or "anchor" in lowered or "지점" in prompt or "시작점과 목표점" in prompt)
            and robot_start_anchor is not None
            and robot_goal_anchor is not None
            and not self._has_risk_element(prompt, lowered)
        )

        if any(token in prompt for token in ("좁", "협소")) or "narrow" in lowered:
            risk_factors.append("narrow_sidewalk")
            difficulty = "medium_high"
        if any(token in prompt for token in ("장애물", "공사 콘", "안내판")) or any(
            token in lowered for token in ("obstacle", "cone", "sign", "panel")
        ):
            risk_factors.append("static_obstacle_ahead")
        if any(token in prompt for token in ("옆에서", "가로지", "가로질", "끼어드", "횡단", "cross")):
            risk_factors.append("cross_path")
        if any(token in prompt for token in ("마주", "대향", "반대편", "oncoming")):
            risk_factors.append("oncoming_pass")
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
            single_pedestrian=single_pedestrian,
            persona_hint=persona_hint,
            explicit_blocking=explicit_blocking,
            requested_gate_obstacle_count=requested_gate_obstacle_count,
            robot_anchor_only=robot_anchor_only,
            robot_start_anchor=robot_start_anchor,
            robot_goal_anchor=robot_goal_anchor,
            corridor_profile=corridor_profile,
        )

    def _is_single_pedestrian_prompt(self, prompt: str, lowered: str) -> bool:
        """Return whether the prompt constrains the scene to one primary pedestrian."""
        return any(
            token in prompt
            for token in (
                "보행자 한 명",
                "보행자 1명",
                "한 명이",
            )
        ) or any(token in lowered for token in ("single pedestrian", "one pedestrian"))

    def _is_explicit_blocking_prompt(self, prompt: str, lowered: str) -> bool:
        """Return whether the user explicitly wants an impassable or intentionally blocked path."""
        return any(
            token in prompt
            for token in (
                "통로가 막",
                "길을 막",
                "지나갈 수 없",
                "통행 불가",
                "일부러",
            )
        ) or any(token in lowered for token in ("blocked path", "intentionally blocking", "impassable"))

    def _requested_gate_obstacle_count(self, prompt: str, lowered: str) -> int | None:
        """Return the explicit gate obstacle count when the prompt constrains it."""
        has_gate = "게이트" in prompt or "gate" in lowered
        has_two = any(token in prompt for token in ("두 개", "2개")) or any(
            token in lowered for token in ("two objects", "two panels", "two cones")
        )
        return 2 if has_gate and has_two else None

    def _is_curved_road_prompt(self, prompt: str, lowered: str) -> bool:
        """Return whether the prompt asks for the bundled curved-road corridor."""
        korean_tokens = ("곡선 도로", "커브", "커브길", "굽은 도로", "휘어진 도로")
        english_tokens = ("curved road", "curve", "bend")
        return any(token in prompt for token in korean_tokens) or any(token in lowered for token in english_tokens)

    def _has_risk_element(self, prompt: str, lowered: str) -> bool:
        """Return whether the prompt asks for obstacles, pedestrians, or risk interactions."""
        korean_tokens = ("장애물", "공사 콘", "안내판", "보행자", "가로지르는", "마주 오는", "대향", "추월", "군중", "충돌", "위험")
        english_tokens = ("obstacle", "pedestrian", "crossing", "oncoming", "crowd", "collision", "near miss", "risk", "blocked", "blocking")
        return any(token in prompt for token in korean_tokens) or any(token in lowered for token in english_tokens)

    def _corridor_pose_anchor(self, prompt: str, segment: str, *, default_along: float) -> dict[str, Any] | None:
        """Extract a corridor-local robot anchor for explicit start/goal pose prompts."""
        if segment not in prompt.lower():
            return None
        match = re.search(rf"{re.escape(segment)}[^0-9]*(\\d+(?:\\.\\d+)?)\\s*m?", prompt, re.IGNORECASE)
        along_m = float(match.group(1)) if match else default_along
        return {
            "type": "corridor_pose",
            "segment": segment,
            "along_m": along_m,
            "offset_m": 0.0,
            "lane": "center",
            "heading": "forward",
        }
