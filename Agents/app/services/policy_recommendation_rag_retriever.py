from __future__ import annotations

from dataclasses import dataclass
from typing import Union

from app.models.rag import RagChunkMetadata, RagRetrievedChunk
from app.models.recommendation import (
    AnalysisStatistics,
    EpisodeSetupParamName,
    EvaluationReportStatistics,
    PolicyParamName,
    PolicyServerParamName,
)
from app.services.pdf_rag_retriever import PdfRagEvidenceItem, get_pdf_rag_retriever, route_query


@dataclass(frozen=True)
class PolicyRagContext:
    param: PolicyParamName
    query: str
    chunks: list[RagRetrievedChunk]


@dataclass(frozen=True)
class EpisodeSetupRagContext:
    param: EpisodeSetupParamName
    query: str
    chunks: list[RagRetrievedChunk]


@dataclass(frozen=True)
class PolicyServerRagContext:
    param: PolicyServerParamName
    query: str
    chunks: list[RagRetrievedChunk]


# EpisodeSetup/PolicyServer 추천 대상 파라미터는 dotted path 그대로지만
# RAG 카드의 relatedPolicyParams에는 dotted가 아닌 짧은 키로 들어 있다.
# 통계 트리거 → (추천 param 이름, 카드 검색 키) 매핑을 위해 별도 테이블 유지.
_EPISODE_PARAM_CARD_KEY: dict[EpisodeSetupParamName, str] = {
    "run.time_limit_s": "time_limit_s",
    "evaluation.goal_acceptance_radius_m": "goal_acceptance_radius_m",
    "evaluation.near_miss.distance_m": "near_miss_distance_m",
    "evaluation.tip_over_angle_deg": "tip_over_angle_deg",
    "actors.pedestrians[*].movement.speed_mps": "pedestrian_speed_mps",
    "actors.pedestrians[*].spawn_time_s": "pedestrian_speed_mps",  # 보행자 카드 공유
    "actors.static_obstacles[*].xy_m": "static_obstacle_count",
}

_POLICY_SERVER_PARAM_CARD_KEY: dict[PolicyServerParamName, str] = {
    "stop_distance_m_threshold": "stop_distance_m_threshold",
    "slow_down_distance_m_threshold": "slow_down_distance_m_threshold",
    "repath_grace_time_s": "repath_grace_time_s",
    "force_action_override": "force_action_override",
}


# 메트릭 조건 → (쿼리 텍스트, 대상 파라미터) 매핑
def _build_queries(statistics: AnalysisStatistics) -> list[tuple[PolicyParamName, str]]:
    queries: list[tuple[PolicyParamName, str]] = []

    if 0 < statistics.minFrontDistanceM < 1.0:
        queries.append(("stopDistanceM", "위험한 정지거리 안전여유 충돌회피"))

    if statistics.brakeAppliedRatio > 0.25:
        queries.append(("slowDownDistanceM", "응급 정지 빈도 감속 시작 거리"))
        queries.append(("stopDistanceM", "비상 제동 안전 거리"))

    if statistics.repathActionRatio > 0.05:
        queries.append(("canRepath", "경로 재탐색 비용 효과"))

    if statistics.slowdownActionRatio > 0.6:
        queries.append(("slowDownDistanceM", "감속 의존도 효율"))

    if statistics.targetSpeedVariance > 4.0 or statistics.avgTargetSpeedKmh < 3.0:
        queries.append(("maxSpeedKmh", "최대 운행 속도 안정성"))

    if statistics.avgFrontDistanceM > 5.0:
        queries.append(("slowDownDistanceM", "과도한 보수성 효율"))

    if not queries:
        # 트리거 조건이 없어도 정책 표준 컨텍스트는 항상 제공
        queries.append(("stopDistanceM", "정지 거리 정책"))
        queries.append(("maxSpeedKmh", "최대 속도 정책"))

    return queries


