from __future__ import annotations

from numbers import Real
from typing import Any

from app.agents.result_analysis_v2.policy_candidate_writer import PARAMETER_LIMITS, RUNTIME_CAP_PARAMETERS
from app.agents.result_analysis_v2.public_text_guardrail import (
    PUBLIC_RECOMMENDATION_TEXT_FIELDS,
    record_contains_forbidden_public_text,
)

# Supported public recommendation keys and conservative upper bounds for policy caps.
POLICY_PARAMETER_ADJUSTMENT_LIMITS = {
    f"{attribute_name}_max": PARAMETER_LIMITS[limit_name]
    for attribute_name, limit_name in RUNTIME_CAP_PARAMETERS
}


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
            if not self._proposed_change_is_valid(proposed_change):
                continue
            evidence = recommendation.get("evidence", [])
            if evidence and not self._evidence_exists(evidence, known_episode_refs):
                continue
            valid.append(recommendation)
        return valid

    def _proposed_change_is_valid(self, proposed_change: Any) -> bool:
        """Return whether a proposed change has a supported public shape."""
        if not isinstance(proposed_change, dict):
            return False
        content = proposed_change.get("content")
        if not isinstance(content, dict):
            return False
        if proposed_change.get("type") == "policy_parameter_adjustment":
            return self._policy_parameter_adjustment_is_valid(content)
        return True

    def _policy_parameter_adjustment_is_valid(self, content: dict[str, Any]) -> bool:
        """Validate LLM-proposed policy caps against supported keys and bounds."""
        if not content:
            return False
        for key, value in content.items():
            limit = POLICY_PARAMETER_ADJUSTMENT_LIMITS.get(str(key))
            if limit is None:
                return False
            if not self._is_numeric_parameter_value(value):
                return False
            numeric_value = float(value)
            if numeric_value < 0.0 or numeric_value > limit:
                return False
        return True

    def _is_numeric_parameter_value(self, value: Any) -> bool:
        """Return whether a value is a real numeric parameter, excluding booleans."""
        return isinstance(value, Real) and not isinstance(value, bool)

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
