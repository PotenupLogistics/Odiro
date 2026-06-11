from __future__ import annotations

from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def _read(path: Path) -> str:
    return path.read_text(encoding="utf-8-sig")


def test_research_alignment_doc_uses_public_source_links_without_required_pdfs() -> None:
    assert (ROOT / "docs" / "research" / "RESEARCH_ALIGNMENT.md").exists()
    text = _read(ROOT / "docs" / "research" / "RESEARCH_ALIGNMENT.md")

    assert "docs/references/EUREKA.pdf" not in text
    assert "docs/references/DREUREKA.pdf" not in text
    assert "https://arxiv.org/abs/2310.12931" in text
    assert "https://arxiv.org/abs/2406.01967" in text
    assert "https://eureka-research.github.io/" in text
    assert "https://eureka-research.github.io/dr-eureka/" in text


def test_research_alignment_harness_does_not_require_local_pdfs() -> None:
    text = _read(ROOT / "harness" / "checks" / "check_research_alignment_docs.py")

    assert "EUREKA = ROOT" not in text
    assert "DREUREKA = ROOT" not in text
    assert "eurekaPdfExists" not in text
    assert "drEurekaPdfExists" not in text


def test_research_alignment_covers_eureka_dreureka_and_scenic_boundaries() -> None:
    text = _read(ROOT / "docs" / "research" / "RESEARCH_ALIGNMENT.md")

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
