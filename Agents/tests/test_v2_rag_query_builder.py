from __future__ import annotations

from app.agents.result_analysis_v2.rag_query_builder import RagQueryBuilderV2


def test_blocked_region_violation_repeated_maps_to_policy_safety_query() -> None:
    query = RagQueryBuilderV2().build_query_for_pattern(
        {"pattern_id": "PATTERN-001", "type": "blocked_region_violation_repeated"}
    )

    assert query["query_type"] == "policy_safety"
    assert query["query_id"]


def test_near_miss_repeated_maps_to_pedestrian_safety_query() -> None:
    query = RagQueryBuilderV2().build_query_for_pattern({"pattern_id": "PATTERN-002", "type": "near_miss_repeated"})

    assert query["query_type"] == "pedestrian_safety"


def test_collision_repeated_maps_to_collision_prevention_query() -> None:
    query = RagQueryBuilderV2().build_query_for_pattern({"pattern_id": "PATTERN-003", "type": "collision_repeated"})

    assert query["query_type"] == "collision_prevention"


def test_timeout_repeated_maps_to_navigation_efficiency_query() -> None:
    query = RagQueryBuilderV2().build_query_for_pattern({"pattern_id": "PATTERN-004", "type": "timeout_repeated"})

    assert query["query_type"] == "navigation_efficiency"


def test_unknown_pattern_maps_to_general_safety_query() -> None:
    query = RagQueryBuilderV2().build_query_for_pattern({"pattern_id": "PATTERN-999", "type": "unexpected"})

    assert query["query_type"] == "general_safety"


def test_build_queries_assigns_query_ids() -> None:
    queries = RagQueryBuilderV2().build_queries(
        failure_patterns=[
            {"pattern_id": "PATTERN-001", "type": "blocked_region_violation_repeated"},
            {"pattern_id": "PATTERN-002", "type": "near_miss_repeated"},
        ],
        experiment_aggregates=[],
        representative_failed_episodes=[],
    )

    assert [query["query_id"] for query in queries] == ["RAGQ-001", "RAGQ-002"]
