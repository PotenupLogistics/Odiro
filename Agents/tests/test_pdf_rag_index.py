from __future__ import annotations

import json
from pathlib import Path

import pytest

from app.services.pdf_rag_index import (
    FakeEmbeddingClient,
    build_chroma_manifest,
    build_pdf_rag_index_atomic,
    get_chroma_collection_count,
    read_chroma_child_metadata,
    diagnose_chroma_staleness,
)


def _write_chunks(path: Path, *, text: str = "보호구역 운행속도 기준") -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    rows = [
        {
            "chunk_id": "KOR-004-parent-speed",
            "source_id": "KOR-004",
            "source_type": "official_notice",
            "authority_rank": 1,
            "version_status": "active",
            "route_names": ["safety_certification"],
            "chunk_kind": "parent",
            "text": "운행속도 인증 기준 전체",
            "chunk_hash": "sha256:parent",
        },
        {
            "chunk_id": "KOR-004-child-speed",
            "parent_chunk_id": "KOR-004-parent-speed",
            "source_id": "KOR-004",
            "source_type": "official_notice",
            "authority_rank": 1,
            "version_status": "active",
            "route_names": ["safety_certification"],
            "chunk_kind": "child",
            "text": text,
            "chunk_hash": "sha256:test",
        },
    ]
    path.write_text(
        "".join(json.dumps(row, ensure_ascii=False) + "\n" for row in rows),
        encoding="utf-8",
    )


def test_manifest_records_embedding_and_schema_versions(tmp_path: Path) -> None:
    chunk_file = tmp_path / "validated_parent_child_chunks.jsonl"
    _write_chunks(chunk_file)

    manifest = build_chroma_manifest(
        chunk_file=chunk_file,
        chunks=[json.loads(line) for line in chunk_file.read_text(encoding="utf-8").splitlines()],
        embedding_provider="openai",
        embedding_model="text-embedding-3-small",
        collection_name="pdf_rag_test",
        source_hashes={"KOR-004": "sha256:source"},
    )

    assert manifest["embedding_model"] == "text-embedding-3-small"
    assert manifest["chunk_count"] == 2
    assert manifest["embedded_child_count"] == 1
    assert manifest["extractor_version"]
    assert manifest["metadata_schema_version"]


def test_atomic_index_build_embeddings_all_children_and_promotes_active_dir(tmp_path: Path) -> None:
    chunk_file = tmp_path / "validated_parent_child_chunks.jsonl"
    active_dir = tmp_path / "active"
    _write_chunks(chunk_file)
    embedding_client = FakeEmbeddingClient()

    result = build_pdf_rag_index_atomic(
        chunk_file=chunk_file,
        active_dir=active_dir,
        embedding_client=embedding_client,
        collection_name="pdf_rag_test",
        run_smoke_tests=False,
    )

    assert result.promoted is True
    assert embedding_client.embedded_texts == ["보호구역 운행속도 기준"]
    assert (active_dir / "manifest.json").is_file()
    assert not (active_dir / "vectors.json").exists()
    assert get_chroma_collection_count(active_dir, "pdf_rag_test") == 1
    metadata = read_chroma_child_metadata(active_dir, "pdf_rag_test")
    assert metadata == [
        {
            "chunk_id": "KOR-004-child-speed",
            "source_id": "KOR-004",
            "parent_chunk_id": "KOR-004-parent-speed",
            "route_safety_certification": True,
        }
    ]
    assert not (tmp_path / "data" / "rag" / "chroma").exists()
    assert not (tmp_path / "data" / "rag" / "embeddings").exists()


def test_atomic_index_build_failure_keeps_existing_index(tmp_path: Path) -> None:
    chunk_file = tmp_path / "validated_parent_child_chunks.jsonl"
    active_dir = tmp_path / "active"
    active_dir.mkdir()
    (active_dir / "manifest.json").write_text('{"previous": true}\n', encoding="utf-8")
    _write_chunks(chunk_file)

    with pytest.raises(RuntimeError):
        build_pdf_rag_index_atomic(
            chunk_file=chunk_file,
            active_dir=active_dir,
            embedding_client=FakeEmbeddingClient(fail=True),
            collection_name="pdf_rag_test",
        )

    assert json.loads((active_dir / "manifest.json").read_text(encoding="utf-8")) == {"previous": True}


def test_stale_manifest_detects_chunk_file_hash_change(tmp_path: Path) -> None:
    chunk_file = tmp_path / "validated_parent_child_chunks.jsonl"
    active_dir = tmp_path / "active"
    _write_chunks(chunk_file, text="first")
    active_dir.mkdir()
    manifest = build_chroma_manifest(
        chunk_file=chunk_file,
        chunks=[json.loads(line) for line in chunk_file.read_text(encoding="utf-8").splitlines()],
        embedding_provider="openai",
        embedding_model="text-embedding-3-small",
        collection_name="pdf_rag_test",
        source_hashes={},
    )
    (active_dir / "manifest.json").write_text(json.dumps(manifest), encoding="utf-8")
    _write_chunks(chunk_file, text="changed")

    diagnostic = diagnose_chroma_staleness(chunk_file=chunk_file, active_dir=active_dir)

    assert diagnostic["stale"] is True
    assert diagnostic["rag_error_type"] == "chroma_index_stale"
