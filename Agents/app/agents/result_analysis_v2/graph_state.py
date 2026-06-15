from __future__ import annotations

from typing import Any, TypedDict


class ResultAnalysisGraphStateV2(TypedDict, total=False):
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
    modified_policy_json: list[dict[str, Any]]
    modified_environment_json: list[dict[str, Any]]
    validation_errors: list[str]
    response: Any
    warnings: list[str]
    analysis_mode: str
    overall_judgement: str
