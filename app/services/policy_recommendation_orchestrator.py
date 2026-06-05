from __future__ import annotations

import uuid
from datetime import datetime, timezone
from pathlib import Path

from app.core.settings import Settings
from app.models.bot_setup import BotLidarSetup, DeliveryBotSetup
from app.models.llm import LlmProvider
from app.models.recommendation import (
    EpisodeRecommendationResult,
    GenerationMethod,
    PolicyRecommendationResult,
)
from app.services.bot_setup_generator import generate_bot_setup
from app.services.evaluation_report_parser import parse_evaluation_report
from app.services.evaluation_report_statistics_extractor import (
    extract_statistics as extract_episode_statistics,
)
from app.services.llm_client import BaseLlmClient
from app.services.measurement_log_parser import parse_measurement_log
from app.services.metrics_extractor import extract_run_metrics, extract_statistics
from app.services.policy_fallback_rules import (
    PolicyParamDefaults,
    apply_episode_fallback_rules,
    apply_fallback_rules,
    build_episode_fallback_summary,
    build_fallback_summary,
)
from app.services.policy_recommendation_llm_client import (
    generate_episode_recommendations,
    generate_recommendations,
)
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


def analyze_episode_and_recommend(
    report_path: str | Path,
    bot_setup_path: str | Path | None = None,
    provider: LlmProvider = LlmProvider.ollama,
    fallback_only: bool = False,
    settings: Settings | None = None,
    llm_client: BaseLlmClient | None = None,
) -> EpisodeRecommendationResult:
    """EpisodeEvaluationReport 분석 → DeliveryBotSetup 파라미터 추천 + 다음 Setup 생성."""
    cfg = settings or Settings()

    report = parse_evaluation_report(report_path)
    statistics = extract_episode_statistics(report)

    if bot_setup_path is not None:
        import json
        raw = Path(bot_setup_path).read_text(encoding="utf-8")
        data = json.loads(raw)
        robot = data.get("robot", {})
        current_setup = DeliveryBotSetup(
            drive=robot.get("drive", {}),
            path_follow=robot.get("path_follow", {}),
            lidar=robot.get("lidar", {}),
        )
    else:
        current_setup = DeliveryBotSetup()

    lidar = current_setup.lidar
    drive = current_setup.drive
    pf = current_setup.path_follow

    warnings: list[str] = []
    generation_method: GenerationMethod
    recommendations: list = []
    summary = ""

    if not statistics.usable_for_llm_tuning:
        warnings.append("usable_for_llm_tuning=False — 이 결과는 LLM 튜닝 근거로 사용할 수 없음.")
        return EpisodeRecommendationResult(
            analysisId=str(uuid.uuid4()),
            reportPath=str(report_path),
            generatedAt=_now_iso(),
            statistics=statistics,
            recommendations=[],
            summary="튜닝 불가 결과 — 추천 생략.",
            generationMethod="fallback_rules",
            llmWarnings=warnings,
            nextBotSetup=None,
        )

    if fallback_only:
        recommendations = apply_episode_fallback_rules(
            statistics,
            current_stop_distance_m=lidar.stop_distance_m,
            current_slow_down_distance_m=lidar.slow_down_distance_m,
            current_max_speed_kmh=drive.max_speed_kmh,
            current_target_speed_kmh=pf.target_speed_kmh,
            current_obstacle_slow_speed_kmh=pf.obstacle_slow_speed_kmh,
            current_look_ahead_distance_m=pf.look_ahead_distance_m,
        )
        summary = build_episode_fallback_summary(recommendations)
        generation_method = "fallback_rules"
    else:
        contexts = retrieve_policy_context(statistics)
        outcome = generate_episode_recommendations(
            statistics=statistics,
            contexts=contexts,
            provider=provider,
            stop_distance_m=lidar.stop_distance_m,
            slow_down_distance_m=lidar.slow_down_distance_m,
            max_speed_kmh=drive.max_speed_kmh,
            target_speed_kmh=pf.target_speed_kmh,
            obstacle_slow_speed_kmh=pf.obstacle_slow_speed_kmh,
            look_ahead_distance_m=pf.look_ahead_distance_m,
            scan_range_m=lidar.scan_range_m,
            angle_step_degree=lidar.angle_step_degree,
            settings=cfg,
            client=llm_client,
        )
        warnings.extend(outcome.warnings)
        if outcome.success and outcome.recommendations:
            citations = context_citation_ids(contexts)
            recommendations = [
                rec if rec.citations else rec.model_copy(update={"citations": list(citations)})
                for rec in outcome.recommendations
            ]
            summary = outcome.summary
            generation_method = "llm_rag"
        else:
            recommendations = apply_episode_fallback_rules(
                statistics,
                current_stop_distance_m=lidar.stop_distance_m,
                current_slow_down_distance_m=lidar.slow_down_distance_m,
                current_max_speed_kmh=drive.max_speed_kmh,
                current_target_speed_kmh=pf.target_speed_kmh,
                current_obstacle_slow_speed_kmh=pf.obstacle_slow_speed_kmh,
                current_look_ahead_distance_m=pf.look_ahead_distance_m,
            )
            summary = build_episode_fallback_summary(recommendations)
            generation_method = "fallback_after_llm_failure"

    next_setup = generate_bot_setup(current_setup, recommendations)

    return EpisodeRecommendationResult(
        analysisId=str(uuid.uuid4()),
        reportPath=str(report_path),
        generatedAt=_now_iso(),
        statistics=statistics,
        recommendations=recommendations,
        summary=summary,
        generationMethod=generation_method,
        llmWarnings=warnings,
        nextBotSetup=next_setup.to_json_dict(),
    )
