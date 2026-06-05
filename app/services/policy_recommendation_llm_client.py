from __future__ import annotations

import json
import uuid
from dataclasses import dataclass
from typing import Any

from app.core.settings import Settings
from app.models.llm import LlmGenerationRequest, LlmGenerationResponse, LlmProvider
from app.models.recommendation import (
    AnalysisStatistics,
    BotSetupParamName,
    BotSetupRecommendation,
    EpisodeSetupParamName,
    EpisodeSetupRecommendation,
    EvaluationReportStatistics,
    ParamRecommendation,
    PolicyParamName,
    PolicyServerParamName,
    PolicyServerRecommendation,
)
from app.services.json_output_extractor import JsonExtractionError, extract_json_object
from app.services.llm_client import BaseLlmClient
from app.services.llm_client_factory import create_llm_client
from app.services.policy_fallback_rules import PolicyParamDefaults
from app.services.policy_recommendation_rag_retriever import (
    EpisodeSetupRagContext,
    PolicyRagContext,
    PolicyServerRagContext,
    context_citation_ids_multi,
)


ALLOWED_PARAMS: set[PolicyParamName] = {
    "stopDistanceM",
    "slowDownDistanceM",
    "maxSpeedKmh",
    "canRepath",
}

ALLOWED_BOT_PARAMS: set[BotSetupParamName] = {
    "stop_distance_m",
    "slow_down_distance_m",
    "max_speed_kmh",
    "target_speed_kmh",
    "obstacle_slow_speed_kmh",
    "look_ahead_distance_m",
    "scan_range_m",
    "angle_step_degree",
}

EPISODE_SYSTEM_PROMPT = """\
당신은 자율주행 배달 로봇의 DeliveryBotSetup 파라미터 최적화 전문가입니다.

입력으로 Episode 평가 결과(EpisodeEvaluationReport)와 관련 법령/표준/연구 문서 청크를 받습니다.
다음 DeliveryBotSetup 파라미터의 조정 추천을 JSON으로 생성하세요:

[lidar]
- stop_distance_m (단위: m, 허용 범위: 0.5 ~ 3.0)
- slow_down_distance_m (단위: m, stop_distance_m + 0.1 이상, 허용 범위: 0.6 ~ 6.0)
- scan_range_m (단위: m, 허용 범위: 2.0 ~ 10.0)
- angle_step_degree (단위: deg, 허용 범위: 1.0 ~ 10.0)

[drive]
- max_speed_kmh (단위: km/h, 허용 범위: 1.0 ~ 20.0)

[path_follow]
- target_speed_kmh (단위: km/h, 허용 범위: 1.0 ~ 20.0)
- obstacle_slow_speed_kmh (단위: km/h, 허용 범위: 0.5 ~ 5.0)
- look_ahead_distance_m (단위: m, 허용 범위: 0.3 ~ 5.0)

각 추천에는 반드시:
1. 평가 결과 수치(충돌/근접/종료원인/점수)를 인용
2. RAG 컨텍스트의 sourceIds(KOR-*, RSR-* 등)를 citations에 포함
3. 한국어 reason에 통계 근거와 문서 근거를 함께 명시

조정이 불필요한 파라미터는 추천에서 제외합니다.

응답은 다음 JSON 스키마를 그대로 따라야 합니다:
{
  "recommendations": [
    {
      "param": "stop_distance_m" | "slow_down_distance_m" | ...,
      "current": number,
      "suggested": number,
      "reason": "string (한국어)",
      "citations": ["KOR-001", ...]
    }
  ],
  "summary": "전체 추천 요약 (한국어)"
}

코드블록, markdown, 추가 설명 없이 JSON 객체만 반환하세요.
"""


SYSTEM_PROMPT = """\
당신은 자율주행 배달 로봇의 정책 파라미터 최적화 전문가입니다.

입력으로 주행 로그 통계와 관련 법령/표준/연구 문서 청크를 받습니다.
다음 정책 파라미터의 조정 추천을 JSON으로 생성하세요:
- stopDistanceM (단위: m, 허용 범위: 0.8 ~ 2.5)
- slowDownDistanceM (단위: m, 허용 범위: 1.5 ~ 6.0)
- maxSpeedKmh (단위: km/h, 허용 범위: 3.0 ~ 15.0)
- canRepath (boolean)

각 추천에는 반드시:
1. 현재 통계 수치를 인용
2. RAG 컨텍스트의 sourceIds(KOR-*, RSR-* 등)를 citations에 포함
3. 한국어 reason에 통계 근거와 문서 근거를 함께 명시

조정이 불필요한 파라미터는 추천에서 제외합니다.

응답은 다음 JSON 스키마를 그대로 따라야 합니다:
{
  "recommendations": [
    {
      "param": "stopDistanceM" | "slowDownDistanceM" | "maxSpeedKmh" | "canRepath",
      "current": number | boolean,
      "suggested": number | boolean,
      "reason": "string (한국어)",
      "citations": ["KOR-001", ...]
    }
  ],
  "summary": "전체 추천 요약 (한국어)"
}

코드블록, markdown, 추가 설명 없이 JSON 객체만 반환하세요.
"""


