from __future__ import annotations

import ast
import json
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[2]
RESULT_DOC = ROOT / "docs" / "handoff" / "HANDOFF_RELEASE_NOTES.md"
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
    result_text = _read(RESULT_DOC)
    readme_text = _read(README)
    docs_readme_text = _read(DOCS_README)
    forbidden = [path.relative_to(ROOT).as_posix() for path in FORBIDDEN_ARTIFACTS if path.exists()]
    result: dict[str, Any] = {
        "check": "environment_sampling_handoff_result_docs",
        "passed": False,
        "warning": False,
        "resultDocExists": RESULT_DOC.exists(),
        "rootReadmeLinksResult": "HANDOFF_RELEASE_NOTES.md" in readme_text,
        "docsReadmeLinksResult": "HANDOFF_RELEASE_NOTES.md" in docs_readme_text,
        "documentsSidewalkWidth": "sidewalkWidthCm=120" in result_text,
        "documentsBlockingRatio": "obstacleBlockingRatio=0.6" in result_text,
        "documentsTimeLimit": "timeLimitSec=60" in result_text,
        "documentsDoeDeferred": "DOE matrix generation" in result_text and "batch scenario generation" in result_text,
        "noLiveProviderCallsInHarness": not _imports_live_http_client(Path(__file__)),
        "forbiddenArtifacts": forbidden,
        "errors": [],
        "warnings": [],
    }

    for key, message in [
        ("resultDocExists", "docs/handoff/HANDOFF_RELEASE_NOTES.md is missing."),
        ("rootReadmeLinksResult", "README.md must link HANDOFF_RELEASE_NOTES.md."),
        ("docsReadmeLinksResult", "docs/README.md must link HANDOFF_RELEASE_NOTES.md."),
        ("documentsSidewalkWidth", "Result doc must include sidewalkWidthCm=120."),
        ("documentsBlockingRatio", "Result doc must include obstacleBlockingRatio=0.6."),
        ("documentsTimeLimit", "Result doc must include timeLimitSec=60."),
        ("documentsDoeDeferred", "Result doc must state DOE/batch generation is deferred."),
        ("noLiveProviderCallsInHarness", "Harness check must not perform live OpenAI/Ollama calls."),
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
