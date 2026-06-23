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
    preset_profile: str | None = None
    requested_length_m: float | None = None
    requested_obstacle_count: int | None = None
    explicit_no_obstacles: bool = False


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
        requested_obstacle_count = self._requested_obstacle_count(prompt, lowered)
        if requested_obstacle_count is None and requested_gate_obstacle_count is not None:
            requested_obstacle_count = requested_gate_obstacle_count
        explicit_no_obstacles = self._is_explicit_no_obstacles_prompt(prompt, lowered)
        corridor_profile = "curved-road" if self._is_curved_road_prompt(prompt, lowered) else "default"
        requested_length_m = self._requested_length_m(prompt)
        robot_start_anchor = self._corridor_pose_anchor(prompt, "approach", default_along=1.0)
        robot_goal_anchor = self._corridor_pose_anchor(prompt, "exit", default_along=16.0)
        robot_anchor_only = bool(
            ("corridor_pose" in lowered or "anchor" in lowered or "지점" in prompt or "시작점과 목표점" in prompt)
            and robot_start_anchor is not None
            and robot_goal_anchor is not None
            and not self._has_risk_element(prompt, lowered)
        )
        preset_profile = self._preset_profile(prompt, lowered, requested_length_m, robot_anchor_only)

        if any(token in prompt for token in ("좁", "협소")) or "narrow" in lowered:
            risk_factors.append("narrow_sidewalk")
            difficulty = "medium_high"
        if not explicit_no_obstacles and (
            any(token in prompt for token in ("장애물", "공사", "바리케이드", "차단막", "공사 콘", "안내판"))
            or any(token in lowered for token in ("obstacle", "cone", "sign", "panel"))
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
            preset_profile=preset_profile,
            requested_length_m=requested_length_m,
            requested_obstacle_count=requested_obstacle_count,
            explicit_no_obstacles=explicit_no_obstacles,
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

    def _requested_obstacle_count(self, prompt: str, lowered: str) -> int | None:
        """Return an explicit obstacle count from common Korean and English phrasings."""
        korean_match = re.search(r"장애물\s*(\d+)\s*개|(\d+)\s*개(?:만)?\s*.*장애물", prompt)
        if korean_match:
            value = korean_match.group(1) or korean_match.group(2)
            return int(value)
        if any(token in prompt for token in ("장애물 두 개", "두 개의 장애물", "장애물 2개")):
            return 2
        generic_korean_match = re.search(r"(\d+)\s*개", prompt)
        if generic_korean_match and ("장애물" in prompt or "obstacle" in lowered or "cone" in lowered):
            return int(generic_korean_match.group(1))
        english_match = re.search(r"(?:obstacle[s]?\D+(\d+)|(\d+)\D+obstacle[s]?)", lowered)
        if english_match:
            value = english_match.group(1) or english_match.group(2)
            return int(value)
        if any(token in lowered for token in ("two obstacles", "two obstacle")):
            return 2
        return None

    def _is_explicit_no_obstacles_prompt(self, prompt: str, lowered: str) -> bool:
        """Return whether the prompt explicitly removes static obstacles."""
        korean_no_obstacle = re.search(r"장애물(?:은|는|이|가)?\s*(?:없|제거)", prompt) is not None
        return korean_no_obstacle or any(token in prompt for token in ("장애물 없는", "장애물 없이", "장애물 제거")) or any(
            token in lowered for token in ("without obstacle", "without obstacles", "no obstacle", "no obstacles")
        )

    def _requested_length_m(self, prompt: str) -> float | None:
        """Return the first explicit meter length requested for the corridor."""
        match = re.search(r"(\d+(?:\.\d+)?)\s*(?:m|미터)", prompt, re.IGNORECASE)
        return float(match.group(1)) if match else None

    def _preset_profile(
        self,
        prompt: str,
        lowered: str,
        requested_length_m: float | None,
        robot_anchor_only: bool,
    ) -> str | None:
        """Return the semantic preset family requested by natural-language tokens."""
        if robot_anchor_only:
            return None
        if any(token in prompt for token in ("빈 보도", "기본 보도")) or "blank" in lowered:
            return "blank"
        if any(token in prompt for token in ("S자", "s자", "에스자")) or any(
            token in lowered for token in ("s-curve", "s curve")
        ):
            return "s-curve"
        if any(token in prompt for token in ("공사", "바리케이드", "차단막")) or any(
            token in lowered for token in ("construction", "barricade")
        ):
            return "barricade"
        if self._is_curved_road_prompt(prompt, lowered):
            return "curved-road"
        if any(token in prompt for token in ("직선", "일자")) or any(
            token in lowered for token in ("straight", "line")
        ):
            return "line"
        return "line" if requested_length_m is not None else None

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