@dataclass(frozen=True)
class LlmRecommendationOutcome:
    success: bool
    recommendations: list[ParamRecommendation]
    summary: str
    warnings: list[str]
    provider: LlmProvider
    rawContent: str | None


def _statistics_block(statistics: AnalysisStatistics) -> str:
    reason_lines = "\n".join(
        f"  - {reason}: {count}회" for reason, count in statistics.actionReasonFreq.items()
    )
    return (
        "===== 주행 로그 통계 =====\n"
        f"총 tick 수: {statistics.totalTicks}\n"
        f"배송 시간: {statistics.deliveryTimeSec:.2f}s\n"
        f"종료 사유: {statistics.closeReason}\n"
        f"diagnostics warning: {statistics.diagnosticsWarningCount}건\n"
        f"평균 전방거리: {statistics.avgFrontDistanceM:.2f}m\n"
        f"최소 전방거리: {statistics.minFrontDistanceM:.2f}m\n"
        f"중앙값 전방거리: {statistics.medianFrontDistanceM:.2f}m\n"
        f"전방객체 감지율: {statistics.frontObjectDetectionRate:.1%}\n"
        f"브레이크 발동: {statistics.brakeAppliedCount}회 ({statistics.brakeAppliedRatio:.1%})\n"
        f"평균 targetSpeed: {statistics.avgTargetSpeedKmh:.2f}km/h\n"
        f"targetSpeed 분산: {statistics.targetSpeedVariance:.2f}\n"
        f"평균 실제 속도: {statistics.avgActualSpeedKmh:.2f}km/h\n"
        f"action reason 빈도:\n{reason_lines}\n"
        f"stop 비율: {statistics.stopActionRatio:.1%}\n"
        f"slowdown 비율: {statistics.slowdownActionRatio:.1%}\n"
        f"repath 비율: {statistics.repathActionRatio:.1%}\n"
    )


def _rag_block(contexts: list[PolicyRagContext]) -> str:
    if not contexts:
        return "===== 관련 법령/표준 컨텍스트 =====\n(검색 결과 없음)\n"
    blocks: list[str] = ["===== 관련 법령/표준 컨텍스트 ====="]
    for context in contexts:
        if not context.chunks:
            continue
        blocks.append(f"\n[대상 파라미터: {context.param}] 쿼리: {context.query}")
        for chunk in context.chunks:
            source_ids = ", ".join(chunk.metadata.sourceIds)
            related_params = ", ".join(chunk.metadata.relatedPolicyParams)
            blocks.append(
                f"- chunkId={chunk.chunkId} | sources=[{source_ids}] | "
                f"category={chunk.metadata.category} | params=[{related_params}]\n"
                f"  내용: {chunk.chunkText[:600]}"
            )
    return "\n".join(blocks) + "\n"


def _defaults_block(defaults: PolicyParamDefaults) -> str:
    return (
        "===== 현재 정책 파라미터 (1차 가정값) =====\n"
        f"stopDistanceM: {defaults.stopDistanceM}\n"
        f"slowDownDistanceM: {defaults.slowDownDistanceM}\n"
        f"maxSpeedKmh: {defaults.maxSpeedKmh}\n"
        f"canRepath: {defaults.canRepath}\n"
    )


def build_user_prompt(
    statistics: AnalysisStatistics,
    contexts: list[PolicyRagContext],
    defaults: PolicyParamDefaults,
) -> str:
    return "\n".join(
        [
            _statistics_block(statistics),
            _rag_block(contexts),
            _defaults_block(defaults),
            "위 데이터를 바탕으로 JSON 추천을 생성하세요.",
        ]
    )


def _validate_and_parse(
    payload: dict[str, Any],
    defaults: PolicyParamDefaults,
) -> tuple[list[ParamRecommendation], str, list[str]]:
    warnings: list[str] = []
    raw_recs = payload.get("recommendations")
    if not isinstance(raw_recs, list):
        raise ValueError("응답에 recommendations 리스트가 없음")

    parsed: list[ParamRecommendation] = []
    for raw in raw_recs:
        if not isinstance(raw, dict):
            warnings.append("recommendations 내 비-객체 항목 무시")
            continue
        param = raw.get("param")
        if param not in ALLOWED_PARAMS:
            warnings.append(f"허용되지 않은 param 무시: {param}")
            continue
        try:
            recommendation = ParamRecommendation.model_validate(raw)
        except Exception as exc:  # pydantic ValidationError 포함
            warnings.append(f"recommendation 검증 실패 무시: {exc}")
            continue
        parsed.append(recommendation)

    summary = str(payload.get("summary") or "")
    if not summary:
        summary = "LLM 추천 요약 없음"
        warnings.append("응답에 summary 누락")

    return parsed, summary, warnings


