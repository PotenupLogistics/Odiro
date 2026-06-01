from __future__ import annotations

from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def _read(path: Path) -> str:
    return path.read_text(encoding="utf-8-sig")


def test_map_generation_data_sources_doc_exists_and_explains_sources() -> None:
    data_sources = ROOT / "docs" / "MAP_GENERATION_DATA_SOURCES.md"

    assert data_sources.exists()
    data_text = _read(data_sources)
    assert "법령/인증 문서는 좌표 생성 근거가 아니라 안전 정책 근거로 사용한다" in data_text
    assert "사용자가 명시한 좌표" in data_text
    assert "robot.spawn / robot.goal" in data_text
    assert "route midpoint rule" in data_text
    assert "environmentSampling" in data_text


def test_readmes_link_map_generation_data_sources_doc_only() -> None:
    text = _read(ROOT / "README.md") + "\n" + _read(ROOT / "docs" / "README.md")

    assert "MAP_GENERATION_DATA_SOURCES.md" in text
    assert "LLM_AGENT_MEETING_BRIEF.md" not in text
    assert "INSTRUCTOR_MEETING_QA.md" not in text


def test_removed_meeting_docs_do_not_exist() -> None:
    assert not (ROOT / "docs" / "LLM_AGENT_MEETING_BRIEF.md").exists()
    assert not (ROOT / "docs" / "INSTRUCTOR_MEETING_QA.md").exists()


def test_forbidden_artifacts_are_not_created() -> None:
    for path in [
        ROOT / "samples",
        ROOT / "fixtures",
        ROOT / "data" / "rag" / "vector_db",
        ROOT / "data" / "rag" / "embeddings",
        ROOT / "data" / "rag" / "chroma",
        ROOT / "ue",
        ROOT / "UE",
    ]:
        assert not path.exists(), f"Forbidden artifact exists: {path}"
