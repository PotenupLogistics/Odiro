from __future__ import annotations

from typing import Any

from app.models.analysis_v2 import AnalysisMetricsV2, AnalysisRunV2Response, AnalysisScopeV2, AnalysisSummaryV2


class ResponseBuilder:
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
    ) -> AnalysisRunV2Response:
        if episodes_count == 0:
            judgement = "insufficient_data"
            message = "분석 가능한 experiment 실행 결과를 찾지 못했습니다."
            if message not in warnings:
                warnings = [*warnings, message]
        elif recommendations or patterns:
            judgement = "change_recommended"
            message = "반복적인 실패 패턴이 확인되어 정책 또는 환경 개선 검토가 필요합니다."
        else:
            judgement = "no_change_needed"
            message = "반복적인 실패 패턴이 확인되지 않아 정책 또는 환경 수정 추천을 생성하지 않았습니다."

        return AnalysisRunV2Response(
            analysis_scope=AnalysisScopeV2(
                experiments_count=experiments_count,
                runs_count=runs_count,
                episodes_count=episodes_count,
            ),
            summary=AnalysisSummaryV2(overall_judgement=judgement, message=message),
            metrics=metrics,
            patterns=patterns,
            recommendations=recommendations,
            modified_policy_json=[
                {
                    "source_recommendation_id": rec["id"],
                    "target": "policy",
                    "content": rec["proposed_change"]["content"],
                }
                for rec in recommendations
                if rec.get("target") == "policy"
                and rec.get("id")
                and isinstance(rec.get("proposed_change"), dict)
                and isinstance(rec["proposed_change"].get("content"), dict)
            ],
            modified_environment_json=[
                {
                    "source_recommendation_id": rec["id"],
                    "target": "environment",
                    "content": rec["proposed_change"]["content"],
                }
                for rec in recommendations
                if rec.get("target") == "environment"
                and rec.get("id")
                and isinstance(rec.get("proposed_change"), dict)
                and isinstance(rec["proposed_change"].get("content"), dict)
            ],
            warnings=warnings,
            analysis_mode=analysis_mode,
        )
