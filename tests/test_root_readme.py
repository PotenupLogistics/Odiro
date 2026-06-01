from __future__ import annotations

from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
README = ROOT / "README.md"


def test_root_readme_exists_and_is_not_empty() -> None:
    assert README.exists()
    assert README.read_text(encoding="utf-8-sig").strip()


def test_root_readme_contains_project_entrypoint_sections() -> None:
    text = README.read_text(encoding="utf-8-sig")
    assert "# Proto-AI" in text
    assert "WorldConfig" in text
    assert "EpisodeSpec" in text
    assert "UE handoff" in text
    assert "responseFormat=episode_spec" in text
    assert "obstacle.kickboard" in text
    assert "obstacle.road_barrier_01" in text
    assert "semantic_type=\"Kickboard\"" in text
    assert "uv run pytest" in text
    assert "harness" in text


def test_root_readme_does_not_create_forbidden_artifacts() -> None:
    assert not (ROOT / "samples").exists()
    assert not (ROOT / "fixtures").exists()
    assert not (ROOT / "data" / "rag" / "vector_db").exists()
    assert not (ROOT / "data" / "rag" / "embeddings").exists()

