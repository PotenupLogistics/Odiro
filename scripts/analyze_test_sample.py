"""
test_sample 5개 파일을 입력받아 OpenAI LLM으로 분석·추천 결과를 생성한다.
LLM 실패 시 규칙 기반 fallback으로 자동 전환.

입력 (test_sample/):
  - DeliveryBotSetupSample_1.json   로봇 파라미터
  - EpisodeSetupSample_1.json       환경 설정
  - *evaluation_report.json         에피소드 결과
  - MeasurementLog_*.jsonl          틱별 주행 로그 (footer 없어도 허용)
  - policy_server.py                행동 결정 로직 소스

출력 (test_sample/test/):
  - analysis_result.json
"""

from __future__ import annotations

import ast
import json
import re
import sys
import uuid
from datetime import datetime, timezone
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT))

from app.core.settings import Settings
from app.models.bot_setup import DeliveryBotSetup
from app.models.llm import LlmProvider
from app.models.recommendation import (
    EvaluationReportStatistics,
)
from app.services.bot_setup_generator import generate_bot_setup
from app.services.episode_setup_patcher import apply_episode_setup_recommendations
from app.services.evaluation_report_parser import parse_evaluation_report
from app.services.evaluation_report_statistics_extractor import (
    extract_statistics as extract_episode_stats,
)
from app.services.measurement_log_parser import MeasurementLogData
from app.services.metrics_extractor import extract_statistics as extract_measurement_stats
from app.services.policy_fallback_rules import (
    apply_episode_fallback_rules,
    apply_episode_setup_fallback_rules,
    apply_policy_server_fallback_rules,
    build_episode_fallback_summary,
    build_episode_setup_fallback_summary,
    build_policy_server_fallback_summary,
)
from app.services.policy_recommendation_llm_client import generate_integrated_recommendations
from app.services.policy_recommendation_rag_retriever import (
    context_citation_ids_multi,
    retrieve_episode_setup_context,
    retrieve_policy_context,
    retrieve_policy_server_context,
)
from app.services.policy_server_inspector import extract_policy_defaults_from_source

SAMPLE_DIR = ROOT / "test_sample"
OUT_DIR = SAMPLE_DIR / "test"


def _now_iso() -> str:
    return datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")


# ── footer 없는 JSONL도 허용하는 파서 ──────────────────────────────────────────

def _parse_measurement_log_lenient(path: Path) -> MeasurementLogData:
    records: list[dict] = []
    for line in path.read_text(encoding="utf-8-sig").splitlines():
        stripped = line.strip()
        if not stripped:
            continue
        try:
            records.append(json.loads(stripped))
        except json.JSONDecodeError:
            continue
    header = next((r for r in records if r.get("type") == "header"), {})
    footer = next((r for r in reversed(records) if r.get("type") == "footer"), {})
    ticks = [r for r in records if r.get("type") == "tick"]
    return MeasurementLogData(header=header, ticks=ticks, footer=footer, sourcePath=str(path))


# ── BotSetup / EpisodeSetup 로더 ───────────────────────────────────────────────

def _load_bot_setup(path: Path) -> tuple[DeliveryBotSetup, dict]:
    data = json.loads(path.read_text(encoding="utf-8-sig"))
    robot = data.get("robot", {}) or {}
    setup = DeliveryBotSetup(
        drive=robot.get("drive", {}) or {},
        path_follow=robot.get("path_follow", {}) or {},
        lidar=robot.get("lidar", {}) or {},
    )
    return setup, data


def _load_episode_setup(path: Path) -> dict:
    return json.loads(path.read_text(encoding="utf-8-sig"))


# ── policy_server.py 소스 분석 (FORCED_ACTION 감지) ───────────────────────────