def _build_episode_queries(
    statistics: EvaluationReportStatistics,
) -> list[tuple[PolicyParamName, str]]:
    queries: list[tuple[PolicyParamName, str]] = []

    if statistics.near_miss_min_distance_m is not None and statistics.near_miss_min_distance_m < 0.5:
        queries.append(("stopDistanceM", "위험한 정지거리 안전여유 충돌회피"))

    if statistics.pedestrian_collision_count > 0:
        queries.append(("stopDistanceM", "보행자 충돌 정지 거리 안전"))

    if statistics.static_obstacle_collision_count > 0:
        queries.append(("slowDownDistanceM", "장애물 감속 시작 거리 충돌 방지"))

    if statistics.terminal_reason == "Timeout":
        queries.append(("maxSpeedKmh", "배송 시간 초과 속도 정책"))

    if statistics.robot_tip_over_count > 0:
        queries.append(("maxSpeedKmh", "로봇 전복 최대 속도 안전"))

    if statistics.terminal_reason in ("Stuck", "PathFindingFailed") or (
        statistics.failure_type in ("Stuck", "PathFindingFailed")
    ):
        queries.append(("slowDownDistanceM", "장애물 회피 감속 경로 추종"))

    if not queries:
        queries.append(("stopDistanceM", "정지 거리 정책"))
        queries.append(("maxSpeedKmh", "최대 속도 정책"))

    return queries


def retrieve_policy_context(
    statistics: Union[AnalysisStatistics, EvaluationReportStatistics],
    topK: int = 3,
) -> list[PolicyRagContext]:
    contexts: list[PolicyRagContext] = []
    seen: set[tuple[PolicyParamName, str]] = set()
    if isinstance(statistics, EvaluationReportStatistics):
        query_pairs = _build_episode_queries(statistics)
    else:
        query_pairs = _build_queries(statistics)
    for param, query_text in query_pairs:
        key = (param, query_text)
        if key in seen:
            continue
        seen.add(key)

        contexts.append(
            PolicyRagContext(
                param=param,
                query=query_text,
                chunks=_search_chunks_for_param(str(param), query_text, topK),
            )
        )
    return contexts


def context_citation_ids(contexts: list[PolicyRagContext]) -> list[str]:
    ids: list[str] = []
    for context in contexts:
        for chunk in context.chunks:
            for source_id in chunk.metadata.sourceIds:
                if source_id not in ids:
                    ids.append(source_id)
    return ids


def _build_episode_setup_queries(
    episode_stats: EvaluationReportStatistics,
    measurement_stats: AnalysisStatistics | None,
) -> list[tuple[EpisodeSetupParamName, str]]:
    queries: list[tuple[EpisodeSetupParamName, str]] = []

    if episode_stats.pedestrian_collision_count > 0 or (
        episode_stats.near_miss_min_distance_m is not None
        and episode_stats.near_miss_min_distance_m < 0.5
    ):
        queries.append((
            "actors.pedestrians[*].movement.speed_mps",
            "보행자 속도 보행자 충돌 안전 여유",
        ))
        queries.append((
            "evaluation.near_miss.distance_m",
            "근접 거리 평가 임계값 안전",
        ))

    # near_miss 기록이 있거나 근접 데이터가 존재하면 항상 전용 쿼리 추가
    if episode_stats.near_miss_count > 0 or episode_stats.near_miss_min_distance_m is not None:
        already = any(p == "evaluation.near_miss.distance_m" for p, _ in queries)
        if not already:
            queries.append((
                "evaluation.near_miss.distance_m",
                "near_miss 거리 평가 임계값 안전 기준",
            ))

    if episode_stats.terminal_reason == "Timeout" or (
        measurement_stats is not None and not episode_stats.goal_reached
    ):
        queries.append((
            "run.time_limit_s",
            "시간 한도 산정 경로 길이 평균 속도",
        ))

    if episode_stats.robot_tip_over_count > 0:
        queries.append((
            "evaluation.tip_over_angle_deg",
            "전복 각도 안전 한계",
        ))

    if episode_stats.static_obstacle_collision_count > 0:
        queries.append((
            "actors.static_obstacles[*].xy_m",
            "정적 장애물 배치 시나리오 난이도",
        ))

    if not episode_stats.goal_reached and not queries:
        queries.append((
            "evaluation.goal_acceptance_radius_m",
            "목표 도달 반경 평가 기준",
        ))

    if not queries:
        # 기본 시나리오 난이도 카드라도 한 번 가져온다
        queries.append((
            "actors.pedestrians[*].movement.speed_mps",
            "시나리오 난이도 보행자 속도",
        ))

    return queries


