from __future__ import annotations

from typing import Any, TypedDict

from app.agents.result_analysis_v2.routes import AnalysisRouteV2, RagRouteV2, RecommendationRouteV2


class ResultAnalysisGraphStateV2(TypedDict, total=False):
    """Internal LangGraph state for result-analysis v2 execution."""

    request: Any
    experiments_root: Any
    scan: Any
    artifacts: list[Any]
    classified_artifacts: list[Any]
    parsed_artifacts: list[Any]
    parse_warnings: list[str]
    episode_metrics: list[Any]
    episode_timelines: list[dict[str, Any]]
    representative_failed_episodes: list[dict[str, Any]]
    run_aggregates: list[dict[str, Any]]
    experiment_aggregates: list[dict[str, Any]]
    failure_patterns: list[dict[str, Any]]
    rag_queries: list[dict[str, Any]]
    retrieved_context: list[dict[str, Any]]
    rag_context: dict[str, Any]
    analysis_context: dict[str, Any]
    llm_analysis: dict[str, Any]
    recommendations: list[dict[str, Any]]
    detailed_recommendations: list[dict[str, Any]]
    validation_errors: list[str]
    response: Any
    warnings: list[str]
    analysis_mode: str
    overall_judgement: str
    analysis_route: AnalysisRouteV2
    rag_route: RagRouteV2
    recommendation_route: RecommendationRouteV2
    recommendation_validation_route: str
    rag_diagnostic: dict[str, Any]
