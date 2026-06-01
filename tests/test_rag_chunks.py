from __future__ import annotations

import json
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
CHUNKS_PATH = ROOT / "data" / "rag" / "policy_rag_chunks.jsonl"
REPORT_JSON = ROOT / "data" / "rag" / "policy_rag_chunks_report.json"
REPORT_MD = ROOT / "data" / "rag" / "policy_rag_chunks_report.md"


def _read_chunks() -> list[dict]:
    assert CHUNKS_PATH.exists()
    return [json.loads(line) for line in CHUNKS_PATH.read_text(encoding="utf-8-sig").splitlines() if line.strip()]


def test_policy_rag_chunks_exist_and_match_card_count() -> None:
    chunks = _read_chunks()
    assert len(chunks) == 9


def test_policy_rag_chunk_report_exists() -> None:
    assert REPORT_JSON.exists()
    assert REPORT_MD.exists()
    report = json.loads(REPORT_JSON.read_text(encoding="utf-8-sig"))
    assert report["inputCardCount"] == 9
    assert report["generatedChunkCount"] == 9


def test_policy_rag_chunk_required_fields() -> None:
    for chunk in _read_chunks():
        assert set(chunk) == {"chunkId", "cardId", "chunkText", "metadata"}
        metadata = chunk["metadata"]
        for field in [
            "sourceIds",
            "category",
            "relatedPolicyParams",
            "relatedRequestFields",
            "relatedActions",
            "relatedMetrics",
            "evidenceLocation",
            "createdFromCandidateId",
            "status",
        ]:
            assert field in metadata
        assert metadata["status"] == "confirmed_policy_card"


def test_no_embedding_or_vector_db_artifacts_exist() -> None:
    assert not (ROOT / "data" / "rag" / "embeddings").exists()
    assert not (ROOT / "data" / "rag" / "vector_db").exists()
    assert not (ROOT / "data" / "rag" / "chroma").exists()


def test_no_sample_fixture_artifacts_exist() -> None:
    forbidden_names = {
        "world_config.json",
        "policy_config.json",
        "decision_request.json",
        "decision_response.json",
    }
    for folder in ["data", "docs", "harness", "scripts", "tests", "app"]:
        root = ROOT / folder
        for path in root.rglob("*"):
            assert path.name not in forbidden_names
    assert not (ROOT / "samples").exists()
    assert not (ROOT / "fixtures").exists()
