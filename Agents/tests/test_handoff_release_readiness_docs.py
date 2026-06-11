from __future__ import annotations

from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def test_handoff_release_readiness_docs_exist() -> None:
    required_docs = [
        ROOT / "docs" / "handoff" / "UE_HANDOFF_DELIVERY_MANIFEST.md",
        ROOT / "docs" / "handoff" / "HANDOFF_RELEASE_NOTES.md",
    ]
    for path in required_docs:
        assert path.exists(), f"{path} is missing"


def test_readme_links_handoff_release_docs() -> None:
    root_readme = (ROOT / "README.md").read_text(encoding="utf-8-sig")
    docs_readme = (ROOT / "docs" / "README.md").read_text(encoding="utf-8-sig")

    assert "UE_HANDOFF_DELIVERY_MANIFEST.md" in root_readme
    assert "HANDOFF_RELEASE_NOTES.md" in root_readme
    assert "HANDOFF_RELEASE_NOTES.md" in docs_readme


def test_handoff_release_notes_contain_current_smoke_summary() -> None:
    text = (ROOT / "docs" / "handoff" / "HANDOFF_RELEASE_NOTES.md").read_text(
        encoding="utf-8-sig"
    )

    assert "OpenAI-first" in text
    assert "setup pair" in text
    assert "policy comparison" in text
    assert "500 passed, 1 warning" in text


def test_delivery_manifest_contains_required_handoff_terms() -> None:
    text = (ROOT / "docs" / "handoff" / "UE_HANDOFF_DELIVERY_MANIFEST.md").read_text(
        encoding="utf-8-sig"
    )

    assert "POST /api/v1/scenarios/generate" in text
    assert "FastAPI/OpenAPI에서 제거" in text
    assert "obstacle.kickboard" in text
    assert "obstacle.road_barrier_01" in text
    assert "ueCompilerReadiness=true" in text


def test_forbidden_release_artifacts_are_not_created() -> None:
    forbidden_paths = [
        ROOT / "samples",
        ROOT / "fixtures",
        ROOT / "data" / "rag" / "vector_db",
        ROOT / "data" / "rag" / "embeddings",
        ROOT / "ue",
        ROOT / "UE",
    ]
    for path in forbidden_paths:
        assert not path.exists(), f"Forbidden artifact exists: {path}"
