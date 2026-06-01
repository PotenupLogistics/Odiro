from __future__ import annotations

from pathlib import Path

from app.models.rag import RagSearchQuery
from app.services.policy_rag_retriever import load_chunks, search_policy_chunks


ROOT = Path(__file__).resolve().parents[1]


def test_loads_nine_policy_rag_chunks() -> None:
    assert len(load_chunks()) == 9


def test_search_emergency_stop_keyword_returns_emergency_result() -> None:
    result = search_policy_chunks(RagSearchQuery(query="비상정지"))
    assert result.results
    assert any(chunk.metadata.category == "emergency_stop" for chunk in result.results)


def test_search_speed_keyword_returns_speed_policy_result() -> None:
    result = search_policy_chunks(RagSearchQuery(query="속도"))
    assert result.results
    assert any(chunk.metadata.category == "speed_policy" for chunk in result.results)


def test_action_filter_emergency_stop_returns_results() -> None:
    result = search_policy_chunks(RagSearchQuery(query="", actionFilter=["EmergencyStop"]))
    assert result.results
    assert all("EmergencyStop" in chunk.metadata.relatedActions for chunk in result.results)


def test_policy_param_filter_returns_results() -> None:
    result = search_policy_chunks(
        RagSearchQuery(query="", policyParamFilter=["emergencyStopDistanceCm"])
    )
    assert result.results
    assert all("emergencyStopDistanceCm" in chunk.metadata.relatedPolicyParams for chunk in result.results)


def test_category_filter_speed_policy_returns_results() -> None:
    result = search_policy_chunks(RagSearchQuery(query="", categoryFilter=["speed_policy"]))
    assert result.results
    assert all(chunk.metadata.category == "speed_policy" for chunk in result.results)


def test_top_k_is_applied() -> None:
    result = search_policy_chunks(RagSearchQuery(query="", topK=2))
    assert len(result.results) == 2


def test_no_vector_embedding_sample_fixture_artifacts_exist() -> None:
    assert not (ROOT / "data" / "rag" / "embeddings").exists()
    assert not (ROOT / "data" / "rag" / "vector_db").exists()
    assert not (ROOT / "data" / "rag" / "chroma").exists()
    assert not (ROOT / "samples").exists()
    assert not (ROOT / "fixtures").exists()
