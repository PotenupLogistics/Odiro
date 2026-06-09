from __future__ import annotations

import uuid
from datetime import datetime, timezone
from pathlib import Path
from typing import Any

from app.core.settings import Settings
from app.models.bot_setup import DeliveryBotSetup
from app.models.llm import LlmProvider
from app.models.recommendation import (
    GenerationMethod,
    IntegratedRecommendationResult,
)
from app.services.bot_setup_generator import generate_bot_setup
from app.services.episode_setup_patcher import apply_episode_setup_recommendations
from app.services.evaluation_report_parser import parse_evaluation_report
from app.services.evaluation_report_statistics_extractor import (
    extract_statistics as extract_episode_statistics,
)
from app.services.llm_client import BaseLlmClient
from app.services.measurement_log_parser import parse_measurement_log_lenient
from app.services.metrics_extractor import extract_statistics
from app.services.policy_fallback_rules import (
    apply_episode_fallback_rules,
    apply_episode_setup_fallback_rules,
    apply_policy_server_fallback_rules,
    build_episode_fallback_summary,
    build_episode_setup_fallback_summary,
    build_policy_server_fallback_summary,
)
from app.services.policy_recommendation_llm_client import (
    generate_integrated_recommendations,
)
from app.services.policy_recommendation_rag_retriever import (
    context_citation_ids_multi,
    retrieve_episode_setup_context,
    retrieve_policy_context,
    retrieve_policy_server_context,
)
from app.services.policy_server_inspector import extract_policy_defaults_from_source
from app.services.policy_source_analyzer import (
    analyze_policy_server_source,
    check_param_consistency,
)


def _now_iso() -> str:
    return datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")


# ── 5-입력 통합 분석 ─────────────────────────────────────────────────────

def _load_bot_setup_from_path(bot_setup_path: str | Path) -> tuple[DeliveryBotSetup, dict[str, Any]]:
    import json as _json
    raw = Path(bot_setup_path).read_text(encoding="utf-8-sig")
    data = _json.loads(raw)
    robot = data.get("robot", {}) or {}
    setup = DeliveryBotSetup(
        drive=robot.get("drive", {}) or {},
        path_follow=robot.get("path_follow", {}) or {},
        lidar=robot.get("lidar", {}) or {},
    )
    return setup, data


def _load_episode_setup_dict(episode_setup_path: str | Path) -> dict[str, Any]:
    import json as _json
    return _json.loads(Path(episode_setup_path).read_text(encoding="utf-8-sig"))


def _first_pedestrian_speed_mps(episode_setup: dict[str, Any]) -> float | None:
    actors = episode_setup.get("actors", {}) or {}
    peds = actors.get("pedestrians", []) or []
    for p in peds:
        if not isinstance(p, dict):
            continue
        mv = p.get("movement", {}) or {}
        speed = mv.get("speed_mps")
        if isinstance(speed, (int, float)):
            return float(speed)
    return None


def _summarize_setup_snapshot(
    episode_setup: dict[str, Any],
    bot_setup_raw: dict[str, Any],
    policy_defaults: dict[str, Any],
) -> dict[str, Any]:
    run = episode_setup.get("run", {}) or {}
    evaluation = episode_setup.get("evaluation", {}) or {}
    near_miss = evaluation.get("near_miss", {}) or {}
    actors = episode_setup.get("actors", {}) or {}
    return {
        "episode_setup": {
            "scenario_id": episode_setup.get("scenario_id"),
            "run.time_limit_s": run.get("time_limit_s"),
            "evaluation.goal_acceptance_radius_m": evaluation.get("goal_acceptance_radius_m"),
            "evaluation.near_miss.distance_m": near_miss.get("distance_m"),
            "evaluation.tip_over_angle_deg": evaluation.get("tip_over_angle_deg"),
            "pedestrians.count": len(actors.get("pedestrians", []) or []),
            "static_obstacles.count": len(actors.get("static_obstacles", []) or []),
        },
        "bot_setup": bot_setup_raw.get("robot", {}),
        "policy_server": policy_defaults,
    }


