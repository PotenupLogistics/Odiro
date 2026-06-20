from __future__ import annotations

from typing import Any

from app.agents.result_analysis_v2.analysis_text_builder import AnalysisTextBuilder
from app.agents.result_analysis_v2.review_text import INSUFFICIENT_DATA_SUMMARY_MESSAGE, default_artifacts
from app.models.analysis_v2 import (
    AnalysisMetricsV2,
    AnalysisRunV2Response,
    AnalysisScopeV2,
    AnalysisSummaryV2,
    RecommendationTypeV2,
)


class ResponseBuilder:
    """Builds the public v2 analysis response from aggregate metrics."""

    def __init__(self, *, analysis_text_builder: AnalysisTextBuilder | None = None) -> None:
        """Allow tests to provide deterministic response text builders."""
        self.analysis_text_builder = analysis_text_builder or AnalysisTextBuilder()

    def build(
        self,
        *,
        experiments_count: int,
        runs_count: int,
        episodes_count: int,
        metrics: AnalysisMetricsV2,
        patterns: list[dict[str, Any]],
        recommendations: list[dict[str, Any]],
        warnings: list[str],
        analysis_mode: str = "rule_based",
        run_id: str | None = None,
        review_id: str | None = None,
    ) -> AnalysisRunV2Response:
        """Create an initial response before review artifacts are finalized."""
        if episodes_count == 0:
            judgement = "insufficient_data"
            message = INSUFFICIENT_DATA_SUMMARY_MESSAGE
            recommendation_type: RecommendationTypeV2 = "insufficient_data"
            if message not in warnings:
                warnings = [*warnings, message]
        elif recommendations or patterns:
            judgement = "change_recommended"
            message = "반복적인 실패 패턴이 확인되어 정책 또는 환경 개선 검토가 필요합니다."
            recommendation_type = "none"
        else:
            judgement = "no_change_needed"
            message = "반복적인 실패 패턴이 확인되지 않아 정책 또는 환경 수정 추천을 생성하지 않았습니다."
            recommendation_type = "none"

        return AnalysisRunV2Response(
            run_id=run_id,
            review_id=review_id,
            analysis_scope=AnalysisScopeV2(
                experiments_count=experiments_count,
                runs_count=runs_count,
                episodes_count=episodes_count,
            ),
            summary=AnalysisSummaryV2(overall_judgement=judgement, message=message),
            metrics=metrics,
            recommendation_type=recommendation_type,
            patterns=patterns,
            recommendations=recommendations,
            modified_policy_json=self.modified_candidate_payloads(recommendations=recommendations, target="policy"),
            modified_environment_json=self.modified_candidate_payloads(
                recommendations=recommendations,
                target="environment",
            ),
            warnings=warnings,
            analysis_mode=analysis_mode,
            analysis_text=self.analysis_text_builder.build(
                recommendation_type="insufficient_data" if episodes_count == 0 else "none",
                artifacts=default_artifacts(),
                metrics=metrics,
                episodes_count=episodes_count,
                patterns=patterns,
            ),
        )

    def modified_candidate_payloads(self, *, recommendations: list[dict[str, Any]], target: str) -> list[dict[str, Any]]:
        """Build legacy modified_*_json payloads from normalized recommendation items."""
        return [
            {
                "source_recommendation_id": rec["id"],
                "target": target,
                "content": rec["proposed_change"]["content"],
            }
            for rec in recommendations
            if rec.get("target") == target
            and rec.get("id")
            and isinstance(rec.get("proposed_change"), dict)
            and isinstance(rec["proposed_change"].get("content"), dict)
        ]
