from __future__ import annotations

from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def test_openai_first_handoff_result_doc_exists() -> None:
    assert (ROOT / "docs" / "OPENAI_FIRST_HANDOFF_RESULT.md").exists()


def test_readmes_link_openai_first_handoff_result() -> None:
    root_readme = (ROOT / "README.md").read_text(encoding="utf-8-sig")
    docs_readme = (ROOT / "docs" / "README.md").read_text(encoding="utf-8-sig")

    assert "docs/OPENAI_FIRST_HANDOFF_RESULT.md" in root_readme
    assert "OPENAI_FIRST_HANDOFF_RESULT.md" in docs_readme


def test_openai_first_handoff_result_contains_required_terms() -> None:
    text = (ROOT / "docs" / "OPENAI_FIRST_HANDOFF_RESULT.md").read_text(
        encoding="utf-8-sig"
    )

    assert "OpenAI-first" in text or "providerUsed=openai" in text
    assert "Ollama" in text and "fallback" in text
    assert "API key" in text and "저장하지" in text
    assert "full WorldConfig" in text
    assert "full EpisodeSpec" in text


def test_handoff_readiness_checklist_mentions_openai_episode_spec_smoke() -> None:
    text = (ROOT / "docs" / "HANDOFF_READINESS_CHECKLIST.md").read_text(
        encoding="utf-8-sig"
    )

    assert "OpenAI EpisodeSpec handoff smoke 성공" in text


def test_forbidden_artifacts_are_not_created() -> None:
    forbidden_paths = [
        ROOT / "samples",
        ROOT / "fixtures",
        ROOT / "data" / "rag" / "vector_db",
        ROOT / "data" / "rag" / "embeddings",
        ROOT / "data" / "rag" / "chroma",
        ROOT / "ue",
        ROOT / "UE",
    ]
    for path in forbidden_paths:
        assert not path.exists(), f"Forbidden artifact exists: {path}"
