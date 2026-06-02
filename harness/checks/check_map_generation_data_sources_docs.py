from __future__ import annotations

import ast
import json
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[2]
DATA_SOURCES = ROOT / "docs" / "MAP_GENERATION_DATA_SOURCES.md"
README = ROOT / "README.md"
DOCS_README = ROOT / "docs" / "README.md"

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
    data_text = _read(DATA_SOURCES)
    readme_text = _read(README) + "\n" + _read(DOCS_README)
    forbidden = [path.relative_to(ROOT).as_posix() for path in FORBIDDEN_ARTIFACTS if path.exists()]
    result: dict[str, Any] = {
        "check": "map_generation_data_sources_docs",
        "passed": False,
        "warning": False,
        "dataSourcesExists": DATA_SOURCES.exists(),
        "readmesLinkDataSources": "MAP_GENERATION_DATA_SOURCES.md" in readme_text,
        "dataSourcesSeparatesPolicyRagFromCoordinates": "좌표 생성 근거가 아니라" in data_text and "법령 RAG" in data_text,
        "dataSourcesExplainCoordinateBasis": all(
            term in data_text
            for term in ["사용자가 명시한 좌표", "robot.spawn / robot.goal", "route midpoint rule", "environmentSampling"]
        ),
        "dataSourcesMentionGenerationTrace": all(
            term in data_text
            for term in ["diagnostics.generationTrace", "full WorldConfig", "full EpisodeSpec", "rawContent"]
        ),
        "noLiveProviderCallsInHarness": not _imports_live_http_client(Path(__file__)),
        "forbiddenArtifacts": forbidden,
        "errors": [],
        "warnings": [],
    }

    for key, message in [
        ("dataSourcesExists", "docs/MAP_GENERATION_DATA_SOURCES.md is missing."),
        ("readmesLinkDataSources", "README or docs README must link the data sources doc."),
        ("dataSourcesSeparatesPolicyRagFromCoordinates", "Data source docs must state Policy RAG is not a coordinate-generation source."),
        ("dataSourcesExplainCoordinateBasis", "Data source docs must explain coordinate decision inputs."),
        ("dataSourcesMentionGenerationTrace", "Data source docs must mention generationTrace and no full payload storage."),
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
