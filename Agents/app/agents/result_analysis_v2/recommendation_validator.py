from __future__ import annotations

from typing import Any

from app.agents.result_analysis_v2.public_text_guardrail import (
    PUBLIC_RECOMMENDATION_TEXT_FIELDS,
    record_contains_forbidden_public_text,
)


class RecommendationValidator:
    """Validates and normalizes recommendation items before response persistence."""

    def validate(self, recommendations: list[dict[str, Any]], known_episode_refs: set[tuple[str, str, str]]) -> list[dict[str, Any]]:
        """Keep only supported recommendations and normalize legacy text fields."""
        valid: list[dict[str, Any]] = []
        for recommendation in recommendations:
            recommendation = self._normalize_recommendation(recommendation)
            if not recommendation.get("id"):
                continue
            if recommendation.get("target") not in {"policy", "environment"}:
                continue
            if not isinstance(recommendation.get("recommendation"), str) or not recommendation["recommendation"]:
                continue
            if record_contains_forbidden_public_text(recommendation, PUBLIC_RECOMMENDATION_TEXT_FIELDS):
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
        """Return whether every referenced episode exists in the analysis input."""
        for item in evidence:
            if not isinstance(item, dict):
                return False
            ref = (str(item.get("experiment_id")), str(item.get("run_id")), str(item.get("episode_id")))
            if ref not in refs:
                return False
        return True

    def _normalize_recommendation(self, recommendation: dict[str, Any]) -> dict[str, Any]:
        """Convert legacy llm_recommendation text into the public recommendation field."""
        normalized = {key: value for key, value in recommendation.items() if key != "llm_recommendation"}
        recommendation_text = recommendation.get("recommendation") or recommendation.get("llm_recommendation")
        if isinstance(recommendation_text, str):
            normalized["recommendation"] = recommendation_text
        return normalized
