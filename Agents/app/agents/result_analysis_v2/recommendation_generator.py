from __future__ import annotations

from typing import Any


class RecommendationGenerator:
    """Creates deterministic recommendation items when LLM output is absent."""

    def generate(self, llm_analysis: dict[str, Any], patterns: list[dict[str, Any]]) -> list[dict[str, Any]]:
        """Return LLM recommendations or pattern-based alpha recommendations."""
        recommendations = llm_analysis.get("recommendations", [])
        if isinstance(recommendations, list) and recommendations:
            return recommendations

        generated: list[dict[str, Any]] = []
        for pattern in patterns:
            pattern_type = pattern.get("type")
            if pattern_type == "blocked_region_violation_repeated":
                generated.append(self._policy_recommendation("REC-001", pattern))
        return generated

    def ensure_for_review(
        self,
        *,
        recommendations: list[dict[str, Any]],
        recommendation_type: str,
        findings: list[dict[str, Any]],
        metrics: Any | None = None,
    ) -> list[dict[str, Any]]:
        """Ensure review recommendation arrays match the decided recommendation type."""
        if recommendation_type not in {"policy_review", "environment_review"}:
            return []
        target = "policy" if recommendation_type == "policy_review" else "environment"
        matching = [recommendation for recommendation in recommendations if recommendation.get("target") == target]
        if matching:
            return matching[:3]
        return self._review_recommendations(recommendation_type=recommendation_type, findings=findings, metrics=metrics)

    def _policy_recommendation(self, recommendation_id: str, pattern: dict[str, Any]) -> dict[str, Any]:
        """Create a policy recommendation for blocked-region repeated patterns."""
        return {
            "id": recommendation_id,
            "target": "policy",
            "priority": "high",
            "title": "좁은 보도에서 정지 우선 조건 검토",
            "reason": "차단 구역 침범이 반복되어 회피 정책이 보도 경계를 벗어날 가능성이 있습니다.",
            "evidence": pattern.get("evidence", []),
            "recommendation": (
                "좁은 보도에서 전방 장애물 또는 보행자 근접 상황이 발생하면 "
                "우회보다 감속 또는 정지를 우선하는 조건을 검토하세요."
            ),
            "proposed_change": {
                "type": "policy_rule_spec",
                "content": {
                    "rule_name": "stop_first_on_narrow_sidewalk",
                    "condition": {"blocked_region_violation_repeated": True},
                    "action": "slow_down_or_stop",
                    "priority": "high",
                },
            },
        }

    def _review_recommendation(
        self,
        *,
        recommendation_type: str,
        findings: list[dict[str, Any]],
        metrics: Any | None,
    ) -> dict[str, Any]:
        """Create the default review recommendation for the decided target."""
        recommendations = self._review_recommendations(
            recommendation_type=recommendation_type,
            findings=findings,
            metrics=metrics,
        )
        return recommendations[0]

    def _review_recommendations(
        self,
        *,
        recommendation_type: str,
        findings: list[dict[str, Any]],
        metrics: Any | None,
    ) -> list[dict[str, Any]]:
        """Create deterministic review recommendations for the decided target."""
        finding_types = {str(finding.get("type")) for finding in findings}
        if recommendation_type == "environment_review":
            recommendations = [self._environment_review_recommendation(finding_types)]
            if self._has_runtime_policy_signal(finding_types):
                recommendations.extend(
                    self._policy_review_recommendations(
                        finding_types,
                        success_safety_case=self._success_safety_case(metrics),
                        has_collision=self._has_collision_signal(finding_types=finding_types, metrics=metrics),
                        mixed_outcomes=self._mixed_outcomes(metrics),
                    )
                )
            return self._renumber_recommendations(recommendations[:3])
        return self._renumber_recommendations(self._policy_review_recommendations(
            finding_types,
            success_safety_case=self._success_safety_case(metrics),
            has_collision=self._has_collision_signal(finding_types=finding_types, metrics=metrics),
            mixed_outcomes=self._mixed_outcomes(metrics),
        )[:3])

    def _policy_review_recommendation(
        self,
        finding_types: set[str],
        *,
        success_safety_case: bool,
        has_collision: bool,
    ) -> dict[str, Any]:
        """Create a conservative policy parameter recommendation item."""
        return self._policy_review_recommendations(
            finding_types,
            success_safety_case=success_safety_case,
            has_collision=has_collision,
            mixed_outcomes=False,
        )[0]

    def _policy_review_recommendations(
        self,
        finding_types: set[str],
        *,
        success_safety_case: bool,
        has_collision: bool,
        mixed_outcomes: bool,
    ) -> list[dict[str, Any]]:
        """Create conservative policy parameter recommendation items."""
        recommendations: list[dict[str, Any]] = []

        def add_policy_item(
            *,
            priority: str,
            title: str,
            reason_bullets: list[str],
            recommendation_bullets: list[str],
        ) -> None:
            recommendations.append(
                {
                    "id": f"REC-{len(recommendations) + 1:03d}",
                    "target": "policy",
                    "priority": priority,
                    "title": title,
                    "reason": self._bullet_text("이유", reason_bullets),
                    "recommendation": self._bullet_text("확인 항목", recommendation_bullets),
                    "proposed_change": self._policy_proposed_change(),
                }
            )

        if "penalty_region_violation" in finding_types:
            add_policy_item(
                priority="high",
                title="경로 경계와 위험 영역 회피 조건 점검",
                reason_bullets=["패널티 구역 침범 신호", "경로 추종 경계와 위험 영역 회피 조건 영향 가능성"],
                recommendation_bullets=["경로 추종 조건", "위험 영역 회피 기준", "경계 이탈 보정 기준"],
            )
        if "near_miss" in finding_types:
            add_policy_item(
                priority="high" if has_collision else "medium",
                title="Near Miss 대응 조건 점검",
                reason_bullets=(
                    ["충돌 문제와 근접 위험 동시 발생", "회피 조건과 환경 배치의 복합 영향 가능성"]
                    if has_collision
                    else ["근접 위험 반복", "회피 여유 거리 부족 가능성"]
                ),
                recommendation_bullets=["감속 시작 거리", "회피 여유 거리", "정지 판단 기준"],
            )
        if {"timeout", "goal_not_reached", "stuck"} & finding_types:
            add_policy_item(
                priority="high",
                title="경로 추종 안정성 점검",
                reason_bullets=["목표 도달 실패와 시간 초과 신호", "충돌보다 경로 효율 문제 가능성"],
                recommendation_bullets=["경로 추종 허용 오차", "전방 주시 거리", "경로 보정 기준"],
            )
        if "repath" in finding_types:
            add_policy_item(
                priority="medium",
                title="재경로 탐색 진입 조건 점검",
                reason_bullets=["재경로 탐색 반복 시 주행 시간 증가", "우회 판단 반복으로 목표 도달 지연 가능성"],
                recommendation_bullets=["재탐색 시작 거리 기준", "정체 판단 기준", "우회 경로 선택 조건"],
            )
        if success_safety_case and not recommendations:
            add_policy_item(
                priority="medium",
                title="성공 run 안전 신호 재검증",
                reason_bullets=["성공 결과 안의 안전·정책 검토 신호", "재현 조건에 따라 실패로 전환될 가능성"],
                recommendation_bullets=["경로 추종 조건", "위험 영역 회피 기준", "재경로 탐색 발생 시점"],
            )
        if "robot_tip_over" in finding_types:
            add_policy_item(
                priority="medium",
                title="로봇 주행 안정성 점검",
                reason_bullets=["전복 이벤트 신호", "속도 또는 조향 변화량 영향 가능성"],
                recommendation_bullets=["최대 속도", "조향 변화량", "장애물 접촉 이후 자세 안정성"],
            )
        if "pedestrian_collision" in finding_types:
            add_policy_item(
                priority="high",
                title="보행자 근접 대응 조건 점검",
                reason_bullets=["보행자 충돌 신호", "근접 상황의 감속·정지 판단 부족 가능성"],
                recommendation_bullets=["보행자 감속 시작 거리", "정지 판단 기준", "회피 여유 거리"],
            )
        if mixed_outcomes:
            add_policy_item(
                priority="low",
                title="성공·실패 episode 비교",
                reason_bullets=["동일 run 안에서 성공과 실패가 함께 발생", "특정 구간 조건 차이 확인 필요"],
                recommendation_bullets=["성공 episode와 실패 episode의 경로 차이", "재탐색 발생 시점", "정지·감속 지속 구간"],
            )
        if not recommendations:
            add_policy_item(
                priority="high",
                title="주행 정책 파라미터 점검",
                reason_bullets=["주행 정책 검토 신호", "경로 추종과 재경로 탐색 조건 영향 가능성"],
                recommendation_bullets=["경로 추종 허용 오차", "전방 주시 거리", "경로 보정 기준"],
            )
        return recommendations[:3]

    def _bullet_text(self, heading: str, bullets: list[str]) -> str:
        """Format one public recommendation field as UI-ready bullet text."""
        section_bullets = [bullet.strip() for bullet in bullets if bullet and bullet.strip()]
        lines = [heading]
        lines.extend(f"- {bullet}" for bullet in section_bullets[:3])
        return "\n".join(lines)

    def _policy_proposed_change(self) -> dict[str, Any]:
        """Return the conservative policy cap payload shared by deterministic recommendations."""
        return {
            "type": "policy_parameter_adjustment",
            "content": {
                "followSpeedKmh_max": 3.5,
                "maxPathErrorM_max": 0.8,
                "lookAheadDistanceM_max": 1.0,
                "pathSmoothingDistanceM_max": 0.25,
                "maxSteeringDelta_max": 0.06,
            },
        }

    def _success_safety_case(self, metrics: Any | None) -> bool:
        """Return whether a policy review comes from safety evidence in successful runs."""
        if metrics is None:
            return False
        if hasattr(metrics, "success_count") and hasattr(metrics, "failure_count"):
            return bool(metrics.success_count > 0 and metrics.failure_count == 0)
        if isinstance(metrics, dict):
            return bool(metrics.get("success_count", 0) > 0 and metrics.get("failure_count", 0) == 0)
        return False

    def _has_collision_signal(self, *, finding_types: set[str], metrics: Any | None) -> bool:
        """Return whether recommendations should treat Near Miss as collision-adjacent."""
        if finding_types & {"static_obstacle_collision", "blocked_region_collision", "pedestrian_collision"}:
            return True
        if metrics is None:
            return False
        if hasattr(metrics, "collision_count"):
            return bool(getattr(metrics, "collision_count", 0) > 0)
        if isinstance(metrics, dict):
            return bool(metrics.get("collision_count", 0) > 0)
        return False

    def _mixed_outcomes(self, metrics: Any | None) -> bool:
        """Return whether the run contains both successful and failed episodes."""
        return self._metric_value(metrics, "success_count") > 0 and self._metric_value(metrics, "failure_count") > 0

    def _metric_value(self, metrics: Any | None, metric_name: str) -> int:
        """Read one non-negative integer metric from a model or dict."""
        if metrics is None:
            return 0
        if hasattr(metrics, metric_name):
            value = getattr(metrics, metric_name, 0)
        elif isinstance(metrics, dict):
            value = metrics.get(metric_name, 0)
        else:
            value = 0
        return max(0, int(value)) if isinstance(value, int | float) else 0

    def _has_runtime_policy_signal(self, finding_types: set[str]) -> bool:
        """Return whether an environment review has separate runtime policy signals."""
        return bool(
            finding_types
            & {
                "timeout",
                "goal_not_reached",
                "stuck",
                "near_miss",
                "repath",
                "robot_tip_over",
            }
        )

    def _renumber_recommendations(self, recommendations: list[dict[str, Any]]) -> list[dict[str, Any]]:
        """Return recommendations with stable unique ids after merging targets."""
        renumbered: list[dict[str, Any]] = []
        for index, recommendation in enumerate(recommendations, start=1):
            item = dict(recommendation)
            item["id"] = f"REC-{index:03d}"
            renumbered.append(item)
        return renumbered

    def _environment_review_recommendation(self, finding_types: set[str]) -> dict[str, Any]:
        """Create a conservative environment scenario recommendation item."""
        title = "정적 장애물 배치와 통로 폭 검토"
        reason = self._bullet_text(
            "이유",
            ["정적 장애물 충돌 반복", "현재 장애물 배치 또는 유효 통로 폭 부족 가능성"],
        )
        content_reason = "static_obstacle_collision_repeated"
        if "blocked_region_collision" in finding_types and "static_obstacle_collision" not in finding_types:
            title = "차단 영역과 통로 배치 검토"
            reason = self._bullet_text(
                "이유",
                ["차단 구역 충돌 또는 침범 반복", "통과 가능 영역과 차단 영역 배치 영향 가능성"],
            )
            content_reason = "blocked_region_collision_repeated"
        return {
            "id": "REC-001",
            "target": "environment",
            "priority": "high",
            "title": title,
            "reason": reason,
            "recommendation": self._bullet_text(
                "확인 항목",
                ["최소 통로 폭", "차단 배치 비활성 후보", "환경 수정 후보 재실행 결과"],
            ),
            "proposed_change": {
                "type": "environment_scenario_adjustment",
                "content": {
                    "increase_min_clear_width_m": True,
                    "disable_allow_blocking": True,
                    "increase_walkway_width_m": True,
                    "reason": content_reason,
                },
            },
        }
