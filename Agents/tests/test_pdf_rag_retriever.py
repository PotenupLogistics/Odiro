from __future__ import annotations

import json
from pathlib import Path

from app.services import pdf_rag_retriever as retriever_module
from app.services.pdf_rag_index import (
    FakeEmbeddingClient,
    PdfRagEmbeddingError,
    build_pdf_rag_index_atomic,
)
from app.core.settings import Settings
from app.services.pdf_rag_retriever import (
    ChromaPdfRagVectorIndex,
    InMemoryPdfRagVectorIndex,
    PdfRagRetrieverConfig,
    PdfVectorHybridRagRetriever,
    _score_keyword_hits,
    expand_query_terms,
    get_pdf_rag_retriever,
    route_query,
)


def _chunks() -> list[dict]:
    return [
        {
            "chunk_id": "KOR-004-child-speed",
            "parent_chunk_id": "KOR-004-parent-speed",
            "source_id": "KOR-004",
            "source_type": "official_notice",
            "authority_rank": 1,
            "version_status": "active",
            "page_start": 8,
            "page_end": 9,
            "section_title": "운행속도",
            "topic_tags": ["speed_policy"],
            "use_scope": ["certification_requirement", "safety_rule"],
            "route_names": ["safety_certification", "crosswalk_sidewalk"],
            "chunk_kind": "child",
            "language": "ko",
            "text": "보호구역 운행속도 기준은 5 km/h 이하이며 속도 정확도 시험 조건을 포함한다.",
            "parent_summary": "운행속도 인증 기준",
            "chunk_hash": "sha256:speed",
            "review_status": "auto_validated",
            "extraction_confidence": "high",
        },
        {
            "chunk_id": "RSR-003-child-coverage",
            "parent_chunk_id": "RSR-003-parent-coverage",
            "source_id": "RSR-003",
            "source_type": "research_paper",
            "authority_rank": 3,
            "version_status": "active",
            "page_start": 12,
            "page_end": 13,
            "section_title": "Combinatorial Coverage",
            "topic_tags": ["coverage_model", "constraint_model"],
            "use_scope": ["experiment_design", "coverage_gap", "next_run_recommendation"],
            "route_names": ["experiment_coverage"],
            "chunk_kind": "child",
            "language": "en",
            "text": "ACTS builds t-way parameter value coverage and excludes invalid combinations with constraints.",
            "parent_summary": "ACTS coverage model",
            "chunk_hash": "sha256:coverage",
            "review_status": "auto_validated",
            "extraction_confidence": "high",
        },
    ]


