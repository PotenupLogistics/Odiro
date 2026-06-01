from __future__ import annotations

from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def test_handoff_release_readiness_docs_exist() -> None:
    required_docs = [
        ROOT / "docs" / "UE_HANDOFF_DELIVERY_MANIFEST.md",
        ROOT / "docs" / "HANDOFF_RELEASE_NOTES.md",
        ROOT / "docs" / "HANDOFF_READINESS_CHECKLIST.md",
        ROOT / "docs" / "HARNESS_WARNING_EXPLANATION.md",
    ]
    for path in required_docs:
        assert path.exists(), f"{path} is missing"


def test_readme_links_handoff_release_docs() -> None:
    root_readme = (ROOT / "README.md").read_text(encoding="utf-8-sig")
    docs_readme = (ROOT / "docs" / "README.md").read_text(encoding="utf-8-sig")

    assert "UE_HANDOFF_DELIVERY_MANIFEST.md" in root_readme
    assert "HANDOFF_RELEASE_NOTES.md" in root_readme
    assert "HANDOFF_READINESS_CHECKLIST.md" in root_readme
    assert "HARNESS_WARNING_EXPLANATION.md" in root_readme
    assert "HANDOFF_RELEASE_NOTES.md" in docs_readme


def test_delivery_manifest_contains_required_handoff_terms() -> None:
    text = (ROOT / "docs" / "UE_HANDOFF_DELIVERY_MANIFEST.md").read_text(
        encoding="utf-8-sig"
    )

    assert "responseFormat=episode_spec" in text
    assert "responseFormat=both" in text
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