def generate_recommendations(
    statistics: AnalysisStatistics,
    contexts: list[PolicyRagContext],
    provider: LlmProvider,
    defaults: PolicyParamDefaults | None = None,
    settings: Settings | None = None,
    client: BaseLlmClient | None = None,
) -> LlmRecommendationOutcome:
    """LLM 호출 → JSON 추출 → ParamRecommendation 검증. 실패 시 success=False."""
    cfg = settings or Settings()
    param_defaults = defaults or PolicyParamDefaults()

    llm_client = client or create_llm_client(provider, cfg)

    if provider == LlmProvider.openai:
        max_tokens = cfg.openaiMaxTokens
        timeout_sec = cfg.openaiTimeoutSec
    else:
        max_tokens = cfg.ollamaMaxTokens
        timeout_sec = cfg.ollamaTimeoutSec

    user_prompt = build_user_prompt(statistics, contexts, param_defaults)
    request = LlmGenerationRequest(
        provider=provider,
        model="policy-recommendation",
        systemPrompt=SYSTEM_PROMPT,
        userPrompt=user_prompt,
        temperature=0.1,
        maxTokens=max_tokens,
        responseFormat="json_object",
        requestId=str(uuid.uuid4()),
        timeoutSec=timeout_sec,
    )

    response: LlmGenerationResponse = llm_client.generate(request)

    if not response.success or not response.content:
        message = response.error.message if response.error else "응답 없음"
        return LlmRecommendationOutcome(
            success=False,
            recommendations=[],
            summary="",
            warnings=[f"LLM 호출 실패: {message}"] + list(response.warnings),
            provider=provider,
            rawContent=response.rawContent,
        )

    try:
        payload = extract_json_object(response.content)
    except JsonExtractionError as exc:
        return LlmRecommendationOutcome(
            success=False,
            recommendations=[],
            summary="",
            warnings=[f"LLM 응답 JSON 파싱 실패: {exc.message}"] + list(response.warnings),
            provider=provider,
            rawContent=response.rawContent or response.content,
        )

    try:
        recommendations, summary, parse_warnings = _validate_and_parse(payload, param_defaults)
    except ValueError as exc:
        return LlmRecommendationOutcome(
            success=False,
            recommendations=[],
            summary="",
            warnings=[f"LLM 응답 스키마 위반: {exc}"] + list(response.warnings),
            provider=provider,
            rawContent=json.dumps(payload, ensure_ascii=False),
        )

    return LlmRecommendationOutcome(
        success=True,
        recommendations=recommendations,
        summary=summary,
        warnings=parse_warnings + list(response.warnings),
        provider=provider,
        rawContent=json.dumps(payload, ensure_ascii=False),
    )


# ── EpisodeEvaluationReport 기반 LLM 추천 ────────────────────────────────

@dataclass(frozen=True)
class EpisodeLlmOutcome:
    success: bool
    recommendations: list[BotSetupRecommendation]
    summary: str
    warnings: list[str]
    provider: LlmProvider
    rawContent: str | None


def _episode_statistics_block(statistics: EvaluationReportStatistics) -> str:
    return (
        "===== Episode 평가 결과 =====\n"
        f"outcome: {statistics.outcome}\n"
        f"terminal_reason: {statistics.terminal_reason}\n"
        f"duration_s: {statistics.duration_s:.2f}s\n"
        f"score: {statistics.score:.2f}\n"
        f"goal_reached: {statistics.goal_reached}\n"
        f"static_obstacle_collision_count: {statistics.static_obstacle_collision_count}\n"
        f"blocked_region_collision_count: {statistics.blocked_region_collision_count}\n"
        f"pedestrian_collision_count: {statistics.pedestrian_collision_count}\n"
        f"near_miss_count: {statistics.near_miss_count}\n"
        f"near_miss_min_distance_m: {statistics.near_miss_min_distance_m}\n"
        f"robot_tip_over_count: {statistics.robot_tip_over_count}\n"
        f"failure_type: {statistics.failure_type}\n"
        f"failure_speed_kmh: {statistics.failure_speed_kmh}\n"
        f"event_counts_by_type: {statistics.event_counts_by_type}\n"
    )


