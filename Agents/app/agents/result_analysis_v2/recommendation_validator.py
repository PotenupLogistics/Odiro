from __future__ import annotations

from typing import Any


class RecommendationValidator:
    def validate(self, recommendations: list[dict[str, Any]], known_episode_refs: set[tuple[str, str, str]]) -> list[dict[str, Any]]:
        valid: list[dict[str, Any]] = []
        for recommendation in recommendations:
            if not recommendation.get("id"):
                continue
            if recommendation.get("target") not in {"policy", "environment"}:
                continue
            proposed_change = recommendation.get("proposed_change")
            if not isinstance(proposed_change, dict) or not isinstance(proposed_change.get("content"), dict):
                continue
            evidence = recommendation.get("evidence", [])
            if evidence and not self._evidence_exists(evidence, known_episode_refs):
                continue
            valid.append(recommendation)
        return valid

    def _evidence_exists(self, evidence: list[Any], refs: set[tuple[str, str, str]]) -> bool:
        for item in evidence:
            if not isinstance(item, dict):
                return False
            ref = (str(item.get("experiment_id")), str(item.get("run_id")), str(item.get("episode_id")))
            if ref not in refs:
                return False
        return True
