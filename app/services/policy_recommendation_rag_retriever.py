from __future__ import annotations

from dataclasses import dataclass
from typing import Union

from app.models.rag import RagRetrievedChunk, RagSearchQuery
from app.models.recommendation import (
    AnalysisStatistics,
    EvaluationReportStatistics,
    PolicyParamName,
)
from app.services.policy_rag_retriever import search_policy_chunks


@dataclass(frozen=True)
class PolicyRagContext:
    param: PolicyParamName
    query: str
    chunks: list[RagRetrievedChunk]


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

        rag_query = RagSearchQuery(
            query=query_text,
            topK=topK,
            policyParamFilter=[param] if param != "canRepath" else None,
        )
        result = search_policy_chunks(rag_query)
        contexts.append(
            PolicyRagContext(
                param=param,
                query=query_text,
                chunks=result.results,
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
