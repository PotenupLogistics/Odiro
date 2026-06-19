from __future__ import annotations

from dataclasses import dataclass
from typing import Any


@dataclass(frozen=True)
class RecommendationTypeDecision:
    """Recommendation type decision stored in recommendations.json."""

    recommendation_type: str
    reason: str
    evidence_ids: list[str]


@dataclass(frozen=True)
class RecommendationTypeDecider:
    """Decides the alpha-level recommendation type from evidence-backed findings."""

    def decide(
        self,
        *,
        summary_judgement: str,
        findings: list[dict[str, Any]],
        data_coverage: dict[str, Any],
    ) -> RecommendationTypeDecision:
        """Classify whether review should suggest policy, environment, none, or insufficient data."""
        if summary_judgement == "insufficient_data" or self._has_no_result_basis(data_coverage):
            return RecommendationTypeDecision(
                recommendation_type="insufficient_data",
                reason="Analysis did not have enough run result data.",
                evidence_ids=[],
            )

        if not findings:
            return RecommendationTypeDecision(
                recommendation_type="none",
                reason="No evidence-backed findings require policy or environment review.",
                evidence_ids=[],
            )

        environment_types = {"static_obstacle_collision", "blocked_region_collision"}
        if any(finding.get("type") in environment_types for finding in findings):
            return RecommendationTypeDecision(
                recommendation_type="environment_review",
                reason="Environment-related collision or blocked-region evidence was found.",
                evidence_ids=self._evidence_ids(findings, environment_types),
            )

        policy_types = {"penalty_region_violation", "timeout", "goal_not_reached", "near_miss"}
        return RecommendationTypeDecision(
            recommendation_type="policy_review",
            reason="Policy-related failure evidence was found.",
            evidence_ids=self._evidence_ids(findings, policy_types),
        )

    def _has_no_result_basis(self, data_coverage: dict[str, Any]) -> bool:
        """Detect runs that lack both episode result files and usable summary data."""
        return data_coverage.get("result_file_count", 0) == 0 and data_coverage.get("summary_json") != "present"

    def _evidence_ids(self, findings: list[dict[str, Any]], finding_types: set[str]) -> list[str]:
        """Collect evidence ids from selected finding types."""
        evidence_ids: list[str] = []
        for finding in findings:
            if finding.get("type") not in finding_types:
                continue
            evidence_ids.extend(str(evidence_id) for evidence_id in finding.get("evidence_ids", []))
        return evidence_ids