def analyze_full_setup_and_recommend(
    *,
    evaluation_report_path: str | Path,
    measurement_log_path: str | Path,
    episode_setup_path: str | Path,
    bot_setup_path: str | Path,
    policy_server_path: str | Path | None = None,
    provider: LlmProvider = LlmProvider.openai,
    fallback_only: bool = False,
    output_path: str | Path | None = None,
    settings: Settings | None = None,
    llm_client: BaseLlmClient | None = None,
) -> IntegratedRecommendationResult:
    """5-입력 통합 분석·추천 파이프라인."""
    cfg = settings or Settings()

    # 1. 5개 입력 파싱
    report = parse_evaluation_report(evaluation_report_path)
    episode_statistics = extract_episode_statistics(report)

    # footer 없는 로그도 허용 (언리얼이 footer를 기록 못 한 경우 대비)
    log_data = parse_measurement_log_lenient(measurement_log_path)
    measurement_statistics = extract_statistics(log_data)

    episode_setup_dict = _load_episode_setup_dict(episode_setup_path)
    bot_setup, bot_setup_raw = _load_bot_setup_from_path(bot_setup_path)
    policy_defaults = extract_policy_defaults_from_source(policy_server_path)

    # policy_server.py 정적 분석 + BotSetup↔PolicyServer 정합성 검사
    if policy_server_path:
        policy_source_analysis = analyze_policy_server_source(policy_server_path)
        param_consistency_check = check_param_consistency(
            bot_setup_raw, episode_setup_dict, policy_source_analysis
        )
    else:
        policy_source_analysis = None
        param_consistency_check = None

    inputs = {
        "evaluation_report": str(evaluation_report_path),
        "measurement_log": str(measurement_log_path),
        "episode_setup": str(episode_setup_path),
        "bot_setup": str(bot_setup_path),
        "policy_server": str(policy_server_path) if policy_server_path else "",
    }

    snapshot = _summarize_setup_snapshot(episode_setup_dict, bot_setup_raw, policy_defaults)

    warnings: list[str] = []

    # 2. usable_for_llm_tuning=False면 빈 추천 즉시 반환
    if not episode_statistics.usable_for_llm_tuning:
        warnings.append("usable_for_llm_tuning=False — 이 결과는 LLM 튜닝 근거로 사용할 수 없음.")
        result = IntegratedRecommendationResult(
            analysisId=str(uuid.uuid4()),
            inputs=inputs,
            generatedAt=_now_iso(),
            episodeStatistics=episode_statistics,
            measurementStatistics=measurement_statistics,
            setupSnapshot=snapshot,
            botSetupRecommendations=[],
            episodeSetupRecommendations=[],
            policyServerRecommendations=[],
            summary="튜닝 불가 결과 — 추천 생략.",
            generationMethod="fallback_rules",
            llmWarnings=warnings,
            nextBotSetup=None,
            nextEpisodeSetup=None,
            policySourceAnalysis=policy_source_analysis,
            paramConsistencyCheck=param_consistency_check,
        )
        _write_output(result, output_path)
        return result

    # 현재값 추출 (fallback rules에 넘기는 용)
    lidar = bot_setup.lidar
    drive = bot_setup.drive
    pf = bot_setup.path_follow
    run_cfg = episode_setup_dict.get("run", {}) or {}
    eval_cfg = episode_setup_dict.get("evaluation", {}) or {}
    near_miss_cfg = eval_cfg.get("near_miss", {}) or {}

    current_time_limit_s = float(run_cfg.get("time_limit_s", 60.0))
    current_goal_radius = float(eval_cfg.get("goal_acceptance_radius_m", 1.0))
    current_near_miss_dist = float(near_miss_cfg.get("distance_m", 0.5))
    current_tip_over = float(eval_cfg.get("tip_over_angle_deg", 60.0))
    current_ped_speed = _first_pedestrian_speed_mps(episode_setup_dict)

    bot_recs: list = []
    episode_recs: list = []
    policy_recs: list = []
    summary = ""
    generation_method: GenerationMethod

    def _run_fallback() -> tuple[list, list, list, str]:
        b = apply_episode_fallback_rules(
            episode_statistics,
            current_stop_distance_m=lidar.stop_distance_m,
            current_slow_down_distance_m=lidar.slow_down_distance_m,
            current_max_speed_kmh=drive.max_speed_kmh,
            current_target_speed_kmh=pf.target_speed_kmh,
            current_obstacle_slow_speed_kmh=pf.obstacle_slow_speed_kmh,
            current_look_ahead_distance_m=pf.look_ahead_distance_m,
        )
        e = apply_episode_setup_fallback_rules(
            episode_statistics,
            measurement_statistics,
            current_time_limit_s=current_time_limit_s,
            current_goal_acceptance_radius_m=current_goal_radius,
            current_near_miss_distance_m=current_near_miss_dist,
            current_tip_over_angle_deg=current_tip_over,
            current_pedestrian_speed_mps=current_ped_speed,
        )
        p = apply_policy_server_fallback_rules(
            episode_statistics,
            measurement_statistics,
            current_stop_distance_m_threshold=float(policy_defaults.get("stop_distance_m_threshold", 1.2)),
            current_slow_down_distance_m_threshold=float(policy_defaults.get("slow_down_distance_m_threshold", 3.5)),
            current_repath_grace_time_s=1.0,
            current_force_action_override=policy_defaults.get("force_action_override"),
        )
        s = " | ".join(filter(None, [
            build_episode_fallback_summary(b),
            build_episode_setup_fallback_summary(e),
            build_policy_server_fallback_summary(p),
        ]))
        return b, e, p, s

    if fallback_only:
        bot_recs, episode_recs, policy_recs, summary = _run_fallback()
        generation_method = "fallback_rules"
    else:
        bot_contexts = retrieve_policy_context(episode_statistics)
        episode_contexts = retrieve_episode_setup_context(episode_statistics, measurement_statistics)
        policy_contexts = retrieve_policy_server_context(episode_statistics, measurement_statistics)

        outcome = generate_integrated_recommendations(
            episode_statistics=episode_statistics,
            measurement_statistics=measurement_statistics,
            episode_setup=episode_setup_dict,
            bot_setup_snapshot=bot_setup_raw.get("robot", {}),
            policy_server_defaults=policy_defaults,
            bot_contexts=bot_contexts,
            episode_contexts=episode_contexts,
            policy_contexts=policy_contexts,
            provider=provider,
            settings=cfg,
            client=llm_client,
        )
        warnings.extend(outcome.warnings)

        if outcome.success:
            citations_pool = context_citation_ids_multi(bot_contexts, episode_contexts, policy_contexts)
            bot_recs = [
                rec if rec.citations else rec.model_copy(update={"citations": list(citations_pool)})
                for rec in outcome.bot_recommendations
            ]
            episode_recs = outcome.episode_recommendations
            policy_recs = outcome.policy_server_recommendations
            summary = outcome.summary
            generation_method = "llm_rag"
        else:
            bot_recs, episode_recs, policy_recs, summary = _run_fallback()
            generation_method = "fallback_after_llm_failure"

    # 3. nextBotSetup / nextEpisodeSetup 생성
    next_bot_setup = generate_bot_setup(bot_setup, bot_recs)
    next_episode_setup, patch_warnings = apply_episode_setup_recommendations(
        episode_setup_dict, episode_recs
    )
    warnings.extend(patch_warnings)

    result = IntegratedRecommendationResult(
        analysisId=str(uuid.uuid4()),
        inputs=inputs,
        generatedAt=_now_iso(),
        episodeStatistics=episode_statistics,
        measurementStatistics=measurement_statistics,
        setupSnapshot=snapshot,
        botSetupRecommendations=bot_recs,
        episodeSetupRecommendations=episode_recs,
        policyServerRecommendations=policy_recs,
        summary=summary,
        generationMethod=generation_method,
        llmWarnings=warnings,
        nextBotSetup=next_bot_setup.to_json_dict(),
        nextEpisodeSetup=next_episode_setup,
        policySourceAnalysis=policy_source_analysis,
        paramConsistencyCheck=param_consistency_check,
    )
    _write_output(result, output_path)
    return result


def _write_output(result: IntegratedRecommendationResult, output_path: str | Path | None) -> None:
    if output_path is None:
        return
    Path(output_path).write_text(
        result.model_dump_json(indent=2),
        encoding="utf-8",
    )
