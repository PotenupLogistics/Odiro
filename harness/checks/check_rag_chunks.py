from __future__ import annotations

import json
from collections import Counter
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[2]
CARDS_PATH = ROOT / "data" / "rag" / "policy_knowledge_cards.jsonl"
CHUNKS_PATH = ROOT / "data" / "rag" / "policy_rag_chunks.jsonl"
REQUIRED_METADATA = {
    "sourceIds",
    "category",
    "relatedPolicyParams",
    "relatedRequestFields",
    "relatedActions",
    "relatedMetrics",
    "evidenceLocation",
    "createdFromCandidateId",
    "status",
}


def _base_result() -> dict[str, Any]:
    return {
        "check": "rag_chunks",
        "passed": False,
        "warning": False,
        "policyCardsExists": False,
        "ragChunksExists": False,
        "cardCount": 0,
        "chunkCount": 0,
        "duplicateChunkIds": [],
        "emptyChunkText": [],
        "missingMetadata": [],
        "invalidStatuses": [],
        "nonConfirmedCandidateChunks": [],
        "generatedArtifactsWarnings": [],
        "warnings": [],
        "errors": [],
    }


def _read_jsonl(path: Path) -> list[dict[str, Any]]:
    return [json.loads(line) for line in path.read_text(encoding="utf-8-sig").splitlines() if line.strip()]


def _detect_forbidden_artifacts() -> list[str]:
    found: list[str] = []
    for pattern in ["*source*_chunks*.jsonl", "*processed*_chunks*.jsonl", "*.chroma", "*.faiss"]:
        found.extend(path.relative_to(ROOT).as_posix() for path in (ROOT / "data").rglob(pattern))
    for path in [ROOT / "data" / "rag" / "embeddings", ROOT / "data" / "rag" / "vector_db", ROOT / "data" / "rag" / "chroma"]:
        if path.exists():
            found.append(path.relative_to(ROOT).as_posix())
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
    result["policyCardsExists"] = CARDS_PATH.exists()
    result["ragChunksExists"] = CHUNKS_PATH.exists()
    if not result["policyCardsExists"]:
        result["errors"].append("policy_knowledge_cards.jsonl does not exist.")
        return result
    if not result["ragChunksExists"]:
        result["errors"].append("policy_rag_chunks.jsonl does not exist.")
        return result

    cards = _read_jsonl(CARDS_PATH)
    chunks = _read_jsonl(CHUNKS_PATH)
    result["cardCount"] = len(cards)
    result["chunkCount"] = len(chunks)
    if len(cards) != len(chunks):
        result["errors"].append("policy card count and rag chunk count must match.")

    confirmed_card_ids = {card["cardId"] for card in cards}
    counts = Counter(chunk.get("chunkId") for chunk in chunks)
    result["duplicateChunkIds"] = sorted(chunk_id for chunk_id, count in counts.items() if count > 1)

    for chunk in chunks:
        chunk_id = chunk.get("chunkId", "<unknown>")
        if not str(chunk.get("chunkText", "")).strip():
            result["emptyChunkText"].append(chunk_id)
        metadata = chunk.get("metadata")
        if not isinstance(metadata, dict):
            result["missingMetadata"].append({"chunkId": chunk_id, "missing": sorted(REQUIRED_METADATA)})
            continue
        missing = sorted(REQUIRED_METADATA - set(metadata))
        if missing:
            result["missingMetadata"].append({"chunkId": chunk_id, "missing": missing})
        if metadata.get("status") != "confirmed_policy_card":
            result["invalidStatuses"].append({"chunkId": chunk_id, "status": metadata.get("status")})
        if chunk.get("cardId") not in confirmed_card_ids:
            result["nonConfirmedCandidateChunks"].append(chunk_id)

    forbidden = _detect_forbidden_artifacts()
    result["generatedArtifactsWarnings"] = forbidden
    if forbidden:
        result["warnings"].append("Forbidden chunk/vector/sample/fixture artifacts detected.")

    result["passed"] = not any(
        [
            result["errors"],
            result["duplicateChunkIds"],
            result["emptyChunkText"],
            result["missingMetadata"],
            result["invalidStatuses"],
            result["nonConfirmedCandidateChunks"],
        ]
    )
    result["warning"] = result["passed"] and bool(result["warnings"])
    return result


def main() -> int:
    result = run_check()
    print(json.dumps(result, ensure_ascii=False, indent=2))
    return 0 if result["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
