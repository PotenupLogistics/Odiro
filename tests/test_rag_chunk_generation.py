from __future__ import annotations

import json
import subprocess
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "scripts" / "generate_rag_chunks.py"
POLICY_CARDS_PATH = ROOT / "data" / "rag" / "policy_knowledge_cards.jsonl"


def _run_generator(tmp_path: Path) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        [
            sys.executable,
            str(SCRIPT),
            "--input",
            str(POLICY_CARDS_PATH),
            "--output",
            str(tmp_path / "policy_rag_chunks.jsonl"),
            "--report-json",
            str(tmp_path / "policy_rag_chunks_report.json"),
            "--report-md",
            str(tmp_path / "policy_rag_chunks_report.md"),
        ],
        cwd=ROOT,
        text=True,
        capture_output=True,
    )


def _read_jsonl(path: Path) -> list[dict]:
    return [json.loads(line) for line in path.read_text(encoding="utf-8-sig").splitlines() if line.strip()]


def test_policy_cards_generate_eleven_rag_chunks(tmp_path: Path) -> None:
    completed = _run_generator(tmp_path)
    assert completed.returncode == 0, completed.stderr
    chunks = _read_jsonl(tmp_path / "policy_rag_chunks.jsonl")
    assert len(chunks) == 11


def test_chunk_text_and_metadata_are_populated(tmp_path: Path) -> None:
    completed = _run_generator(tmp_path)
    assert completed.returncode == 0, completed.stderr
    chunks = _read_jsonl(tmp_path / "policy_rag_chunks.jsonl")
    for chunk in chunks:
        assert chunk["chunkText"]
        assert chunk["metadata"]["category"]
        assert chunk["metadata"]["sourceIds"]
        assert chunk["metadata"]["relatedActions"]
        assert chunk["metadata"]["relatedPolicyParams"]
        assert chunk["metadata"]["status"] == "confirmed_policy_card"


def test_chunk_ids_are_unique(tmp_path: Path) -> None:
    completed = _run_generator(tmp_path)
    assert completed.returncode == 0, completed.stderr
    chunks = _read_jsonl(tmp_path / "policy_rag_chunks.jsonl")
    chunk_ids = [chunk["chunkId"] for chunk in chunks]
    assert len(chunk_ids) == len(set(chunk_ids))


def test_generator_is_idempotent(tmp_path: Path) -> None:
    first = _run_generator(tmp_path)
    second = _run_generator(tmp_path)
    assert first.returncode == 0, first.stderr
    assert second.returncode == 0, second.stderr
    chunks = _read_jsonl(tmp_path / "policy_rag_chunks.jsonl")
    assert len(chunks) == 11


def test_no_source_document_chunks_or_vector_index_are_created() -> None:
    forbidden_patterns = ["*source*_chunks*.jsonl", "*processed*_chunks*.jsonl", "*.chroma", "*.faiss"]
    for pattern in forbidden_patterns:
        assert list((ROOT / "data").rglob(pattern)) == []
    assert not (ROOT / "data" / "rag" / "chroma").exists()
    assert not (ROOT / "data" / "rag" / "vector_db").exists()