def _parse_policy_server_source(path: Path) -> dict:
    src = path.read_text(encoding="utf-8")
    forced_action = None
    try:
        tree = ast.parse(src)
        for node in ast.walk(tree):
            if isinstance(node, ast.Assign):
                for t in node.targets:
                    if isinstance(t, ast.Name) and t.id == "FORCED_ACTION":
                        v = node.value
                        forced_action = v.value if isinstance(v, ast.Constant) else None
    except SyntaxError:
        pass

    stop_m = re.search(r'stop_distance_m.*?get\([^,]+,\s*([\d.]+)\)', src)
    slow_m = re.search(r'slow_down_distance_m.*?get\([^,]+,\s*([\d.]+)\)', src)
    default_stop = float(stop_m.group(1)) if stop_m else 1.2
    default_slow = float(slow_m.group(1)) if slow_m else 3.5

    warning = None
    if forced_action is not None:
        warning = (
            f"FORCED_ACTION='{forced_action}' 활성화 — 실제 거리 기반 로직이 무시됩니다. "
            "운영 환경에서는 None으로 설정하세요."
        )

    return {
        "forced_action": forced_action,
        "default_stop_distance_m": default_stop,
        "default_slow_down_distance_m": default_slow,
        "logic_summary": [
            "hasFrontObject=False → None",
            "inRepathMoveGraceTime=True → SlowDown",
            f"dist ≤ {default_stop}m + canRepath=True → Repath",
            f"dist ≤ {default_stop}m + canRepath=False → Stop",
            f"dist ≤ {default_slow}m → SlowDown",
            "그 외 → None",
        ],
        "warning": warning,
    }


# ── BotSetup ↔ PolicyServer 파라미터 정합성 검사 ──────────────────────────────

def _check_param_consistency(bot_setup: dict, episode_setup: dict, policy_source: dict) -> dict:
    lidar = bot_setup.get("robot", {}).get("lidar", {})
    issues = []

    for param, bot_key, ps_key in [
        ("stop_distance_m", "stop_distance_m", "default_stop_distance_m"),
        ("slow_down_distance_m", "slow_down_distance_m", "default_slow_down_distance_m"),
    ]:
        bot_val = lidar.get(bot_key)
        ps_val = policy_source.get(ps_key)
        if bot_val is not None and ps_val is not None and abs(bot_val - ps_val) > 0.01:
            issues.append({
                "param": param,
                "bot_value": bot_val,
                "policy_server_value": ps_val,
                "gap": round(bot_val - ps_val, 3),
                "description": f"BotSetup({bot_val}m) ≠ PolicyServer 기본값({ps_val}m)",
            })

    near_miss_thresh = episode_setup.get("evaluation", {}).get("near_miss", {}).get("distance_m")
    return {
        "ok": len(issues) == 0,
        "near_miss_threshold_m": near_miss_thresh,
        "issues": issues,
    }


# ── fallback 실행 헬퍼 ─────────────────────────────────────────────────────────

