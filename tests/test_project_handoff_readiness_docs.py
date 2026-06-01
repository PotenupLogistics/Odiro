from __future__ import annotations

from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
REQUIRED_DOCS = [
    ROOT / "docs" / "UE_INTEGRATION_HANDOFF_INDEX.md",
    ROOT / "docs" / "CURRENT_PROJECT_STATUS.md",
    ROOT / "docs" / "UE_AI_INTEGRATION_ISSUES.md",
    ROOT / "docs" / "NEXT_ACTIONS.md",
]


def test_handoff_readiness_docs_exist() -> None:
    for path in REQUIRED_DOCS:
        assert path.exists()


def test_message_draft_mentions_episode_spec_and_kickboard_request() -> None:
    text = (ROOT / "docs" / "UE_TEAM_MESSAGE_DRAFT.md").read_text(encoding="utf-8-sig")
    assert "responseFormat=episode_spec" in text
    assert "obstacle.kickboard" in text


def test_docs_index_exists_and_links_handoff_docs() -> None:
    index = ROOT / "docs" / "README.md"
    assert index.exists() or (ROOT / "README.md").exists()
    text = index.read_text(encoding="utf-8-sig") if index.exists() else (ROOT / "README.md").read_text(encoding="utf-8-sig")
    assert "UE_INTEGRATION_HANDOFF_INDEX.md" in text
    assert "uv run python -m harness.checks.check_all" in text


def test_no_forbidden_artifacts_created_for_handoff_readiness_docs() -> None:
    assert not (ROOT / "samples").exists()
    assert not (ROOT / "fixtures").exists()
    assert not (ROOT / "data" / "rag" / "vector_db").exists()
    assert not (ROOT / "data" / "rag" / "embeddings").exists()

