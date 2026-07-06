from __future__ import annotations

import importlib.util
import json
import os
import subprocess
import sys
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


ROOT = Path(__file__).resolve().parents[1]
INDEX_SCRIPT = ROOT / "scripts" / "build_pdf_rag_index.py"


def _load_index_script_module():
    spec = importlib.util.spec_from_file_location("build_pdf_rag_index_for_test", INDEX_SCRIPT)
    assert spec is not None
    assert spec.loader is not None
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


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


class RecordingEmbeddingClient:
    """Test embedding client that records each batch sent by the index builder."""

    provider = "openai"
    model = "text-embedding-3-small"

    def __init__(self) -> None:
        """Initialize an empty call log."""
        self.calls: list[list[str]] = []

    def embed_texts(self, texts: list[str]) -> list[list[float]]:
        """Record one embedding batch and return deterministic vectors."""
        self.calls.append(list(texts))
        return [[float(index), float(len(text))] for index, text in enumerate(texts)]


def _write_many_child_chunks(path: Path, *, count: int) -> list[str]:
    """Write one parent chunk and several child chunks for batch-oriented tests."""
    path.parent.mkdir(parents=True, exist_ok=True)
    texts = [f"chunk text {index}" for index in range(count)]
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
        }
    ]
    for index, text in enumerate(texts):
        rows.append(
            {
                "chunk_id": f"KOR-004-child-speed-{index}",
                "parent_chunk_id": "KOR-004-parent-speed",
                "source_id": "KOR-004",
                "source_type": "official_notice",
                "authority_rank": 1,
                "version_status": "active",
                "route_names": ["safety_certification"],
                "chunk_kind": "child",
                "text": text,
                "chunk_hash": f"sha256:test-{index}",
            }
        )
    path.write_text(
        "".join(json.dumps(row, ensure_ascii=False) + "\n" for row in rows),
        encoding="utf-8",
    )
    return texts


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


def test_atomic_index_build_batches_child_embeddings(tmp_path: Path) -> None:
    chunk_file = tmp_path / "validated_parent_child_chunks.jsonl"
    active_dir = tmp_path / "active"
    texts = _write_many_child_chunks(chunk_file, count=5)
    embedding_client = RecordingEmbeddingClient()

    result = build_pdf_rag_index_atomic(
        chunk_file=chunk_file,
        active_dir=active_dir,
        embedding_client=embedding_client,
        collection_name="pdf_rag_test",
        run_smoke_tests=False,
        embedding_batch_size=2,
    )

    assert result.embedded_child_count == 5
    assert result.collection_count == 5
    assert embedding_client.calls == [texts[0:2], texts[2:4], texts[4:5]]


def test_index_cli_loads_agents_env_without_overriding_process_env(monkeypatch, tmp_path: Path) -> None:
    env_file = tmp_path / ".env"
    env_file.write_text(
        "\n".join(
            [
                "OPENAI_API_KEY=dotenv-key-is-not-printed",
                "PDF_RAG_EMBEDDING_MODEL=dotenv-model",
                "PDF_RAG_QUERY_TIMEOUT_SEC=13",
                "PDF_RAG_QUERY_MAX_RETRIES=0",
            ]
        )
        + "\n",
        encoding="utf-8",
    )
    monkeypatch.setenv("OPENAI_API_KEY", "process-key-is-not-printed")
    monkeypatch.delenv("PDF_RAG_EMBEDDING_MODEL", raising=False)
    monkeypatch.delenv("PDF_RAG_QUERY_TIMEOUT_SEC", raising=False)
    monkeypatch.delenv("PDF_RAG_QUERY_MAX_RETRIES", raising=False)
    module = _load_index_script_module()

    module.load_agents_dotenv(env_file)
    args = module.build_parser().parse_args([])

    assert os.environ["OPENAI_API_KEY"] == "process-key-is-not-printed"
    assert args.model == "dotenv-model"
    assert args.timeout_sec == 13
    assert args.max_retries == 0


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


def _run_index_cli(*args: str) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        [sys.executable, str(INDEX_SCRIPT), *args],
        cwd=ROOT,
        text=True,
        capture_output=True,
    )


def test_check_only_reports_missing_index_without_openai_call(tmp_path: Path) -> None:
    chunk_file = tmp_path / "validated_parent_child_chunks.jsonl"
    active_dir = tmp_path / "active"
    _write_chunks(chunk_file)

    completed = _run_index_cli(
        "--check-only",
        "--chunk-file",
        str(chunk_file),
        "--active-dir",
        str(active_dir),
        "--collection-name",
        "pdf_rag_test",
        "--model",
        "text-embedding-3-small",
    )

    assert completed.returncode == 10
    assert "missing" in completed.stdout.lower()
    assert "OPENAI_API_KEY" not in completed.stdout


def test_check_only_reports_up_to_date_index(tmp_path: Path) -> None:
    chunk_file = tmp_path / "validated_parent_child_chunks.jsonl"
    active_dir = tmp_path / "active"
    _write_chunks(chunk_file)
    build_pdf_rag_index_atomic(
        chunk_file=chunk_file,
        active_dir=active_dir,
        embedding_client=FakeEmbeddingClient(),
        collection_name="pdf_rag_test",
        run_smoke_tests=False,
    )

    completed = _run_index_cli(
        "--check-only",
        "--chunk-file",
        str(chunk_file),
        "--active-dir",
        str(active_dir),
        "--collection-name",
        "pdf_rag_test",
        "--model",
        "text-embedding-3-small",
    )

    assert completed.returncode == 0
    assert "up to date" in completed.stdout.lower()
