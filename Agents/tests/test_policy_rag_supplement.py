"""신규 RAG 카드 보강과 EpisodeSetup·PolicyServer 컨텍스트 빌더 테스트."""
from __future__ import annotations

from app.models.rag import RagChunkMetadata, RagSearchQuery
from app.models.recommendation import AnalysisStatistics, EvaluationReportStatistics
from app.services.policy_rag_retriever import load_chunks, search_policy_chunks
from app.services.policy_recommendation_rag_retriever import (
    context_citation_ids_multi,
    retrieve_episode_setup_context,
    retrieve_policy_context,
    retrieve_policy_server_context,
)


NEW_CARD_IDS = {
    "CARD-PRJ-DOE-pedestrian_behavior-001",
    "CARD-PRJ-EVAL-episode_evaluation_threshold-001",
    "CARD-PRJ-DOE-episode_time_budget-001",
    "CARD-PRJ-DOE-scenario_difficulty-001",
    "CARD-PRJ-AGENT-policy_server_logic-001",
    "CARD-PRJ-AGENT-policy_server_threshold-001",
}


def _episode_stats(
    *,
    outcome: str = "Failure",
    terminal_reason: str = "Timeout",
    pedestrian_collision: int = 0,
    static_collision: int = 0,
    near_miss_min: float | None = None,
    tip_over: int = 0,
    goal_reached: bool = False,
    score: float = -10.0,
) -> EvaluationReportStatistics:
    return EvaluationReportStatistics(
        outcome=outcome,
        terminal_reason=terminal_reason,
        duration_s=60.0,
        score=score,
        goal_reached=goal_reached,
        static_obstacle_collision_count=static_collision,
        pedestrian_collision_count=pedestrian_collision,
        near_miss_count=0,
        near_miss_min_distance_m=near_miss_min,
        robot_tip_over_count=tip_over,
    )


def _measurement_stats(
    *,
    brake_ratio: float = 0.1,
    slow_ratio: float = 0.2,
    repath_ratio: float = 0.02,
) -> AnalysisStatistics:
    return AnalysisStatistics(
        totalTicks=100,
        deliveryTimeSec=20.0,
        closeReason="Goal",
        diagnosticsWarningCount=0,
        avgFrontDistanceM=2.0,
        minFrontDistanceM=1.0,
        medianFrontDistanceM=1.8,
        frontObjectDetectionRate=0.5,
        actionReasonFreq={"none": 50},
        brakeAppliedCount=int(brake_ratio * 100),
        brakeAppliedRatio=brake_ratio,
        avgTargetSpeedKmh=5.0,
        targetSpeedVariance=1.0,
        avgActualSpeedKmh=4.5,
        stopActionRatio=0.05,
        slowdownActionRatio=slow_ratio,
        repathActionRatio=repath_ratio,
    )


# ── 카드 로딩/스키마 ────────────────────────────────────────────────────────


def test_jsonl_loads_17_chunks() -> None:
    chunks = load_chunks()
    assert len(chunks) == 17
    card_ids = {c["cardId"] for c in chunks}
    assert NEW_CARD_IDS.issubset(card_ids)


def test_jsonl_metadata_schema_valid() -> None:
    chunks = load_chunks()
    new_chunks = [c for c in chunks if c["cardId"] in NEW_CARD_IDS]
    assert len(new_chunks) == 6
    for c in new_chunks:
        RagChunkMetadata.model_validate(c["metadata"])


# ── policyParamFilter 기반 직접 검색 ────────────────────────────────────────


def test_search_by_pedestrian_speed_param() -> None:
    result = search_policy_chunks(
        RagSearchQuery(query="", topK=5, policyParamFilter=["pedestrian_speed_mps"])
    )
    card_ids = {r.cardId for r in result.results}
    assert "CARD-PRJ-DOE-pedestrian_behavior-001" in card_ids
    assert "CARD-PRJ-DOE-scenario_difficulty-001" in card_ids


def test_search_by_time_limit_param() -> None:
    result = search_policy_chunks(
        RagSearchQuery(query="", topK=5, policyParamFilter=["time_limit_s"])
    )
    card_ids = {r.cardId for r in result.results}
    assert "CARD-PRJ-DOE-episode_time_budget-001" in card_ids


def test_search_by_force_action_param() -> None:
    result = search_policy_chunks(
        RagSearchQuery(query="", topK=5, policyParamFilter=["force_action_override"])
    )
    card_ids = {r.cardId for r in result.results}
    assert "CARD-PRJ-AGENT-policy_server_logic-001" in card_ids


# ── 컨텍스트 빌더 ──────────────────────────────────────────────────────────


def test_retrieve_episode_setup_context_pedestrian_collision() -> None:
    es = _episode_stats(pedestrian_collision=1, near_miss_min=0.3)
    ms = _measurement_stats()
    contexts = retrieve_episode_setup_context(es, ms)
    params = {c.param for c in contexts}
    assert "actors.pedestrians[*].movement.speed_mps" in params
    assert "evaluation.near_miss.distance_m" in params
    cites = context_citation_ids_multi(contexts)
    assert "PRJ-EVAL" in cites or "PRJ-DOE" in cites


def test_retrieve_policy_server_context_timeout() -> None:
    es = _episode_stats(terminal_reason="Timeout")
    ms = _measurement_stats(repath_ratio=0.2, brake_ratio=0.4)
    contexts = retrieve_policy_server_context(es, ms)
    params = {c.param for c in contexts}
    assert "force_action_override" in params
    assert "stop_distance_m_threshold" in params
    cites = context_citation_ids_multi(contexts)
    assert "PRJ-AGENT" in cites


# ── 기존 검색 회귀 ─────────────────────────────────────────────────────────


def test_existing_param_searches_still_work() -> None:
    # 기존 KOR-003 speed_policy 카드 두 장이 여전히 maxSpeedKmh 검색에 잡혀야 한다.
    result = search_policy_chunks(
        RagSearchQuery(query="", topK=10, policyParamFilter=["maxSpeedKmh"])
    )
    kor_speed_cards = {
        r.cardId for r in result.results
        if r.cardId in {"CARD-KOR-003-speed_policy-001", "CARD-KOR-003-speed_policy-002"}
    }
    assert kor_speed_cards == {
        "CARD-KOR-003-speed_policy-001",
        "CARD-KOR-003-speed_policy-002",
    }


def test_existing_episode_retrieve_policy_context_pedestrian() -> None:
    # 기존 retrieve_policy_context (bot setup 추천용)도 보행자 충돌 통계로 stopDistanceM 카드를 잡아야 한다.
    es = _episode_stats(pedestrian_collision=1, near_miss_min=0.3)
    contexts = retrieve_policy_context(es)
    params = {c.param for c in contexts}
    assert "stopDistanceM" in params
