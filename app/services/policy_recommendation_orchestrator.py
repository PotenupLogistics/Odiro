from __future__ import annotations

import uuid
from datetime import datetime, timezone
from pathlib import Path

from app.core.settings import Settings
from app.models.llm import LlmProvider
from app.models.recommendation import (
    GenerationMethod,
    PolicyRecommendationResult,
)
from app.services.llm_client import BaseLlmClient
from app.services.measurement_log_parser import parse_measurement_log
from app.services.metrics_extractor import extract_run_metrics, extract_statistics
from app.services.policy_fallback_rules import (
    PolicyParamDefaults,
    apply_fallback_rules,
    build_fallback_summary,
)
from app.services.policy_recommendation_llm_client import generate_recommendations
from app.services.policy_recommendation_rag_retriever import (
    context_citation_ids,
    retrieve_policy_context,
)


def _now_iso() -> str:
    return datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")


def _attach_default_citations(
    recommendations,
    citations: list[str],
):
    """LLM이 citations 빈 채로 추천했을 때 RAG sourceIds로 보강."""
    if not citations:
        return recommendations
    enriched = []
    for rec in recommendations:
        if rec.citations:
            enriched.append(rec)
        else:
            enriched.append(rec.model_copy(update={"citations": list(citations)}))
    return enriched


def analyze_and_recommend(
    log_path: str | Path,
    provider: LlmProvider = LlmProvider.ollama,
    fallback_only: bool = False,
    defaults: PolicyParamDefaults | None = None,
    settings: Settings | None = None,
    llm_client: BaseLlmClient | None = None,
) -> PolicyRecommendationResult:
    """주행로그 분석 → 정책 파라미터 추천 전체 파이프라인."""
    cfg = settings or Settings()
    param_defaults = defaults or PolicyParamDefaults()

    log_data = parse_measurement_log(log_path)
    statistics = extract_statistics(log_data)
    run_metrics = extract_run_metrics(log_data, statistics)

    warnings: list[str] = []
    generation_method: GenerationMethod
    recommendations: list = []
    summary = ""

    if fallback_only:
        recommendations = apply_fallback_rules(statistics, param_defaults)
        summary = build_fallback_summary(recommendations)
        generation_method = "fallback_rules"
    else:
        contexts = retrieve_policy_context(statistics)
        outcome = generate_recommendations(
            statistics=statistics,
            contexts=contexts,
            provider=provider,
            defaults=param_defaults,
            settings=cfg,
            client=llm_client,
        )
        warnings.extend(outcome.warnings)
        if outcome.success and outcome.recommendations:
            citations = context_citation_ids(contexts)
            recommendations = _attach_default_citations(outcome.recommendations, citations)
            summary = outcome.summary
            generation_method = "llm_rag"
        else:
            recommendations = apply_fallback_rules(statistics, param_defaults)
            summary = build_fallback_summary(recommendations)
            generation_method = "fallback_after_llm_failure"

    return PolicyRecommendationResult(
        analysisId=str(uuid.uuid4()),
        logPath=str(log_data.sourcePath),
        generatedAt=_now_iso(),
        runMetrics=run_metrics,
        statistics=statistics,
        recommendations=recommendations,
        summary=summary,
        generationMethod=generation_method,
        llmWarnings=warnings,
    )
