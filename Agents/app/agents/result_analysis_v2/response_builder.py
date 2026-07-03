from __future__ import annotations

from typing import Any

from app.agents.result_analysis_v2.public_text_guardrail import (
    PUBLIC_INSIGHT_TEXT_FIELDS,
    PUBLIC_RECOMMENDATION_TEXT_FIELDS,
    contains_forbidden_public_text,
    record_contains_forbidden_public_text,
)
from app.agents.result_analysis_v2.review_text import INSUFFICIENT_DATA_SUMMARY_MESSAGE
from app.models.analysis_v2 import (
    AnalysisEpisodeV2,
    AnalysisInsightV2,
    AnalysisMetricsV2,
    AnalysisRunV2Response,
    AnalysisRunOverviewV2,
    AnalysisScopeV2,
    AnalysisSummaryV2,
    RecommendationTypeV2,
)


class ResponseBuilder:
    """Builds the public v2 analysis response from aggregate metrics."""

    def build(
        self,
        *,
        experiments_count: int,
        runs_count: int,
        episodes_count: int,
        metrics: AnalysisMetricsV2,
        run_overview: AnalysisRunOverviewV2 | None = None,
        episodes: list[AnalysisEpisodeV2] | None = None,
        insights: list[dict[str, Any]] | None = None,
        patterns: list[dict[str, Any]],
        recommendations: list[dict[str, Any]],
        warnings: list[str],
        analysis_mode: str = "rule_based",
        run_id: str | None = None,
        review_id: str | None = None,
    ) -> AnalysisRunV2Response:
        """Create an initial response before review artifacts are finalized."""
        _ = analysis_mode
        if episodes_count == 0:
            judgement = "insufficient_data"
            message = INSUFFICIENT_DATA_SUMMARY_MESSAGE
            recommendation_type: RecommendationTypeV2 = "insufficient_data"
        elif recommendations or patterns:
            judgement = "change_recommended"
            message = "반복적인 실패 패턴이 확인되어 정책 또는 환경 개선 검토가 필요합니다."
            recommendation_type = "none"
        else:
            judgement = "no_change_needed"
            message = "반복적인 실패 패턴이 확인되지 않아 정책 또는 환경 수정 추천을 생성하지 않았습니다."
            recommendation_type = "none"

        message = self._safe_summary_message(message)
        return AnalysisRunV2Response(
            run_id=run_id,
            review_id=review_id,
            run_overview=run_overview,
            episodes=episodes or [],
            analysis_scope=AnalysisScopeV2(
                experiments_count=experiments_count,
                runs_count=runs_count,
                episodes_count=episodes_count,
            ),
            summary=AnalysisSummaryV2(overall_judgement=judgement, message=message),
            metrics=metrics,
            recommendation_type=recommendation_type,
            insights=self.public_insights(insights or []),
            patterns=patterns,
            recommendations=self.public_recommendations(recommendations),
            warnings=self._public_warnings(warnings),
        )

    def _public_warnings(self, warnings: list[str]) -> list[str]:
        """Keep user-actionable analysis warnings while hiding collector noise."""
        public_warnings: list[str] = []
        for warning in warnings:
            normalized = warning.strip()
            lowered = normalized.casefold()
            if not normalized:
                continue
            if lowered.startswith("skipped large file:"):
                continue
            if lowered.startswith("skipped symlink in policy copy:"):
                continue
            public_warnings.append(normalized)
        return public_warnings

    def public_insights(self, insights: list[dict[str, Any]]) -> list[AnalysisInsightV2]:
        """Trim detailed insight records down to UI card fields."""
        public_items: list[AnalysisInsightV2] = []
        for insight in insights[:3]:
            if not isinstance(insight, dict):
                continue
            severity = str(insight.get("severity") or "medium").casefold()
            if severity not in {"high", "medium", "low"}:
                severity = "medium"
            description = str(insight.get("description") or insight.get("detail") or "")
            public_record = {"title": str(insight.get("title") or ""), "description": description}
            if record_contains_forbidden_public_text(public_record, PUBLIC_INSIGHT_TEXT_FIELDS):
                continue
            public_items.append(
                AnalysisInsightV2(
                    severity=severity,
                    title=public_record["title"],
                    description=description,
                )
            )
        return public_items

    def public_recommendations(self, recommendations: list[dict[str, Any]]) -> list[dict[str, Any]]:
        """Trim detailed recommendation records down to public display fields."""
        public_items: list[dict[str, Any]] = []
        for recommendation in recommendations:
            if not isinstance(recommendation, dict):
                continue
            public_item = {
                key: recommendation.get(key)
                for key in ("target", "priority", "title", "reason", "recommendation")
                if recommendation.get(key) is not None
            }
            if record_contains_forbidden_public_text(public_item, PUBLIC_RECOMMENDATION_TEXT_FIELDS):
                continue
            public_items.append(public_item)
        return public_items

    def _safe_summary_message(self, message: str) -> str:
        """Replace summary text only if an internal source marker reaches the public boundary."""
        if not contains_forbidden_public_text(message):
            return message
        return "제공된 실행 요약을 기준으로 결과를 확인했습니다."

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
