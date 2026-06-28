from __future__ import annotations

import json
from pathlib import Path
from typing import Any

from app.models.rag import RagSearchQuery
from app.services.policy_rag_retriever import load_chunks, search_policy_chunks


ROOT = Path(__file__).resolve().parents[2]
CHUNKS_PATH = ROOT / "data" / "rag" / "policy_rag_chunks.jsonl"
EXPECTED_RAG_CHUNK_COUNT = 17
REQUIRED_FILES = {
    "app/services/policy_rag_retriever.py",
    "app/models/rag.py",
    "scripts/search_policy_rag.py",
    "docs/rag/RAG_RETRIEVAL_STRATEGY.md",
}


def _base_result() -> dict[str, Any]:
    return {
        "check": "rag_retrieval",
        "passed": False,
        "warning": False,
        "missingFiles": [],
        "chunkCount": 0,
        "keywordSearchResultCount": 0,
        "actionSearchResultCount": 0,
        "categorySearchResultCount": 0,
        "paramSearchResultCount": 0,
        "generatedArtifactsWarnings": [],
        "warnings": [],
        "errors": [],
    }


def _detect_forbidden_artifacts() -> list[str]:
    found: list[str] = []
    for path in [ROOT / "data" / "rag" / "embeddings", ROOT / "data" / "rag" / "vector_db", ROOT / "data" / "rag" / "chroma"]:
        if path.exists():
            found.append(path.relative_to(ROOT).as_posix())
    for pattern in ["*source*_chunks*.jsonl", "*processed*_chunks*.jsonl", "*.faiss", "*.chroma"]:
        found.extend(path.relative_to(ROOT).as_posix() for path in (ROOT / "data").rglob(pattern))
    names = {"world_config.json", "policy_config.json", "decision_request.json", "decision_response.json"}
    for folder in ["data", "docs", "harness", "scripts", "tests", "app"]:
        root = ROOT / folder
        if not root.exists():
            continue
        for path in root.rglob("*"):
            if path.is_file() and path.name in names:
                found.append(path.relative_to(ROOT).as_posix())
    if (ROOT / "samples").exists():
        found.append("samples/")
    if (ROOT / "fixtures").exists():
        found.append("fixtures/")
    return sorted(set(found))


def run_check() -> dict[str, Any]:
    result = _base_result()
    result["missingFiles"] = sorted(path for path in REQUIRED_FILES if not (ROOT / path).exists())
    if result["missingFiles"]:
        return result

    chunks = load_chunks(str(CHUNKS_PATH))
    result["chunkCount"] = len(chunks)
    if len(chunks) != EXPECTED_RAG_CHUNK_COUNT:
        result["errors"].append(f"policy_rag_chunks.jsonl must contain {EXPECTED_RAG_CHUNK_COUNT} chunks.")

    result["keywordSearchResultCount"] = len(search_policy_chunks(RagSearchQuery(query="비상정지")).results)
    result["actionSearchResultCount"] = len(
        search_policy_chunks(RagSearchQuery(query="", actionFilter=["EmergencyStop"])).results
    )
    result["categorySearchResultCount"] = len(
        search_policy_chunks(RagSearchQuery(query="", categoryFilter=["speed_policy"])).results
    )
    result["paramSearchResultCount"] = len(
        search_policy_chunks(RagSearchQuery(query="", policyParamFilter=["emergencyStopDistanceCm"])).results
    )
    if result["keywordSearchResultCount"] < 1:
        result["errors"].append("비상정지 search must return at least one result.")
    if result["actionSearchResultCount"] < 1:
        result["errors"].append("EmergencyStop action search must return at least one result.")
    if result["categorySearchResultCount"] < 1:
        result["errors"].append("speed_policy category search must return at least one result.")
    if result["paramSearchResultCount"] < 1:
        result["errors"].append("emergencyStopDistanceCm param search must return at least one result.")

    forbidden = _detect_forbidden_artifacts()
    result["generatedArtifactsWarnings"] = forbidden
    if forbidden:
        result["warnings"].append("Forbidden vector/embedding/sample/fixture artifacts detected.")

    result["passed"] = not any([result["errors"], result["missingFiles"]])
    result["warning"] = result["passed"] and bool(result["warnings"])
    return result


def main() -> int:
    result = run_check()
    print(json.dumps(result, ensure_ascii=False, indent=2))
    return 0 if result["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