def _build_policy_server_queries(
    episode_stats: EvaluationReportStatistics,
    measurement_stats: AnalysisStatistics | None,
) -> list[tuple[PolicyServerParamName, str]]:
    queries: list[tuple[PolicyServerParamName, str]] = []

    if measurement_stats is not None and measurement_stats.repathActionRatio > 0.1:
        queries.append((
            "repath_grace_time_s",
            "재경로 grace 시간 정책서버 로직",
        ))
        queries.append((
            "force_action_override",
            "FORCED_ACTION 디버깅 강제 행동",
        ))

    if episode_stats.static_obstacle_collision_count > 0 or (
        measurement_stats is not None and measurement_stats.brakeAppliedRatio > 0.3
    ):
        queries.append((
            "stop_distance_m_threshold",
            "정지 거리 임계값 정책서버 LiDAR 정합",
        ))

    if measurement_stats is not None and measurement_stats.slowdownActionRatio > 0.5:
        queries.append((
            "slow_down_distance_m_threshold",
            "감속 거리 임계값 정책서버 LiDAR 정합",
        ))

    if not queries:
        queries.append((
            "stop_distance_m_threshold",
            "정책서버 임계값 LiDAR 정합 기본",
        ))

    return queries


def _search_chunks_for_param(card_key: str, query_text: str, topK: int) -> list[RagRetrievedChunk]:
    _ = card_key
    result = get_pdf_rag_retriever().retrieve(query_text, route_hint=route_query(query_text))
    if not result.available:
        return []
    return [
        _evidence_to_rag_chunk(index=index, evidence=evidence)
        for index, evidence in enumerate(result.context_pack.evidence_items[:topK], start=1)
    ]


def _evidence_to_rag_chunk(*, index: int, evidence: PdfRagEvidenceItem) -> RagRetrievedChunk:
    """Adapt internal PDF RAG evidence to the legacy in-process context model."""
    context_id = f"PDF-RAG-{index:03d}"
    return RagRetrievedChunk(
        chunkId=context_id,
        cardId=context_id,
        chunkText=evidence.evidence_text,
        metadata=RagChunkMetadata(
            sourceIds=[],
            category=evidence.topic_tags[0] if evidence.topic_tags else "pdf_rag_context",
            relatedPolicyParams=[],
            relatedRequestFields=[],
            relatedActions=[],
            relatedMetrics=[],
            evidenceLocation=evidence.section_title,
            createdFromCandidateId="",
            status="auto_validated_pdf_rag",
        ),
        score=evidence.score,
        matchedFields=["pdf_vector_hybrid"],
    )


def retrieve_episode_setup_context(
    episode_stats: EvaluationReportStatistics,
    measurement_stats: AnalysisStatistics | None = None,
    topK: int = 3,
) -> list[EpisodeSetupRagContext]:
    contexts: list[EpisodeSetupRagContext] = []
    seen: set[tuple[EpisodeSetupParamName, str]] = set()
    for param, query_text in _build_episode_setup_queries(episode_stats, measurement_stats):
        key = (param, query_text)
        if key in seen:
            continue
        seen.add(key)
        card_key = _EPISODE_PARAM_CARD_KEY[param]
        chunks = _search_chunks_for_param(card_key, query_text, topK)
        contexts.append(EpisodeSetupRagContext(param=param, query=query_text, chunks=chunks))
    return contexts


def retrieve_policy_server_context(
    episode_stats: EvaluationReportStatistics,
    measurement_stats: AnalysisStatistics | None = None,
    topK: int = 3,
) -> list[PolicyServerRagContext]:
    contexts: list[PolicyServerRagContext] = []
    seen: set[tuple[PolicyServerParamName, str]] = set()
    for param, query_text in _build_policy_server_queries(episode_stats, measurement_stats):
        key = (param, query_text)
        if key in seen:
            continue
        seen.add(key)
        card_key = _POLICY_SERVER_PARAM_CARD_KEY[param]
        chunks = _search_chunks_for_param(card_key, query_text, topK)
        contexts.append(PolicyServerRagContext(param=param, query=query_text, chunks=chunks))
    return contexts


def context_citation_ids_multi(
    *context_lists: list[PolicyRagContext]
    | list[EpisodeSetupRagContext]
    | list[PolicyServerRagContext],
) -> list[str]:
    ids: list[str] = []
    for contexts in context_lists:
        for context in contexts:
            for chunk in context.chunks:
                for source_id in chunk.metadata.sourceIds:
                    if source_id not in ids:
                        ids.append(source_id)
    return ids
