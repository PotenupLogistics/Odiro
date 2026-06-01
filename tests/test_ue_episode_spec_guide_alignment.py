from __future__ import annotations

from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
GUIDE = ROOT / "docs" / "UE_EPISODE_SPEC_JSON_GUIDE.md"


def test_ue_episode_spec_json_guide_exists_and_contains_core_contract_terms() -> None:
    text = GUIDE.read_text(encoding="utf-8")

    for term in [
        "episode_actor_spawn_mvp",
        "ground_model",
        "paths",
        "actors",
        "location_m",
        "size_m",
        "speed_mps",
        "prop_id",
        "properties",
    ]:
        assert term in text


def test_project_docs_reference_ue_episode_spec_guide_as_source_of_truth() -> None:
    for path in [
        ROOT / "README.md",
        ROOT / "docs" / "README.md",
        ROOT / "docs" / "UE5_EPISODE_SPEC_ADAPTER.md",
        ROOT / "docs" / "UE5_ENDPOINT_USAGE_FOR_UE_TEAM.md",
        ROOT / "docs" / "UE_TEAM_HANDOFF_PACKAGE.md",
        ROOT / "docs" / "UE_INTEGRATION_HANDOFF_INDEX.md",
    ]:
        assert "UE_EPISODE_SPEC_JSON_GUIDE.md" in path.read_text(encoding="utf-8"), path


def test_no_forbidden_generated_artifacts_exist() -> None:
    for path in [
        ROOT / "samples",
        ROOT / "fixtures",
        ROOT / "data" / "rag" / "vector_db",
        ROOT / "data" / "rag" / "embeddings",
        ROOT / "ue",
        ROOT / "UE",
    ]:
        assert not path.exists(), f"Forbidden artifact exists: {path}"
