from __future__ import annotations

from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
RESULT_DOC = ROOT / "docs" / "handoff" / "HANDOFF_RELEASE_NOTES.md"


def test_environment_sampling_handoff_result_doc_exists_and_records_core_values() -> None:
    text = RESULT_DOC.read_text(encoding="utf-8")

    assert "# Handoff Release Notes" in text
    assert "sidewalkWidthCm=120" in text
    assert "obstacleBlockingRatio=0.6" in text
    assert "timeLimitSec=60" in text
    assert "DOE" in text
    assert "batch" in text


def test_readmes_link_environment_sampling_handoff_result() -> None:
    root_readme = (ROOT / "README.md").read_text(encoding="utf-8")
    docs_readme = (ROOT / "docs" / "README.md").read_text(encoding="utf-8")

    assert "HANDOFF_RELEASE_NOTES.md" in root_readme
    assert "HANDOFF_RELEASE_NOTES.md" in docs_readme


def test_environment_sampling_handoff_docs_do_not_create_forbidden_artifacts() -> None:
    assert not (ROOT / "samples").exists()
    assert not (ROOT / "fixtures").exists()
    assert not (ROOT / "data" / "rag" / "vector_db").exists()
    assert not (ROOT / "data" / "rag" / "embeddings").exists()
