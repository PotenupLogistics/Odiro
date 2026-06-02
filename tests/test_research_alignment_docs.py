from __future__ import annotations

from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def _read(path: Path) -> str:
    return path.read_text(encoding="utf-8-sig")


def test_research_alignment_docs_and_pdfs_exist() -> None:
    assert (ROOT / "docs" / "RESEARCH_ALIGNMENT.md").exists()
    assert (ROOT / "docs" / "EUREKA.pdf").exists()
    assert (ROOT / "docs" / "DREUREKA.pdf").exists()


def test_research_alignment_covers_eureka_dreureka_and_scenic_boundaries() -> None:
    text = _read(ROOT / "docs" / "RESEARCH_ALIGNMENT.md")

    assert "Eureka" in text
    assert "DrEureka" in text
    assert "Scenic" in text
    assert "DrEureka-inspired environment sampling" in text
    assert "Scenic-inspired placement" in text or "Scenic-inspired scenario sampling" in text
    assert "DrEureka 전체 구현이 아니다" in text
    assert "Scenic DSL을 사용하고 있지 않다" in text
    assert "WorldConfig/EpisodeSpec 기반" in text


def test_readmes_link_research_alignment_and_no_scenic_runtime_dependency() -> None:
    readme_text = _read(ROOT / "README.md") + "\n" + _read(ROOT / "docs" / "README.md")
    pyproject_text = _read(ROOT / "pyproject.toml").lower()

    assert "RESEARCH_ALIGNMENT.md" in readme_text
    assert "scenic" not in pyproject_text


def test_research_alignment_tests_do_not_import_scenic_runtime() -> None:
    forbidden = "import " + "scenic"
    assert forbidden not in Path(__file__).read_text(encoding="utf-8").lower()
