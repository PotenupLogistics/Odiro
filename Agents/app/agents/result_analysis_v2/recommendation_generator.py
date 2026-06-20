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
    ) -> list[dict[str, Any]]:
        """Ensure review recommendation arrays match the decided recommendation type."""
        if recommendation_type not in {"policy_review", "environment_review"}:
            return []
        target = "policy" if recommendation_type == "policy_review" else "environment"
        matching = [recommendation for recommendation in recommendations if recommendation.get("target") == target]
        if matching:
            return matching
        return [self._review_recommendation(recommendation_type=recommendation_type, findings=findings)]

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

    def _review_recommendation(self, *, recommendation_type: str, findings: list[dict[str, Any]]) -> dict[str, Any]:
        """Create the default review recommendation for the decided target."""
        finding_types = {str(finding.get("type")) for finding in findings}
        if recommendation_type == "environment_review":
            return self._environment_review_recommendation(finding_types)
        return self._policy_review_recommendation(finding_types)

    def _policy_review_recommendation(self, finding_types: set[str]) -> dict[str, Any]:
        """Create a conservative policy parameter recommendation item."""
        title = "주행 정책 보수화 검토"
        reason = "제한 시간 초과, 정체, 경로 이탈 또는 패널티 구역 침범 등 정책 검토가 필요한 근거가 확인되었습니다."
        if "penalty_region_violation" in finding_types:
            title = "위험 영역 회피와 경로 경계 유지 조건 검토"
            reason = "패널티 구역 침범이 확인되어 경로 추종 경계와 위험 영역 회피 조건을 보수적으로 조정할 필요가 있습니다."
        elif "pedestrian_collision" in finding_types:
            title = "보행자 충돌 대응 정책 검토"
            reason = "보행자 충돌 근거가 확인되어 보행자 근접 상황의 감속/정지 조건을 보수적으로 검토할 필요가 있습니다."
        elif {"timeout", "stuck"} & finding_types:
            title = "정체와 제한 시간 초과 대응 정책 검토"
            reason = "제한 시간 초과 또는 정체가 확인되어 감속, 정지, 재경로 탐색 조건을 검토할 필요가 있습니다."
        elif "near_miss" in finding_types:
            title = "근접 위험 상황의 감속/정지 우선 조건 검토"
            reason = "근접 위험 근거가 확인되어 보행자 또는 장애물 근접 상황에서 감속/정지 우선 조건을 검토할 필요가 있습니다."
        return {
            "id": "REC-001",
            "target": "policy",
            "priority": "high",
            "title": title,
            "reason": reason,
            "recommendation": (
                "반복 실패 구간에서 속도, 경로 추종 허용 오차, 전방 주시 거리, 조향 변화량을 "
                "보수적으로 조정해 동일 조건에서 재실행하는 것을 권장합니다."
            ),
            "proposed_change": {
                "type": "policy_parameter_adjustment",
                "content": {
                    "followSpeedKmh_max": 3.5,
                    "maxPathErrorM_max": 0.8,
                    "lookAheadDistanceM_max": 1.0,
                    "pathSmoothingDistanceM_max": 0.25,
                    "maxSteeringDelta_max": 0.06,
                },
            },
        }

    def _environment_review_recommendation(self, finding_types: set[str]) -> dict[str, Any]:
        """Create a conservative environment scenario recommendation item."""
        title = "정적 장애물 배치와 통로 폭 검토"
        reason = "정적 장애물 충돌이 반복되어 현재 장애물 배치 또는 유효 통로 폭이 주행에 충분하지 않을 가능성이 있습니다."
        content_reason = "static_obstacle_collision_repeated"
        if "blocked_region_collision" in finding_types and "static_obstacle_collision" not in finding_types:
            title = "차단 영역과 통로 배치 검토"
            reason = "차단 구역 충돌 또는 침범이 반복되어 통과 가능 영역과 차단 영역 배치 검토가 필요합니다."
            content_reason = "blocked_region_collision_repeated"
        return {
            "id": "REC-001",
            "target": "environment",
            "priority": "high",
            "title": title,
            "reason": reason,
            "recommendation": (
                "장애물 배치가 주행 경로를 과도하게 막지 않도록 최소 통로 폭을 늘리고, "
                "차단 배치를 비활성화한 환경 수정 후보로 재실행해 충돌과 제한 시간 초과가 줄어드는지 확인하세요."
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