def _run_fallback(episode_stats: EvaluationReportStatistics, measurement_stats, bot_setup: DeliveryBotSetup, episode_setup: dict, policy_defaults: dict):
    lidar = bot_setup.lidar
    drive = bot_setup.drive
    pf = bot_setup.path_follow
    run_cfg = episode_setup.get("run", {}) or {}
    eval_cfg = episode_setup.get("evaluation", {}) or {}
    near_miss_cfg = eval_cfg.get("near_miss", {}) or {}
    actors = episode_setup.get("actors", {}) or {}
    peds = actors.get("pedestrians", []) or []
    ped_speed = None
    for p in peds:
        mv = (p.get("movement") or {})
        s = mv.get("speed_mps")
        if isinstance(s, (int, float)):
            ped_speed = float(s)
            break

    bot_recs = apply_episode_fallback_rules(
        episode_stats,
        current_stop_distance_m=lidar.stop_distance_m,
        current_slow_down_distance_m=lidar.slow_down_distance_m,
        current_max_speed_kmh=drive.max_speed_kmh,
        current_target_speed_kmh=pf.target_speed_kmh,
        current_obstacle_slow_speed_kmh=pf.obstacle_slow_speed_kmh,
        current_look_ahead_distance_m=pf.look_ahead_distance_m,
    )
    episode_recs = apply_episode_setup_fallback_rules(
        episode_stats,
        measurement_stats,
        current_time_limit_s=float(run_cfg.get("time_limit_s", 60.0)),
        current_goal_acceptance_radius_m=float(eval_cfg.get("goal_acceptance_radius_m", 1.0)),
        current_near_miss_distance_m=float(near_miss_cfg.get("distance_m", 0.5)),
        current_tip_over_angle_deg=float(eval_cfg.get("tip_over_angle_deg", 60.0)),
        current_pedestrian_speed_mps=ped_speed,
    )
    policy_recs = apply_policy_server_fallback_rules(
        episode_stats,
        measurement_stats,
        current_stop_distance_m_threshold=float(policy_defaults.get("stop_distance_m_threshold", 1.2)),
        current_slow_down_distance_m_threshold=float(policy_defaults.get("slow_down_distance_m_threshold", 3.5)),
        current_repath_grace_time_s=1.0,
        current_force_action_override=policy_defaults.get("force_action_override"),
    )
    summary = " | ".join(filter(None, [
        build_episode_fallback_summary(bot_recs),
        build_episode_setup_fallback_summary(episode_recs),
        build_policy_server_fallback_summary(policy_recs),
    ]))
    return bot_recs, episode_recs, policy_recs, summary


# ── 메인 ──────────────────────────────────────────────────────────────────────