def _write_chunk_file(path: Path, chunks: list[dict]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    rows = chunks + [
        {
            "chunk_id": "KOR-004-parent-speed",
            "source_id": "KOR-004",
            "source_type": "official_notice",
            "authority_rank": 1,
            "version_status": "active",
            "page_start": 8,
            "page_end": 9,
            "section_title": "운행속도",
            "topic_tags": ["speed_policy"],
            "use_scope": ["certification_requirement", "safety_rule"],
            "route_names": ["safety_certification", "crosswalk_sidewalk"],
            "chunk_kind": "parent",
            "language": "ko",
            "text": "운행속도 인증 기준 전체",
            "parent_summary": "운행속도 인증 기준",
            "chunk_hash": "sha256:parent",
            "review_status": "auto_validated",
            "extraction_confidence": "high",
        }
    ]
    path.write_text("".join(json.dumps(row, ensure_ascii=False) + "\n" for row in rows), encoding="utf-8")


def _keyword_child(chunk_id: str, text: str) -> dict:
    return {
        "chunk_id": chunk_id,
        "parent_chunk_id": f"{chunk_id}-parent",
        "source_id": "KOR-004",
        "source_type": "official_notice",
        "authority_rank": 1,
        "version_status": "active",
        "page_start": 1,
        "page_end": 1,
        "section_title": "키워드 검증",
        "topic_tags": ["speed_policy"],
        "use_scope": ["certification_requirement", "safety_rule"],
        "route_names": ["safety_certification"],
        "chunk_kind": "child",
        "language": "ko",
        "text": text,
    }


def test_expand_query_terms_maps_event_names_to_korean_policy_terms() -> None:
    expanded = expand_query_terms("static_obstacle_collision slowdown")

    assert "장애물 감지" in expanded
    assert "운행속도" in expanded


def test_route_query_keeps_research_out_of_safety_certification_route() -> None:
    assert route_query("보호구역 운행속도 기준") == "safety_certification"
    assert route_query("파라미터 조합 coverage gap") == "experiment_coverage"


def test_keyword_search_uses_bm25_idf_for_rare_terms() -> None:
    chunks = [
        _keyword_child("common-alpha", "공통 alpha"),
        _keyword_child("common-beta", "공통 beta"),
        _keyword_child("rare-gamma", "공통 희귀"),
    ]

    rare_scores = _score_keyword_hits(query="희귀", chunks=chunks, route_name="safety_certification", top_k=3)
    common_scores = _score_keyword_hits(query="공통", chunks=chunks, route_name="safety_certification", top_k=3)

    assert rare_scores["rare-gamma"] > common_scores["rare-gamma"]


def test_keyword_search_uses_bm25_term_frequency() -> None:
    chunks = [
        _keyword_child("once", "위험 alpha beta"),
        _keyword_child("repeated", "위험 위험 위험 alpha beta"),
    ]

    scores = _score_keyword_hits(query="위험", chunks=chunks, route_name="safety_certification", top_k=2)

    assert scores["repeated"] > scores["once"]


def test_keyword_search_uses_bm25_length_normalization() -> None:
    long_tail = " ".join(f"noise{index}" for index in range(80))
    chunks = [
        _keyword_child("short", "속도 제한"),
        _keyword_child("long", f"속도 제한 {long_tail}"),
    ]

    scores = _score_keyword_hits(query="속도", chunks=chunks, route_name="safety_certification", top_k=2)

    assert scores["short"] > scores["long"]


def test_keyword_search_uses_query_expansion_for_korean_chunks() -> None:
    chunks = [
        _keyword_child("korean-obstacle", "장애물 감지와 충돌 방지 시험 조건"),
    ]

    scores = _score_keyword_hits(
        query="static_obstacle_collision",
        chunks=chunks,
        route_name="safety_certification",
        top_k=1,
    )

    assert "korean-obstacle" in scores


def test_hybrid_retrieval_merges_vector_and_keyword_with_parent_context() -> None:
    retriever = PdfVectorHybridRagRetriever(
        chunks=_chunks(),
        vector_index=InMemoryPdfRagVectorIndex({"보호구역 운행속도 기준": ["KOR-004-child-speed"]}),
        config=PdfRagRetrieverConfig(context_item_limit=3),
    )

    result = retriever.retrieve("보호구역 운행속도 기준", route_hint="safety_certification")

    assert result.available is True
    assert [item.source_id for item in result.context_pack.evidence_items][:1] == ["KOR-004"]
    assert result.context_pack.evidence_items[0].parent_summary == "운행속도 인증 기준"
    assert result.diagnostic["backend"] == "pdf_vector_hybrid"


def test_safety_route_blocks_rsr_top_evidence_even_when_vector_returns_research() -> None:
    retriever = PdfVectorHybridRagRetriever(
        chunks=_chunks(),
        vector_index=InMemoryPdfRagVectorIndex(
            {"보호구역 운행속도 기준": ["RSR-003-child-coverage", "KOR-004-child-speed"]}
        ),
    )

    result = retriever.retrieve("보호구역 운행속도 기준", route_hint="safety_certification")

    assert result.available is True
    assert result.context_pack.evidence_items
    assert result.context_pack.evidence_items[0].source_id != "RSR-003"


def test_missing_vector_index_returns_unavailable_without_legacy_jsonl_fallback() -> None:
    retriever = PdfVectorHybridRagRetriever(chunks=_chunks(), vector_index=None)

    result = retriever.retrieve("보호구역 운행속도 기준", route_hint="safety_certification")

    assert result.available is False
    assert result.context_pack.evidence_items == []
    assert result.diagnostic["rag_unavailable"] is True
    assert result.diagnostic["rag_error_type"] == "chroma_index_missing"
    assert "legacy" not in str(result.diagnostic).lower()


def test_chroma_runtime_retrieval_embeds_query_and_queries_collection(tmp_path: Path) -> None:
    chunk_file = tmp_path / "validated_parent_child_chunks.jsonl"
    active_dir = tmp_path / "chroma"
    chunks = _chunks()
    _write_chunk_file(chunk_file, chunks)
    build_pdf_rag_index_atomic(
        chunk_file=chunk_file,
        active_dir=active_dir,
        embedding_client=FakeEmbeddingClient(),
        collection_name="pdf_rag_test",
        run_smoke_tests=True,
    )
    query_embedding_client = FakeEmbeddingClient()
    vector_index = ChromaPdfRagVectorIndex(
        active_dir=active_dir,
        collection_name="pdf_rag_test",
        embedding_client=query_embedding_client,
    )
    retriever = PdfVectorHybridRagRetriever(chunks=chunks, vector_index=vector_index)

    result = retriever.retrieve("보호구역 운행속도 기준", route_hint="safety_certification")

    assert result.available is True
    assert query_embedding_client.embedded_texts
    assert "보호구역" in query_embedding_client.embedded_texts[-1]
    assert vector_index.query_count == 1


def test_chroma_runtime_routes_experiment_coverage_to_rsr_003(tmp_path: Path) -> None:
    chunk_file = tmp_path / "validated_parent_child_chunks.jsonl"
    active_dir = tmp_path / "chroma"
    chunks = _chunks()
    _write_chunk_file(chunk_file, chunks)
    build_pdf_rag_index_atomic(
        chunk_file=chunk_file,
        active_dir=active_dir,
        embedding_client=FakeEmbeddingClient(),
        collection_name="pdf_rag_test",
        run_smoke_tests=True,
    )
    query_embedding_client = FakeEmbeddingClient()
    vector_index = ChromaPdfRagVectorIndex(
        active_dir=active_dir,
        collection_name="pdf_rag_test",
        embedding_client=query_embedding_client,
    )
    retriever = PdfVectorHybridRagRetriever(chunks=chunks, vector_index=vector_index)

    result = retriever.retrieve(
        "coverage gap을 줄이기 위한 scenario matrix 조합 추천",
        route_hint="experiment_coverage",
    )

    assert result.available is True
    assert result.context_pack.evidence_items
    assert result.context_pack.evidence_items[0].source_id == "RSR-003"
    assert result.context_pack.evidence_items[0].chunk_id == "RSR-003-child-coverage"
    assert vector_index.query_count == 1

    safety_result = retriever.retrieve("보호구역 운행속도 기준", route_hint="safety_certification")

    assert safety_result.available is True
    assert safety_result.context_pack.evidence_items
    assert safety_result.context_pack.evidence_items[0].source_id == "KOR-004"
    assert all(item.source_id != "RSR-003" for item in safety_result.context_pack.evidence_items)
    assert vector_index.query_count == 2


def test_query_embedding_failure_returns_unavailable_without_legacy_fallback(tmp_path: Path) -> None:
    class MissingKeyEmbeddingClient(FakeEmbeddingClient):
        def embed_texts(self, texts: list[str]) -> list[list[float]]:
            raise PdfRagEmbeddingError(
                rag_error_type="query_embedding_failure",
                embedding_error_type="api_key_missing",
            )

    chunk_file = tmp_path / "validated_parent_child_chunks.jsonl"
    active_dir = tmp_path / "chroma"
    chunks = _chunks()
    _write_chunk_file(chunk_file, chunks)
    build_pdf_rag_index_atomic(
        chunk_file=chunk_file,
        active_dir=active_dir,
        embedding_client=FakeEmbeddingClient(),
        collection_name="pdf_rag_test",
        run_smoke_tests=False,
    )
    retriever = PdfVectorHybridRagRetriever(
        chunks=chunks,
        vector_index=ChromaPdfRagVectorIndex(
            active_dir=active_dir,
            collection_name="pdf_rag_test",
            embedding_client=MissingKeyEmbeddingClient(),
        ),
    )

    result = retriever.retrieve("보호구역 운행속도 기준", route_hint="safety_certification")

    assert result.available is False
    assert result.diagnostic["rag_unavailable"] is True
    assert result.diagnostic["rag_error_type"] == "query_embedding_failure"
    assert result.diagnostic["embedding_error_type"] == "api_key_missing"
    assert "legacy" not in str(result.diagnostic).lower()


def test_query_embedding_timeout_returns_unavailable_without_legacy_fallback(tmp_path: Path) -> None:
    class TimeoutEmbeddingClient(FakeEmbeddingClient):
        def embed_texts(self, texts: list[str]) -> list[list[float]]:
            raise TimeoutError("embedding request timed out")

    chunk_file = tmp_path / "validated_parent_child_chunks.jsonl"
    active_dir = tmp_path / "chroma"
    chunks = _chunks()
    _write_chunk_file(chunk_file, chunks)
    build_pdf_rag_index_atomic(
        chunk_file=chunk_file,
        active_dir=active_dir,
        embedding_client=FakeEmbeddingClient(),
        collection_name="pdf_rag_test",
        run_smoke_tests=False,
    )
    retriever = PdfVectorHybridRagRetriever(
        chunks=chunks,
        vector_index=ChromaPdfRagVectorIndex(
            active_dir=active_dir,
            collection_name="pdf_rag_test",
            embedding_client=TimeoutEmbeddingClient(),
        ),
    )

    result = retriever.retrieve("보호구역 운행속도 기준", route_hint="safety_certification")

    assert result.available is False
    assert result.diagnostic["rag_unavailable"] is True
    assert result.diagnostic["rag_error_type"] == "embedding_timeout"
    assert result.diagnostic["embedding_error_type"] == "embedding_timeout"
    assert "legacy" not in str(result.diagnostic).lower()


def test_embedding_model_mismatch_returns_unavailable_before_query_embedding(tmp_path: Path) -> None:
    chunk_file = tmp_path / "validated_parent_child_chunks.jsonl"
    active_dir = tmp_path / "chroma"
    chunks = _chunks()
    _write_chunk_file(chunk_file, chunks)
    build_pdf_rag_index_atomic(
        chunk_file=chunk_file,
        active_dir=active_dir,
        embedding_client=FakeEmbeddingClient(model="text-embedding-3-small"),
        collection_name="pdf_rag_test",
        run_smoke_tests=False,
    )
    query_embedding_client = FakeEmbeddingClient(model="text-embedding-3-large")
    retriever = get_pdf_rag_retriever(
        corpus_path=chunk_file,
        chroma_dir=active_dir,
        embedding_client=query_embedding_client,
        collection_name="pdf_rag_test",
        settings=Settings(openaiApiKey="unused", pdfRagEmbeddingModel="text-embedding-3-large"),
    )

    result = retriever.retrieve("보호구역 운행속도 기준", route_hint="safety_certification")

    assert result.available is False
    assert result.diagnostic["rag_error_type"] == "embedding_model_mismatch"
    assert result.diagnostic["embedding_error_type"] == "model_mismatch"
    assert query_embedding_client.embedded_texts == []


def test_missing_chroma_manifest_returns_unavailable_before_query_embedding(tmp_path: Path) -> None:
    chunk_file = tmp_path / "validated_parent_child_chunks.jsonl"
    chunks = _chunks()
    _write_chunk_file(chunk_file, chunks)
    query_embedding_client = FakeEmbeddingClient()
    retriever = get_pdf_rag_retriever(
        corpus_path=chunk_file,
        chroma_dir=tmp_path / "missing-chroma",
        embedding_client=query_embedding_client,
        collection_name="pdf_rag_test",
    )

    result = retriever.retrieve("보호구역 운행속도 기준", route_hint="safety_certification")

    assert result.available is False
    assert result.diagnostic["rag_error_type"] == "chroma_index_missing"
    assert result.diagnostic["embedding_error_type"] is None
    assert query_embedding_client.embedded_texts == []


def test_chroma_query_failure_returns_unavailable_without_legacy_fallback(monkeypatch, tmp_path: Path) -> None:
    class FailingCollection:
        def query(self, **_kwargs):
            raise RuntimeError("chroma query failed")

    class FailingClient:
        def get_collection(self, name: str):
            return FailingCollection()

        def close(self) -> None:
            return None

        def clear_system_cache(self) -> None:
            return None

    chunk_file = tmp_path / "validated_parent_child_chunks.jsonl"
    active_dir = tmp_path / "chroma"
    chunks = _chunks()
    _write_chunk_file(chunk_file, chunks)
    build_pdf_rag_index_atomic(
        chunk_file=chunk_file,
        active_dir=active_dir,
        embedding_client=FakeEmbeddingClient(),
        collection_name="pdf_rag_test",
        run_smoke_tests=False,
    )
    monkeypatch.setattr(retriever_module, "_create_chroma_client", lambda _active_dir: FailingClient())
    retriever = PdfVectorHybridRagRetriever(
        chunks=chunks,
        vector_index=ChromaPdfRagVectorIndex(
            active_dir=active_dir,
            collection_name="pdf_rag_test",
            embedding_client=FakeEmbeddingClient(),
        ),
    )

    result = retriever.retrieve("보호구역 운행속도 기준", route_hint="safety_certification")

    assert result.available is False
    assert result.diagnostic["rag_unavailable"] is True
    assert result.diagnostic["rag_error_type"] == "chroma_query_failure"
    assert result.diagnostic["embedding_error_type"] is None
    assert result.diagnostic["detail"] == "RuntimeError"
    assert "legacy" not in str(result.diagnostic).lower()


def test_stale_chroma_manifest_returns_unavailable_before_query_embedding(tmp_path: Path) -> None:
    chunk_file = tmp_path / "validated_parent_child_chunks.jsonl"
    active_dir = tmp_path / "chroma"
    chunks = _chunks()
    _write_chunk_file(chunk_file, chunks)
    build_pdf_rag_index_atomic(
        chunk_file=chunk_file,
        active_dir=active_dir,
        embedding_client=FakeEmbeddingClient(),
        collection_name="pdf_rag_test",
        run_smoke_tests=False,
    )
    chunks[0] = {**chunks[0], "text": "변경된 운행속도 기준"}
    _write_chunk_file(chunk_file, chunks)
    query_embedding_client = FakeEmbeddingClient()
    retriever = get_pdf_rag_retriever(
        corpus_path=chunk_file,
        chroma_dir=active_dir,
        embedding_client=query_embedding_client,
        collection_name="pdf_rag_test",
    )

    result = retriever.retrieve("보호구역 운행속도 기준", route_hint="safety_certification")

    assert result.available is False
    assert result.diagnostic["rag_error_type"] == "chroma_index_stale"
    assert result.diagnostic["embedding_error_type"] is None
    assert query_embedding_client.embedded_texts == []


def test_missing_openai_api_key_returns_unavailable_without_legacy_fallback(tmp_path: Path) -> None:
    chunk_file = tmp_path / "validated_parent_child_chunks.jsonl"
    active_dir = tmp_path / "chroma"
    chunks = _chunks()
    _write_chunk_file(chunk_file, chunks)
    build_pdf_rag_index_atomic(
        chunk_file=chunk_file,
        active_dir=active_dir,
        embedding_client=FakeEmbeddingClient(),
        collection_name="pdf_rag_test",
        run_smoke_tests=False,
    )
    retriever = get_pdf_rag_retriever(
        corpus_path=chunk_file,
        chroma_dir=active_dir,
        collection_name="pdf_rag_test",
        settings=Settings(openaiApiKey="", pdfRagEmbeddingModel="text-embedding-3-small"),
    )

    result = retriever.retrieve("보호구역 운행속도 기준", route_hint="safety_certification")

    assert result.available is False
    assert result.diagnostic["rag_error_type"] == "query_embedding_failure"
    assert result.diagnostic["embedding_error_type"] == "api_key_missing"
    assert "legacy" not in str(result.diagnostic).lower()