def _episode_bot_setup_block(
    stop_distance_m: float,
    slow_down_distance_m: float,
    max_speed_kmh: float,
    target_speed_kmh: float,
    obstacle_slow_speed_kmh: float,
    look_ahead_distance_m: float,
    scan_range_m: float,
    angle_step_degree: float,
) -> str:
    return (
        "===== 현재 DeliveryBotSetup 파라미터 =====\n"
        f"lidar.stop_distance_m: {stop_distance_m}\n"
        f"lidar.slow_down_distance_m: {slow_down_distance_m}\n"
        f"lidar.scan_range_m: {scan_range_m}\n"
        f"lidar.angle_step_degree: {angle_step_degree}\n"
        f"drive.max_speed_kmh: {max_speed_kmh}\n"
        f"path_follow.target_speed_kmh: {target_speed_kmh}\n"
        f"path_follow.obstacle_slow_speed_kmh: {obstacle_slow_speed_kmh}\n"
        f"path_follow.look_ahead_distance_m: {look_ahead_distance_m}\n"
    )


def _validate_and_parse_bot_setup(
    payload: dict[str, Any],
) -> tuple[list[BotSetupRecommendation], str, list[str]]:
    warnings: list[str] = []
    raw_recs = payload.get("recommendations")
    if not isinstance(raw_recs, list):
        raise ValueError("응답에 recommendations 리스트가 없음")

    parsed: list[BotSetupRecommendation] = []
    for raw in raw_recs:
        if not isinstance(raw, dict):
            warnings.append("recommendations 내 비-객체 항목 무시")
            continue
        param = raw.get("param")
        if param not in ALLOWED_BOT_PARAMS:
            warnings.append(f"허용되지 않은 param 무시: {param}")
            continue
        try:
            rec = BotSetupRecommendation.model_validate(raw)
        except Exception as exc:
            warnings.append(f"recommendation 검증 실패 무시: {exc}")
            continue
        parsed.append(rec)

    summary = str(payload.get("summary") or "")
    if not summary:
        summary = "LLM 추천 요약 없음"
        warnings.append("응답에 summary 누락")

    return parsed, summary, warnings


def generate_episode_recommendations(
    statistics: EvaluationReportStatistics,
    contexts: list[PolicyRagContext],
    provider: LlmProvider,
    stop_distance_m: float = 1.2,
    slow_down_distance_m: float = 3.5,
    max_speed_kmh: float = 10.0,
    target_speed_kmh: float = 10.0,
    obstacle_slow_speed_kmh: float = 1.5,
    look_ahead_distance_m: float = 1.0,
    scan_range_m: float = 5.0,
    angle_step_degree: float = 2.0,
    settings: Settings | None = None,
    client: BaseLlmClient | None = None,
) -> EpisodeLlmOutcome:
    cfg = settings or Settings()
    llm_client = client or create_llm_client(provider, cfg)

    if provider == LlmProvider.openai:
        max_tokens = cfg.openaiMaxTokens
        timeout_sec = cfg.openaiTimeoutSec
    else:
        max_tokens = cfg.ollamaMaxTokens
        timeout_sec = cfg.ollamaTimeoutSec

    user_prompt = "\n".join([
        _episode_statistics_block(statistics),
        _rag_block(contexts),
        _episode_bot_setup_block(
            stop_distance_m, slow_down_distance_m, max_speed_kmh,
            target_speed_kmh, obstacle_slow_speed_kmh, look_ahead_distance_m,
            scan_range_m, angle_step_degree,
        ),
        "위 데이터를 바탕으로 JSON 추천을 생성하세요.",
    ])

    request = LlmGenerationRequest(
        provider=provider,
        model="policy-recommendation",
        systemPrompt=EPISODE_SYSTEM_PROMPT,
        userPrompt=user_prompt,
        temperature=0.1,
        maxTokens=max_tokens,
        responseFormat="json_object",
        requestId=str(uuid.uuid4()),
        timeoutSec=timeout_sec,
    )

    response: LlmGenerationResponse = llm_client.generate(request)

    if not response.success or not response.content:
        message = response.error.message if response.error else "응답 없음"
        return EpisodeLlmOutcome(
            success=False,
            recommendations=[],
            summary="",
            warnings=[f"LLM 호출 실패: {message}"] + list(response.warnings),
            provider=provider,
            rawContent=response.rawContent,
        )

    try:
        payload = extract_json_object(response.content)
    except JsonExtractionError as exc:
        return EpisodeLlmOutcome(
            success=False,
            recommendations=[],
            summary="",
            warnings=[f"LLM 응답 JSON 파싱 실패: {exc.message}"] + list(response.warnings),
            provider=provider,
            rawContent=response.rawContent or response.content,
        )

    try:
        recommendations, summary, parse_warnings = _validate_and_parse_bot_setup(payload)
    except ValueError as exc:
        return EpisodeLlmOutcome(
            success=False,
            recommendations=[],
            summary="",
            warnings=[f"LLM 응답 스키마 위반: {exc}"] + list(response.warnings),
            provider=provider,
            rawContent=json.dumps(payload, ensure_ascii=False),
        )

    return EpisodeLlmOutcome(
        success=True,
        recommendations=recommendations,
        summary=summary,
        warnings=parse_warnings + list(response.warnings),
        provider=provider,
        rawContent=json.dumps(payload, ensure_ascii=False),
    )


