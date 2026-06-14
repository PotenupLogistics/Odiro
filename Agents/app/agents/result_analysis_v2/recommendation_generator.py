from __future__ import annotations

from typing import Any


class RecommendationGenerator:
    def generate(self, llm_analysis: dict[str, Any], patterns: list[dict[str, Any]]) -> list[dict[str, Any]]:
        recommendations = llm_analysis.get("recommendations", [])
        if isinstance(recommendations, list) and recommendations:
            return recommendations

        generated: list[dict[str, Any]] = []
        for pattern in patterns:
            pattern_type = pattern.get("type")
            if pattern_type == "blocked_region_violation_repeated":
                generated.append(self._policy_recommendation("REC-001", pattern))
        return generated

    def _policy_recommendation(self, recommendation_id: str, pattern: dict[str, Any]) -> dict[str, Any]:
        return {
            "id": recommendation_id,
            "target": "policy",
            "priority": "high",
            "title": "좁은 보도에서 정지 우선 조건 검토",
            "reason": "blocked region violation이 반복되어 회피 정책이 보도 경계를 벗어날 가능성이 있습니다.",
            "evidence": pattern.get("evidence", []),
            "llm_recommendation": (
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
