from __future__ import annotations

import json
import uuid
from dataclasses import dataclass
from typing import Any

from app.core.settings import Settings
from app.models.llm import LlmGenerationRequest, LlmGenerationResponse, LlmProvider
from app.models.recommendation import (
    AnalysisStatistics,
    ParamRecommendation,
    PolicyParamName,
)
from app.services.json_output_extractor import JsonExtractionError, extract_json_object
from app.services.llm_client import BaseLlmClient
from app.services.llm_client_factory import create_llm_client
from app.services.policy_fallback_rules import PolicyParamDefaults
from app.services.policy_recommendation_rag_retriever import PolicyRagContext


ALLOWED_PARAMS: set[PolicyParamName] = {
    "stopDistanceM",
    "slowDownDistanceM",
    "maxSpeedKmh",
    "canRepath",
}


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