# ── 통합 (5-입력) LLM 추천 ────────────────────────────────────────────────

ALLOWED_EPISODE_SETUP_PARAMS: set[EpisodeSetupParamName] = {
    "run.time_limit_s",
    "evaluation.goal_acceptance_radius_m",
    "evaluation.near_miss.distance_m",
    "evaluation.tip_over_angle_deg",
    "actors.pedestrians[*].movement.speed_mps",
    "actors.pedestrians[*].spawn_time_s",
    "actors.static_obstacles[*].xy_m",
}

ALLOWED_POLICY_SERVER_PARAMS: set[PolicyServerParamName] = {
    "stop_distance_m_threshold",
    "slow_down_distance_m_threshold",
    "repath_grace_time_s",
    "force_action_override",
}


INTEGRATED_SYSTEM_PROMPT = """\
당신은 자율주행 배달 로봇 시뮬레이션의 통합 튜닝 전문가입니다.

입력으로 한 회 Episode 실행 결과(EpisodeEvaluationReport 통계, MeasurementLog 시계열 통계),
현재 EpisodeSetup·DeliveryBotSetup·정책서버 설정, 관련 법령/표준/프로젝트 정책 카드를 받습니다.

다음 세 카테고리의 조정 추천을 단일 JSON으로 생성하세요:

[botSetupRecommendations] — DeliveryBotSetup 파라미터
- lidar.stop_distance_m (m, 0.5 ~ 3.0)
- lidar.slow_down_distance_m (m, stop_distance_m+0.1 이상, 0.6 ~ 6.0)
- lidar.scan_range_m (m, 2.0 ~ 10.0)
- lidar.angle_step_degree (deg, 1.0 ~ 10.0)
- drive.max_speed_kmh (km/h, 1.0 ~ 20.0)
- path_follow.target_speed_kmh (km/h, 1.0 ~ 20.0)
- path_follow.obstacle_slow_speed_kmh (km/h, 0.5 ~ 5.0)
- path_follow.look_ahead_distance_m (m, 0.3 ~ 5.0)
param 값은 위 키의 **마지막 한 단어**(예: stop_distance_m, max_speed_kmh)로 적습니다.

[episodeSetupRecommendations] — EpisodeSetup 환경/평가 파라미터
- run.time_limit_s (s, 10 ~ 600)
- evaluation.goal_acceptance_radius_m (m, 0.3 ~ 3.0)
- evaluation.near_miss.distance_m (m, 0.2 ~ 1.5)
- evaluation.tip_over_angle_deg (deg, 30 ~ 60, 60 초과 금지 — 안전 상한)
- actors.pedestrians[*].movement.speed_mps (m/s, 0.5 ~ 2.5)
- actors.pedestrians[*].spawn_time_s (s, 0 ~ 30)
- actors.static_obstacles[*].xy_m (좌표는 자유 텍스트 권고)
param 값은 위 dotted path 그대로 적습니다.

[policyServerRecommendations] — policy_server.py 임계값/로직
- stop_distance_m_threshold (m, LiDAR stop_distance_m과 동일하게)
- slow_down_distance_m_threshold (m, LiDAR slow_down_distance_m과 동일하게)
- repath_grace_time_s (s, 0.5 ~ 3.0)
- force_action_override (값은 "None" 문자열 또는 "Stop"/"SlowDown"/"Repath" — 운영 회차는 "None" 권장)
param 값은 위 키 그대로 적습니다.

[추천 포함 기준 — 반드시 준수]
추천은 통계에서 실제 문제가 확인된 파라미터에만 생성한다.
아래 조건을 만족하지 않으면 해당 파라미터는 추천에서 완전히 제외한다.

단, 다음 두 조건은 에피소드 통계와 무관하게 항상 추천한다:
- stop_distance_m_threshold / slow_down_distance_m_threshold: DeliveryBotSetup의 LiDAR 값(stop_distance_m, slow_down_distance_m)과 수치가 다를 경우 항상 PolicyServer 쪽을 LiDAR 값에 맞춰 동기화 추천한다.
- force_action_override: "None"이 아닌 값으로 설정되어 있으면 항상 "None"으로 되돌리는 추천을 생성한다.

- stop_distance_m / slow_down_distance_m (BotSetup lidar 값): 충돌 or 위험 근접(min_front_distance < stop_distance) 발생 시에만 추천
- max_speed_kmh / target_speed_kmh: 충돌·전복 발생 시 낮춘다 / 타임아웃·목표 미달 발생 시 높인다. 둘 다 없으면 추천 제외
- run.time_limit_s: terminal_reason=Timeout 발생 시에만 높인다 / 실제 소요시간이 제한시간의 30% 미만이면 낮춘다. 그 외 추천 제외
- evaluation.goal_acceptance_radius_m: goal_reached=false 또는 terminal_reason=Timeout 발생 시에만 높인다. 목표 도달 성공이면 추천 제외
- evaluation.near_miss.distance_m: near_miss_count > 0 또는 보행자 충돌 발생 시에만 높인다. 둘 다 없으면 추천 제외. 절대 낮추지 않는다
- evaluation.tip_over_angle_deg: robot_tip_over_count > 0 시에만 낮춘다. 60deg 초과 금지. 전복 없으면 추천 제외
- current == suggested인 추천은 생성하지 않는다 (변경 없는 추천 금지)

[파라미터 조정 방향 원칙 — 반드시 준수]
- evaluation.near_miss.distance_m: 안전 기준 임계값. 낮추는 것은 기준 완화이므로 절대 줄이지 않는다.
- evaluation.tip_over_angle_deg: 안전 상한(60deg). 절대 높이지 않는다.
- max_speed_kmh / target_speed_kmh: 효율이 낮다는 이유만으로 낮추지 않는다. 반드시 충돌·전복 통계가 있어야 한다.

각 추천에는 반드시:
1. 통계 수치(충돌/근접/종료원인/점수/시계열 통계)를 인용
2. RAG 컨텍스트의 sourceIds(KOR-*, PRJ-DOE, PRJ-EVAL, PRJ-AGENT)를 citations에 포함
3. 한국어 reason에 통계 근거와 문서 근거를 함께 명시

해당 카테고리의 RAG 컨텍스트가 비어 있으면 citations는 빈 배열로 둡니다(환각 금지).

응답은 다음 JSON 스키마를 그대로 따라야 합니다:
{
  "botSetupRecommendations": [{"param": "...", "current": number, "suggested": number, "reason": "...", "citations": [...]}],
  "episodeSetupRecommendations": [{"param": "run.time_limit_s" | ..., "current": ..., "suggested": ..., "reason": "...", "citations": [...]}],
  "policyServerRecommendations": [{"param": "stop_distance_m_threshold" | ..., "current": ..., "suggested": ..., "reason": "...", "citations": [...]}],
  "summary": "전체 추천 요약 (한국어)"
}

코드블록, markdown, 추가 설명 없이 JSON 객체만 반환하세요.
"""


