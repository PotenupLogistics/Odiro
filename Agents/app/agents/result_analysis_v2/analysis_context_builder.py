from __future__ import annotations

from typing import Any


class AnalysisContextBuilder:
    def build(
        self,
        *,
        experiments_count: int,
        runs_count: int,
        episodes_count: int,
        experiment_summaries: list[dict[str, Any]],
        run_summaries: list[dict[str, Any]],
        failure_patterns: list[dict[str, Any]],
    ) -> dict[str, Any]:
        return {
            "analysis_goal": "전체 experiments 폴더를 분석하여 정책 또는 환경 개선 필요 여부를 판단한다.",
            "workspace_summary": {
                "experiments_count": experiments_count,
                "runs_count": runs_count,
                "episodes_count": episodes_count,
            },
            "experiment_summaries": experiment_summaries,
            "run_summaries": run_summaries,
            "failure_patterns": failure_patterns,
        }
