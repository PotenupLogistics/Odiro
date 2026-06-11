from __future__ import annotations

import ast
import json
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[2]
DOC = ROOT / "docs" / "research" / "RESEARCH_ALIGNMENT.md"
SOURCE_REGISTRY = ROOT / "docs" / "research" / "RESEARCH_SOURCE_REGISTRY.md"
README = ROOT / "README.md"
DOCS_README = ROOT / "docs" / "README.md"
PYPROJECT = ROOT / "pyproject.toml"

FORBIDDEN_ARTIFACTS = [
    ROOT / "samples",
    ROOT / "fixtures",
    ROOT / "data" / "rag" / "vector_db",
    ROOT / "data" / "rag" / "embeddings",
    ROOT / "data" / "rag" / "chroma",
    ROOT / "ue",
    ROOT / "UE",
]


def _read(path: Path) -> str:
    return path.read_text(encoding="utf-8-sig") if path.exists() else ""


def _imports_live_http_client(path: Path) -> bool:
    tree = ast.parse(path.read_text(encoding="utf-8"))
    for node in ast.walk(tree):
        if isinstance(node, ast.Import):
            if any(alias.name in {"urllib", "urllib.request", "requests", "httpx"} for alias in node.names):
                return True
        if isinstance(node, ast.ImportFrom) and node.module in {"urllib", "urllib.request", "requests", "httpx"}:
            return True
    return False


def run_check() -> dict[str, Any]:
    doc_text = _read(DOC)
    source_registry_text = _read(SOURCE_REGISTRY)
    readme_text = _read(README) + "\n" + _read(DOCS_README)
    pyproject_text = _read(PYPROJECT).lower()
    forbidden = [path.relative_to(ROOT).as_posix() for path in FORBIDDEN_ARTIFACTS if path.exists()]
    result: dict[str, Any] = {
        "check": "research_alignment_docs",
        "passed": False,
        "warning": False,
        "docExists": DOC.exists(),
        "sourceRegistryExists": SOURCE_REGISTRY.exists(),
        "usesPublicEurekaLinks": all(
            term in doc_text
            for term in [
                "https://eureka-research.github.io/",
                "https://arxiv.org/abs/2310.12931",
            ]
        ),
        "usesPublicDrEurekaLinks": all(
            term in doc_text
            for term in [
                "https://eureka-research.github.io/dr-eureka/",
                "https://arxiv.org/abs/2406.01967",
            ]
        ),
        "doesNotRequireLocalResearchPdfs": "docs/references/EUREKA.pdf" not in doc_text
        and "docs/references/DREUREKA.pdf" not in doc_text,
        "sourceRegistryIncludesResearchSources": all(
            term in source_registry_text
            for term in [
                "RSR-005",
                "RSR-006",
                "https://eureka-research.github.io/",
                "https://eureka-research.github.io/dr-eureka/",
            ]
        ),
        "mentionsEureka": "Eureka" in doc_text,
        "mentionsDrEureka": "DrEureka" in doc_text,
        "mentionsScenic": "Scenic" in doc_text,
        "mentionsDrEurekaInspiredSampling": "DrEureka-inspired environment sampling" in doc_text,
        "mentionsScenicInspiredDirection": "Scenic-inspired placement" in doc_text
        or "Scenic-inspired scenario sampling" in doc_text,
        "statesNotDrEurekaFullImplementation": "DrEureka 전체 구현이 아니다" in doc_text
        or "DrEureka 전체 구현으로 단정" in doc_text,
        "statesNotUsingScenicDsl": "Scenic DSL을 사용하고 있지 않다" in doc_text
        or "not Scenic DSL" in doc_text
        or "Scenic DSL이 아니라 WorldConfig/EpisodeSpec 기반" in doc_text,
        "readmesLinkResearchAlignment": "RESEARCH_ALIGNMENT.md" in readme_text,
        "noScenicRuntimeDependency": "scenic" not in pyproject_text,
        "noLiveProviderCallsInHarness": not _imports_live_http_client(Path(__file__)),
        "forbiddenArtifacts": forbidden,
        "errors": [],
        "warnings": [],
    }
    for key, message in [
        ("docExists", "docs/research/RESEARCH_ALIGNMENT.md is missing."),
        ("sourceRegistryExists", "docs/research/RESEARCH_SOURCE_REGISTRY.md is missing."),
        ("usesPublicEurekaLinks", "Research alignment must link Eureka public project/arXiv sources."),
        ("usesPublicDrEurekaLinks", "Research alignment must link DrEureka public project/arXiv sources."),
        ("doesNotRequireLocalResearchPdfs", "Research alignment must not require local Eureka/DrEureka PDFs."),
        ("sourceRegistryIncludesResearchSources", "Research source registry must include Eureka and DrEureka source entries."),
        ("mentionsEureka", "Research alignment must mention Eureka."),
        ("mentionsDrEureka", "Research alignment must mention DrEureka."),
        ("mentionsScenic", "Research alignment must mention Scenic."),
        ("mentionsDrEurekaInspiredSampling", "Research alignment must mention DrEureka-inspired environment sampling."),
        ("mentionsScenicInspiredDirection", "Research alignment must mention Scenic-inspired placement/scenario sampling direction."),
        ("statesNotDrEurekaFullImplementation", "Research alignment must state Proto-AI is not a DrEureka full implementation."),
        ("statesNotUsingScenicDsl", "Research alignment must state Proto-AI is not currently using Scenic DSL."),
        ("readmesLinkResearchAlignment", "README or docs README must link RESEARCH_ALIGNMENT.md."),
        ("noScenicRuntimeDependency", "Scenic runtime dependency must not be added."),
        ("noLiveProviderCallsInHarness", "Harness check must not perform live provider calls."),
    ]:
        if not result[key]:
            result["errors"].append(message)
    if forbidden:
        result["warnings"].append("Forbidden sample/fixture/vector/embedding/UE artifacts detected.")
    result["passed"] = not result["errors"]
    result["warning"] = result["passed"] and bool(result["warnings"])
    return result


def main() -> int:
    result = run_check()
    print(json.dumps(result, ensure_ascii=False, indent=2))
    return 0 if result["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