@dataclass(frozen=True)
class IntegratedLlmOutcome:
    success: bool
    bot_recommendations: list[BotSetupRecommendation]
    episode_recommendations: list[EpisodeSetupRecommendation]
    policy_server_recommendations: list[PolicyServerRecommendation]
    summary: str
    warnings: list[str]
    provider: LlmProvider
    rawContent: str | None


def _measurement_statistics_block(stats: AnalysisStatistics) -> str:
    reason_lines = "\n".join(
        f"  - {reason}: {count}회" for reason, count in stats.actionReasonFreq.items()
    )
    return (
        "===== MeasurementLog 시계열 통계 =====\n"
        f"총 tick 수: {stats.totalTicks}\n"
        f"배송 시간: {stats.deliveryTimeSec:.2f}s\n"
        f"종료 사유(close reason): {stats.closeReason}\n"
        f"평균 전방거리: {stats.avgFrontDistanceM:.2f}m / 최소: {stats.minFrontDistanceM:.2f}m / 중앙값: {stats.medianFrontDistanceM:.2f}m\n"
        f"전방객체 감지율: {stats.frontObjectDetectionRate:.1%}\n"
        f"브레이크 발동: {stats.brakeAppliedCount}회 ({stats.brakeAppliedRatio:.1%})\n"
        f"평균 targetSpeed: {stats.avgTargetSpeedKmh:.2f}km/h / 분산: {stats.targetSpeedVariance:.2f}\n"
        f"평균 실제 속도: {stats.avgActualSpeedKmh:.2f}km/h\n"
        f"action reason 빈도:\n{reason_lines}\n"
        f"stop 비율: {stats.stopActionRatio:.1%} / slowdown 비율: {stats.slowdownActionRatio:.1%} / repath 비율: {stats.repathActionRatio:.1%}\n"
    )


