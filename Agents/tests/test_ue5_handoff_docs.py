from __future__ import annotations

from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def test_ue5_handoff_implementation_docs_exist() -> None:
    required = [
        ROOT / "docs" / "archive" / "previous_episode_spec" / "UE5_WORLD_CONFIG_FIELD_MAPPING.md",
        ROOT / "docs" / "archive" / "previous_episode_spec" / "UE5_CONTROLLED_INTEGRATION_TEST_PLAN.md",
        ROOT / "docs" / "archive" / "previous_episode_spec" / "UE5_WORLD_CONFIG_PARSER_PSEUDOCODE.md",
        ROOT / "docs" / "archive" / "previous_episode_spec" / "UE5_HANDOFF_ACCEPTANCE_CHECKLIST.md",
    ]

    for path in required:
        assert path.exists(), f"missing {path}"


def test_field_mapping_doc_contains_required_ue5_fields() -> None:
    text = (ROOT / "docs" / "archive" / "previous_episode_spec" / "UE5_WORLD_CONFIG_FIELD_MAPPING.md").read_text(encoding="utf-8")

    for term in ["map.lengthCm", "robot.spawn", "obstacles", "pedestrians", "runtime.maxDurationSec"]:
        assert term in text


def test_parser_pseudocode_doc_contains_spawn_steps() -> None:
    text = (ROOT / "docs" / "archive" / "previous_episode_spec" / "UE5_WORLD_CONFIG_PARSER_PSEUDOCODE.md").read_text(encoding="utf-8")

    for term in ["SpawnMap", "SpawnRobot", "SpawnObstacles", "SpawnPedestrians"]:
        assert term in text


def test_acceptance_checklist_includes_failure_guard() -> None:
    text = (ROOT / "docs" / "archive" / "previous_episode_spec" / "UE5_HANDOFF_ACCEPTANCE_CHECKLIST.md").read_text(encoding="utf-8")

    assert "success=false" in text
    assert "실행하지" in text


def test_no_sample_fixture_vector_or_embedding_artifacts_exist() -> None:
    assert not (ROOT / "samples").exists()
    assert not (ROOT / "fixtures").exists()
    assert not (ROOT / "data" / "rag" / "vector_db").exists()
    assert not (ROOT / "data" / "rag" / "embeddings").exists()
