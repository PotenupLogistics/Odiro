from __future__ import annotations

import re
from dataclasses import dataclass, field
from typing import Any

from app.agents.scenario_generation_v2.prop_normalizer import normalize_legacy_static_obstacle_prop_id
from app.catalogs.static_obstacle_catalog import get_allowed_static_obstacle_prop_ids


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
    requested_obstacle_counts: list[int] = field(default_factory=list)
    requested_prop: str | None = None
    explicit_no_obstacles: bool = False
    start_goal_clearance: bool = False
    requested_conflict_segment_count: int | None = None
    pedestrian_count: int | None = None
    explicit_no_pedestrians: bool = False

    @property
    def no_obstacles(self) -> bool:
        """Return the user-facing no-obstacle flag for fallback generation."""
        return self.explicit_no_obstacles


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
        requested_obstacle_counts = self._requested_obstacle_counts(prompt, lowered)
        requested_obstacle_count = sum(requested_obstacle_counts) if requested_obstacle_counts else None
        if requested_obstacle_count is None and requested_gate_obstacle_count is not None:
            requested_obstacle_count = requested_gate_obstacle_count
        requested_prop = self._requested_prop(prompt, lowered)
        start_goal_clearance = self._is_start_goal_clearance_prompt(prompt, lowered)
        requested_conflict_segment_count = self._requested_conflict_segment_count(prompt, lowered)
        explicit_no_obstacles = self._is_explicit_no_obstacles_prompt(prompt, lowered, start_goal_clearance)
        explicit_no_pedestrians = self._is_explicit_no_pedestrians_prompt(prompt, lowered)
        pedestrian_count = self._requested_pedestrian_count(prompt, lowered)
        requested_length_m = self._requested_length_m(prompt)
        robot_start_anchor = self._corridor_pose_anchor(prompt, "approach", default_along=1.0)
        robot_goal_anchor = self._corridor_pose_anchor(prompt, "exit", default_along=16.0)
        robot_anchor_only = bool(
            ("corridor_pose" in lowered or "anchor" in lowered or "지점" in prompt or "시작점과 목표점" in prompt)
            and robot_start_anchor is not None
            and robot_goal_anchor is not None
            and not self._has_risk_element(prompt, lowered)
        )
        if robot_anchor_only:
            requested_length_m = None
        corridor_profile = self._corridor_profile(prompt, lowered, requested_length_m)
        preset_profile = self._preset_profile(prompt, lowered, requested_length_m, robot_anchor_only)

        if any(token in prompt for token in ("좁", "협소")) or "narrow" in lowered:
            risk_factors.append("narrow_sidewalk")
            difficulty = "medium_high"
        if not explicit_no_obstacles and (
            any(token in prompt for token in ("장애물", "공사", "바리케이드", "차단막", "공사 콘", "콘", "꼬깔", "안내판"))
            or any(token in lowered for token in ("obstacle", "cone", "sign", "panel"))
        ):
            risk_factors.append("static_obstacle_ahead")
        if any(token in prompt for token in ("옆에서", "가로지", "가로질", "끼어드", "횡단", "cross")):
            risk_factors.append("cross_path")
        if any(token in prompt for token in ("마주", "대향", "반대편", "oncoming")):
            risk_factors.append("oncoming_pass")
        if not explicit_no_pedestrians and any(token in prompt for token in ("보행자", "pedestrian", "횡단", "cross")):
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
            requested_obstacle_counts=requested_obstacle_counts,
            requested_prop=requested_prop,
            explicit_no_obstacles=explicit_no_obstacles,
            start_goal_clearance=start_goal_clearance,
            requested_conflict_segment_count=requested_conflict_segment_count,
            pedestrian_count=pedestrian_count,
            explicit_no_pedestrians=explicit_no_pedestrians,
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

    def _requested_obstacle_counts(self, prompt: str, lowered: str) -> list[int]:
        """Return obstacle counts without confusing segment counts for placements."""
        counts: list[tuple[int, tuple[int, int]]] = []
        obstacle_terms = r"(?:장애물|콘|꼬깔|road\s*cone|obstacle\.[a-z0-9_]+)"
        patterns = (
            re.compile(rf"{obstacle_terms}\s*(\d+)\s*개", re.IGNORECASE),
            re.compile(rf"(\d+)\s*개(?:만|의)?\s*{obstacle_terms}", re.IGNORECASE),
        )
        for pattern in patterns:
            for match in pattern.finditer(lowered):
                value = next((group for group in match.groups() if group), None)
                if value is not None:
                    counts.append((int(value), match.span()))
        if any(token in prompt for token in ("장애물 두 개", "두 개의 장애물", "콘 두 개", "두 개의 콘")):
            counts.append((2, (-1, -1)))
        if not counts:
            english_match = re.search(r"(?:obstacle[s]?\D+(\d+)|(\d+)\D+obstacle[s]?)", lowered)
            if english_match:
                value = english_match.group(1) or english_match.group(2)
                counts.append((int(value), english_match.span()))
        if any(token in lowered for token in ("two obstacles", "two obstacle")):
            counts.append((2, (-2, -2)))
        deduped: list[int] = []
        seen_spans: set[tuple[int, int]] = set()
        for value, span in counts:
            if span in seen_spans:
                continue
            seen_spans.add(span)
            deduped.append(value)
        return deduped

    def _requested_prop(self, prompt: str, lowered: str) -> str | None:
        """Return a prompt-mentioned static obstacle prop id without inventing unknown aliases."""
        allowed_props = get_allowed_static_obstacle_prop_ids()
        if "콘" in prompt or "꼬깔" in prompt or "cone" in lowered:
            return "obstacle.road_cone_01"
        prop_pattern = r"(?:obstacle\.[a-z0-9_]+|[a-z][a-z0-9]*(?:_[a-z0-9]+)+_\d+)"
        match = re.search(prop_pattern, lowered)
        if match is None:
            return None
        requested = match.group(0)
        normalized = normalize_legacy_static_obstacle_prop_id(requested)
        if isinstance(normalized, str) and normalized in allowed_props:
            return normalized
        if requested in allowed_props:
            return requested
        prefixed = f"obstacle.{requested}"
        if "." not in requested and prefixed in allowed_props:
            return prefixed
        return requested

    def _is_explicit_no_obstacles_prompt(self, prompt: str, lowered: str, start_goal_clearance: bool) -> bool:
        """Return whether the prompt explicitly removes static obstacles."""
        if start_goal_clearance and not any(token in prompt for token in ("장애물 없는", "장애물 없이", "장애물 제거")):
            return any(token in lowered for token in ("without obstacle", "without obstacles", "no obstacle", "no obstacles"))
        korean_no_obstacle = re.search(r"장애물(?:은|는|이|가)?\s*(?:없|제거)", prompt) is not None
        return korean_no_obstacle or any(token in prompt for token in ("장애물 없는", "장애물 없이", "장애물 제거")) or any(
            token in lowered for token in ("without obstacle", "without obstacles", "no obstacle", "no obstacles")
        )

    def _is_start_goal_clearance_prompt(self, prompt: str, lowered: str) -> bool:
        """Return whether no-obstacle wording only protects robot anchors."""
        has_start = any(token in prompt for token in ("출발", "시작")) or "start" in lowered
        has_goal = any(token in prompt for token in ("도착", "목표")) or "goal" in lowered
        has_local = any(token in prompt for token in ("주변", "근처", "앞")) or any(
            token in lowered for token in ("near", "around")
        )
        has_no_obstacle = "장애물" in prompt and any(token in prompt for token in ("없게", "없도록", "없이"))
        return has_start and has_goal and has_local and has_no_obstacle

    def _requested_conflict_segment_count(self, prompt: str, lowered: str) -> int | None:
        """Return requested conflict-zone count without treating it as obstacle count."""
        match = re.search(r"(?:conflict|구간)\s*(?:구간)?(?:을|를)?\s*(\d+)\s*개", lowered)
        if match and ("conflict" in lowered or "좁아지는" in prompt):
            return int(match.group(1))
        return None

    def _is_explicit_no_pedestrians_prompt(self, prompt: str, lowered: str) -> bool:
        """Return whether the prompt explicitly removes pedestrians."""
        korean_no_pedestrian = re.search(r"보행자(?:는|은|이|가)?\s*(?:없|제거)", prompt) is not None
        return korean_no_pedestrian or any(token in prompt for token in ("보행자 없는", "보행자 없이", "보행자 제거")) or any(
            token in lowered for token in ("without pedestrian", "without pedestrians", "no pedestrian", "no pedestrians")
        )

    def _requested_pedestrian_count(self, prompt: str, lowered: str) -> int | None:
        """Return an explicit pedestrian count when the prompt includes one."""
        korean_match = re.search(r"보행자\s*(\d+)\s*명|(\d+)\s*명(?:만)?\s*.*보행자", prompt)
        if korean_match:
            value = korean_match.group(1) or korean_match.group(2)
            return int(value)
        if any(token in prompt for token in ("보행자 한 명", "한 명의 보행자")):
            return 1
        english_match = re.search(r"(?:pedestrian[s]?\D+(\d+)|(\d+)\D+pedestrian[s]?)", lowered)
        if english_match:
            value = english_match.group(1) or english_match.group(2)
            return int(value)
        return None

    def _requested_length_m(self, prompt: str) -> float | None:
        """Return the first explicit meter length requested for the corridor."""
        match = re.search(r"(\d+(?:\.\d+)?)\s*(?:m|미터)", prompt, re.IGNORECASE)
        return float(match.group(1)) if match else None

    def _corridor_profile(self, prompt: str, lowered: str, requested_length_m: float | None) -> str:
        """Return the natural-language corridor shape used by preset and fallback generation."""
        has_s_curve = any(token in prompt for token in ("S자", "s자", "에스자")) or any(
            token in lowered for token in ("s-curve", "s curve")
        )
        if has_s_curve and self._is_right_angle_corridor_prompt(prompt, lowered):
            return "complex"
        if has_s_curve:
            return "s-curve"
        if self._is_right_angle_corridor_prompt(prompt, lowered):
            return "g-shape"
        if any(token in prompt for token in ("공사", "바리케이드", "차단막")) or any(
            token in lowered for token in ("construction", "barricade")
        ):
            return "construction"
        if self._is_curved_road_prompt(prompt, lowered):
            return "curved"
        if requested_length_m is not None or any(token in prompt for token in ("직선", "일자")) or any(
            token in lowered for token in ("straight", "line")
        ):
            return "straight"
        return "straight"

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
        if (
            (any(token in prompt for token in ("S자", "s자", "에스자")) or any(token in lowered for token in ("s-curve", "s curve")))
            and self._is_right_angle_corridor_prompt(prompt, lowered)
        ):
            return None
        if any(token in prompt for token in ("빈 보도", "기본 보도")) or "blank" in lowered:
            return "blank"
        if any(token in prompt for token in ("S자", "s자", "에스자")) or any(
            token in lowered for token in ("s-curve", "s curve")
        ):
            return "s-curve"
        if self._is_right_angle_corridor_prompt(prompt, lowered) and not (
            any(token in prompt for token in ("공사", "바리케이드", "차단막"))
            or any(token in lowered for token in ("construction", "barricade"))
        ):
            return None
        if any(token in prompt for token in ("공사", "바리케이드", "차단막")) or any(
            token in lowered for token in ("construction", "barricade")
        ):
            return "barricade"
        if self._is_curved_road_prompt(prompt, lowered):
            return "curved"
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

    def _is_right_angle_corridor_prompt(self, prompt: str, lowered: str) -> bool:
        """Return whether the prompt asks for a right-angle or L-shaped corridor."""
        korean_tokens = ("ㄱ자", "L자", "l자", "엘자", "직각", "꺾인 도로", "코너")
        english_tokens = ("l-shaped", "l shaped", "right angle", "right-angle", "corner")
        return any(token in prompt for token in korean_tokens) or any(token in lowered for token in english_tokens)

    def _has_risk_element(self, prompt: str, lowered: str) -> bool:
        """Return whether the prompt asks for obstacles, pedestrians, or risk interactions."""
        korean_tokens = ("장애물", "공사 콘", "콘", "꼬깔", "안내판", "보행자", "가로지르는", "마주 오는", "대향", "추월", "군중", "충돌", "위험")
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