def _episode_setup_block(episode_setup: dict[str, Any]) -> str:
    run = episode_setup.get("run", {}) or {}
    evaluation = episode_setup.get("evaluation", {}) or {}
    near_miss = evaluation.get("near_miss", {}) or {}
    actors = episode_setup.get("actors", {}) or {}
    peds = actors.get("pedestrians", []) or []
    obstacles = actors.get("static_obstacles", []) or []
    ped_speeds = [
        (p.get("movement") or {}).get("speed_mps") for p in peds if isinstance(p, dict)
    ]
    ped_speeds_str = ", ".join(f"{s:.2f}" for s in ped_speeds if s is not None) or "(없음)"
    return (
        "===== 현재 EpisodeSetup 요약 =====\n"
        f"scenario_id: {episode_setup.get('scenario_id', '?')}\n"
        f"run.time_limit_s: {run.get('time_limit_s', '?')}\n"
        f"evaluation.goal_acceptance_radius_m: {evaluation.get('goal_acceptance_radius_m', '?')}\n"
        f"evaluation.near_miss.distance_m: {near_miss.get('distance_m', '?')}\n"
        f"evaluation.tip_over_angle_deg: {evaluation.get('tip_over_angle_deg', '?')}\n"
        f"pedestrians.count: {len(peds)}\n"
        f"pedestrians.speed_mps: {ped_speeds_str}\n"
        f"static_obstacles.count: {len(obstacles)}\n"
    )


def _policy_server_block(defaults: dict[str, Any]) -> str:
    return (
        "===== 현재 정책서버(policy_server.py) 디폴트 =====\n"
        f"stop_distance_m_threshold: {defaults.get('stop_distance_m_threshold')}\n"
        f"slow_down_distance_m_threshold: {defaults.get('slow_down_distance_m_threshold')}\n"
        f"max_speed_kmh: {defaults.get('max_speed_kmh')}\n"
        f"force_action_override: {defaults.get('force_action_override')}\n"
    )


def _integrated_rag_block(
    bot_contexts: list[PolicyRagContext],
    episode_contexts: list[EpisodeSetupRagContext],
    policy_contexts: list[PolicyServerRagContext],
) -> str:
    blocks: list[str] = ["===== 관련 법령/표준/프로젝트 정책 컨텍스트 ====="]

    def _emit(title: str, contexts: list) -> None:
        if not contexts:
            blocks.append(f"\n[{title}] (검색 결과 없음)")
            return
        blocks.append(f"\n[{title}]")
        for ctx in contexts:
            if not ctx.chunks:
                continue
            blocks.append(f"  대상 파라미터: {ctx.param} / 쿼리: {ctx.query}")
            for chunk in ctx.chunks:
                source_ids = ", ".join(chunk.metadata.sourceIds)
                related_params = ", ".join(chunk.metadata.relatedPolicyParams)
                blocks.append(
                    f"  - chunkId={chunk.chunkId} | sources=[{source_ids}] | "
                    f"category={chunk.metadata.category} | params=[{related_params}]\n"
                    f"    내용: {chunk.chunkText[:600]}"
                )

    _emit("botSetup", bot_contexts)
    _emit("episodeSetup", episode_contexts)
    _emit("policyServer", policy_contexts)
    return "\n".join(blocks) + "\n"


def _validate_integrated_payload(
    payload: dict[str, Any],
    valid_source_ids: set[str] | None = None,
) -> tuple[
    list[BotSetupRecommendation],
    list[EpisodeSetupRecommendation],
    list[PolicyServerRecommendation],
    str,
    list[str],
]:
    warnings: list[str] = []

    BOT_PARAM_PREFIXES = ("lidar.", "drive.", "path_follow.")

    def _normalize_bot_param(raw: dict[str, Any]) -> None:
        # LLM이 종종 dotted prefix("lidar.stop_distance_m")로 응답 — 마지막 토큰만 사용.
        param = raw.get("param")
        if isinstance(param, str):
            for prefix in BOT_PARAM_PREFIXES:
                if param.startswith(prefix):
                    raw["param"] = param[len(prefix):]
                    break

    def _parse_list(
        key: str,
        allowed: set[str],
        model_cls,
        normalize=None,
    ) -> list:
        raw_list = payload.get(key)
        if raw_list is None:
            return []
        if not isinstance(raw_list, list):
            warnings.append(f"{key}가 list가 아님 — 빈 배열로 처리")
            return []
        parsed = []
        for raw in raw_list:
            if not isinstance(raw, dict):
                warnings.append(f"{key} 내 비-객체 항목 무시")
                continue
            if normalize is not None:
                normalize(raw)
            if raw.get("param") not in allowed:
                warnings.append(f"{key}: 허용되지 않은 param 무시: {raw.get('param')}")
                continue
            try:
                item = model_cls.model_validate(raw)
            except Exception as exc:
                warnings.append(f"{key} 검증 실패 무시: {exc}")
                continue
            # current == suggested인 추천 제거
            if hasattr(item, "current") and hasattr(item, "suggested") and item.current == item.suggested:
                warnings.append(f"{key}[{item.param}] current==suggested 무시 ({item.current})")
                continue
            # citations에 실제 RAG에 없는 sourceId가 있으면 제거
            if valid_source_ids is not None and item.citations:
                invalid = [c for c in item.citations if c not in valid_source_ids]
                if invalid:
                    warnings.append(
                        f"{key}[{item.param}] citations에서 근거 없는 sourceId 제거: {invalid}"
                    )
                    item = item.model_copy(
                        update={"citations": [c for c in item.citations if c in valid_source_ids]}
                    )
            parsed.append(item)
        return parsed

    bot_recs = _parse_list(
        "botSetupRecommendations", ALLOWED_BOT_PARAMS, BotSetupRecommendation,
        normalize=_normalize_bot_param,
    )
    episode_recs = _parse_list(
        "episodeSetupRecommendations",
        ALLOWED_EPISODE_SETUP_PARAMS,
        EpisodeSetupRecommendation,
    )
    policy_recs = _parse_list(
        "policyServerRecommendations",
        ALLOWED_POLICY_SERVER_PARAMS,
        PolicyServerRecommendation,
    )

    summary = str(payload.get("summary") or "")
    if not summary:
        summary = "LLM 추천 요약 없음"
        warnings.append("응답에 summary 누락")

    return bot_recs, episode_recs, policy_recs, summary, warnings