def main() -> None:
    # 파일 자동 탐지
    log_paths = list(SAMPLE_DIR.glob("MeasurementLog_*.jsonl"))
    report_paths = list(SAMPLE_DIR.glob("*evaluation_report.json"))
    if not log_paths:
        print("오류: MeasurementLog_*.jsonl 없음")
        sys.exit(1)
    if not report_paths:
        print("오류: *evaluation_report.json 없음")
        sys.exit(1)

    bot_setup_path = SAMPLE_DIR / "DeliveryBotSetupSample_1.json"
    episode_setup_path = SAMPLE_DIR / "EpisodeSetupSample_1.json"
    policy_server_path = SAMPLE_DIR / "policy_server.py"
    log_path = log_paths[0]
    report_path = report_paths[0]

    cfg = Settings()

    # 1. 파일 로드
    print("[1/7] 파일 로드...")
    bot_setup, bot_setup_raw = _load_bot_setup(bot_setup_path)
    episode_setup_dict = _load_episode_setup(episode_setup_path)
    policy_source = _parse_policy_server_source(policy_server_path)
    policy_defaults = extract_policy_defaults_from_source(policy_server_path)
    param_consistency = _check_param_consistency(bot_setup_raw, episode_setup_dict, policy_source)

    # 2. 파싱
    print(f"[2/7] 측정 로그 파싱: {log_path.name}")
    log_data = _parse_measurement_log_lenient(log_path)
    measurement_stats = extract_measurement_stats(log_data)

    print(f"[3/7] 에피소드 리포트 파싱: {report_path.name}")
    report = parse_evaluation_report(report_path)
    episode_stats = extract_episode_stats(report)

    # 3. usability 검사
    warnings: list[str] = []
    if not episode_stats.usable_for_llm_tuning:
        warnings.append("usable_for_llm_tuning=False — LLM 튜닝 근거로 사용 불가.")

    # 4. RAG 컨텍스트 수집
    print("[4/7] RAG 컨텍스트 수집...")
    bot_contexts = retrieve_policy_context(episode_stats)
    episode_contexts = retrieve_episode_setup_context(episode_stats, measurement_stats)
    policy_contexts = retrieve_policy_server_context(episode_stats, measurement_stats)

    bot_recs = []
    episode_recs = []
    policy_recs = []
    summary = ""
    generation_method: str

    if not episode_stats.usable_for_llm_tuning:
        print("[5/7] 튜닝 불가 — fallback 규칙 실행...")
        bot_recs, episode_recs, policy_recs, summary = _run_fallback(
            episode_stats, measurement_stats, bot_setup, episode_setup_dict, policy_defaults
        )
        generation_method = "fallback_rules"
    else:
        # 5. LLM 호출
        print("[5/7] OpenAI LLM 호출...")
        outcome = generate_integrated_recommendations(
            episode_statistics=episode_stats,
            measurement_statistics=measurement_stats,
            episode_setup=episode_setup_dict,
            bot_setup_snapshot=bot_setup_raw.get("robot", {}),
            policy_server_defaults=policy_defaults,
            bot_contexts=bot_contexts,
            episode_contexts=episode_contexts,
            policy_contexts=policy_contexts,
            provider=LlmProvider.openai,
            settings=cfg,
        )
        warnings.extend(outcome.warnings)

        if outcome.success:
            citations_pool = context_citation_ids_multi(bot_contexts, episode_contexts, policy_contexts)
            bot_recs = [
                r if r.citations else r.model_copy(update={"citations": list(citations_pool)})
                for r in outcome.bot_recommendations
            ]
            episode_recs = outcome.episode_recommendations
            policy_recs = outcome.policy_server_recommendations
            summary = outcome.summary
            generation_method = "llm_rag"
            print(f"  → LLM 성공 (bot:{len(bot_recs)} episode:{len(episode_recs)} policy:{len(policy_recs)})")
        else:
            print("  → LLM 실패, fallback 규칙으로 전환...")
            bot_recs, episode_recs, policy_recs, summary = _run_fallback(
                episode_stats, measurement_stats, bot_setup, episode_setup_dict, policy_defaults
            )
            generation_method = "fallback_after_llm_failure"

    # 6. 다음 Setup 생성
    print("[6/7] 다음 Setup 생성...")
    next_bot_setup = generate_bot_setup(bot_setup, bot_recs)
    next_episode_setup, patch_warnings = apply_episode_setup_recommendations(
        episode_setup_dict, episode_recs
    )
    warnings.extend(patch_warnings)

    # 7. 결과 저장
    print("[7/7] 결과 저장...")
    OUT_DIR.mkdir(parents=True, exist_ok=True)

    result = {
        "schema": "test_sample_analysis_result",
        "version": 2,
        "analysisId": str(uuid.uuid4()),
        "generatedAt": _now_iso(),
        "generationMethod": generation_method,
        "inputs": {
            "bot_setup": str(bot_setup_path.relative_to(ROOT)),
            "episode_setup": str(episode_setup_path.relative_to(ROOT)),
            "measurement_log": str(log_path.relative_to(ROOT)),
            "evaluation_report": str(report_path.relative_to(ROOT)),
            "policy_server_source": str(policy_server_path.relative_to(ROOT)),
        },
        "policy_source_analysis": policy_source,
        "param_consistency_check": param_consistency,
        "episodeStatistics": episode_stats.model_dump(),
        "measurementStatistics": measurement_stats.model_dump(),
        "botSetupRecommendations": [r.model_dump() for r in bot_recs],
        "episodeSetupRecommendations": [r.model_dump() for r in episode_recs],
        "policyServerRecommendations": [r.model_dump() for r in policy_recs],
        "summary": summary,
        "llmWarnings": warnings,
        "nextBotSetup": next_bot_setup.to_json_dict(),
        "nextEpisodeSetup": next_episode_setup,
    }

    out_path = OUT_DIR / "analysis_result.json"
    out_path.write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding="utf-8")

    print(f"\n완료: {out_path}")
    print(f"generationMethod: {generation_method}")
    print(f"추천 — bot:{len(bot_recs)} episode:{len(episode_recs)} policy:{len(policy_recs)}")
    print(f"요약: {summary}")


if __name__ == "__main__":
    main()