def generate_integrated_recommendations(
    *,
    episode_statistics: EvaluationReportStatistics,
    measurement_statistics: AnalysisStatistics,
    episode_setup: dict[str, Any],
    bot_setup_snapshot: dict[str, Any],
    policy_server_defaults: dict[str, Any],
    bot_contexts: list[PolicyRagContext],
    episode_contexts: list[EpisodeSetupRagContext],
    policy_contexts: list[PolicyServerRagContext],
    provider: LlmProvider,
    settings: Settings | None = None,
    client: BaseLlmClient | None = None,
) -> IntegratedLlmOutcome:
    """5개 입력을 받아 3종 통합 추천을 단일 JSON으로 생성."""
    cfg = settings or Settings()
    llm_client = client or create_llm_client(provider, cfg)

    if provider == LlmProvider.openai:
        max_tokens = cfg.openaiMaxTokens
        timeout_sec = cfg.openaiTimeoutSec
    else:
        max_tokens = cfg.ollamaMaxTokens
        timeout_sec = cfg.ollamaTimeoutSec

    user_prompt = "\n".join([
        _episode_statistics_block(episode_statistics),
        _measurement_statistics_block(measurement_statistics),
        _episode_setup_block(episode_setup),
        "===== 현재 DeliveryBotSetup 스냅샷 =====\n"
        + json.dumps(bot_setup_snapshot, ensure_ascii=False, indent=2),
        _policy_server_block(policy_server_defaults),
        _integrated_rag_block(bot_contexts, episode_contexts, policy_contexts),
        "위 데이터를 바탕으로 세 카테고리 추천 JSON을 생성하세요.",
    ])

    request = LlmGenerationRequest(
        provider=provider,
        model="integrated-recommendation",
        systemPrompt=INTEGRATED_SYSTEM_PROMPT,
        userPrompt=user_prompt,
        temperature=0.1,
        maxTokens=max_tokens,
        responseFormat="json_object",
        requestId=str(uuid.uuid4()),
        timeoutSec=timeout_sec,
    )

    response: LlmGenerationResponse = llm_client.generate(request)

    if not response.success or not response.content:
        message = response.error.message if response.error else "응답 없음"
        return IntegratedLlmOutcome(
            success=False,
            bot_recommendations=[],
            episode_recommendations=[],
            policy_server_recommendations=[],
            summary="",
            warnings=[f"LLM 호출 실패: {message}"] + list(response.warnings),
            provider=provider,
            rawContent=response.rawContent,
        )

    try:
        payload = extract_json_object(response.content)
    except JsonExtractionError as exc:
        return IntegratedLlmOutcome(
            success=False,
            bot_recommendations=[],
            episode_recommendations=[],
            policy_server_recommendations=[],
            summary="",
            warnings=[f"LLM 응답 JSON 파싱 실패: {exc.message}"] + list(response.warnings),
            provider=provider,
            rawContent=response.rawContent or response.content,
        )

    valid_source_ids = set(context_citation_ids_multi(bot_contexts, episode_contexts, policy_contexts))
    try:
        bot_recs, ep_recs, ps_recs, summary, parse_warnings = _validate_integrated_payload(
            payload, valid_source_ids=valid_source_ids
        )
    except ValueError as exc:
        return IntegratedLlmOutcome(
            success=False,
            bot_recommendations=[],
            episode_recommendations=[],
            policy_server_recommendations=[],
            summary="",
            warnings=[f"LLM 응답 스키마 위반: {exc}"] + list(response.warnings),
            provider=provider,
            rawContent=json.dumps(payload, ensure_ascii=False),
        )

    return IntegratedLlmOutcome(
        success=True,
        bot_recommendations=bot_recs,
        episode_recommendations=ep_recs,
        policy_server_recommendations=ps_recs,
        summary=summary,
        warnings=parse_warnings + list(response.warnings),
        provider=provider,
        rawContent=json.dumps(payload, ensure_ascii=False),
    )
