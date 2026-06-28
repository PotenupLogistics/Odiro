from __future__ import annotations

import json
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[2]
MODEL_PATH = ROOT / "app" / "models" / "handoff.py"
SERVICE_PATH = ROOT / "app" / "services" / "ue5_world_config_handoff_service.py"
ROUTES_PATH = ROOT / "app" / "api" / "routes.py"
DOC_PATH = ROOT / "docs" / "archive" / "previous_episode_spec" / "UE5_WORLD_CONFIG_HANDOFF.md"
SCRIPT_PATH = ROOT / "scripts" / "run_ue5_handoff_smoke.py"
POLICY_CARDS_PATH = ROOT / "data" / "rag" / "policy_knowledge_cards.jsonl"
RAG_CHUNKS_PATH = ROOT / "data" / "rag" / "policy_rag_chunks.jsonl"
EXPECTED_POLICY_CARD_COUNT = 11
EXPECTED_RAG_CHUNK_COUNT = 17


def _jsonl_count(path: Path) -> int:
    if not path.exists():
        return 0
    return sum(1 for line in path.read_text(encoding="utf-8-sig").splitlines() if line.strip())


def _detect_forbidden_artifacts() -> list[str]:
    found: list[str] = []
    for path in [
        ROOT / "samples",
        ROOT / "fixtures",
        ROOT / "data" / "rag" / "vector_db",
        ROOT / "data" / "rag" / "embeddings",
        ROOT / "data" / "rag" / "chroma",
    ]:
        if path.exists():
            found.append(path.relative_to(ROOT).as_posix())
    return sorted(set(found))


def _detect_forbidden_code() -> list[str]:
    terms = ["from openai", "import openai", "OpenAI(", "AsyncOpenAI(", "responses.create"]
    found: list[str] = []
    for path in [MODEL_PATH, SERVICE_PATH, ROUTES_PATH, SCRIPT_PATH]:
        if not path.exists():
            continue
        text = path.read_text(encoding="utf-8-sig")
        for term in terms:
            if term in text:
                found.append(f"{path.relative_to(ROOT).as_posix()} contains {term}")
    return found


def run_check() -> dict[str, Any]:
    result: dict[str, Any] = {
        "check": "ue5_handoff",
        "passed": False,
        "warning": False,
        "modelExists": MODEL_PATH.exists(),
        "serviceExists": SERVICE_PATH.exists(),
        "docExists": DOC_PATH.exists(),
        "scriptExists": SCRIPT_PATH.exists(),
        "legacyRouteRemoved": False,
        "policyCardCount": _jsonl_count(POLICY_CARDS_PATH),
        "ragChunkCount": _jsonl_count(RAG_CHUNKS_PATH),
        "openAiImports": [],
        "forbiddenArtifacts": [],
        "errors": [],
        "warnings": [],
    }

    if not result["modelExists"]:
        result["errors"].append("app/models/handoff.py is missing.")
    if not result["serviceExists"]:
        result["errors"].append("app/services/ue5_world_config_handoff_service.py is missing.")
    if not result["docExists"]:
        result["errors"].append("docs/archive/previous_episode_spec/UE5_WORLD_CONFIG_HANDOFF.md is missing.")
    if not result["scriptExists"]:
        result["errors"].append("scripts/run_ue5_handoff_smoke.py is missing.")

    try:
        from app.main import app

        route_paths = {route.path for route in app.routes}
        result["legacyRouteRemoved"] = "/api/v1/ue5/world-config/handoff" not in route_paths
    except Exception as exc:  # pragma: no cover
        result["errors"].append(f"Failed to import app.main:app: {exc}")

    if not result["legacyRouteRemoved"]:
        result["errors"].append("Legacy UE5 handoff route is still registered.")

    if result["policyCardCount"] != EXPECTED_POLICY_CARD_COUNT:
        result["errors"].append(f"Policy card count changed from {EXPECTED_POLICY_CARD_COUNT}.")
    if result["ragChunkCount"] != EXPECTED_RAG_CHUNK_COUNT:
        result["errors"].append(f"Policy RAG chunk count changed from {EXPECTED_RAG_CHUNK_COUNT}.")

    result["openAiImports"] = _detect_forbidden_code()
    if result["openAiImports"]:
        result["errors"].append("OpenAI SDK import or call code detected.")

    result["forbiddenArtifacts"] = _detect_forbidden_artifacts()
    if result["forbiddenArtifacts"]:
        result["warnings"].append("Forbidden sample/fixture/vector/embedding artifacts detected.")

    result["passed"] = not result["errors"]
    result["warning"] = result["passed"] and bool(result["warnings"])
    return result


def main() -> int:
    result = run_check()
    print(json.dumps(result, ensure_ascii=False, indent=2))
    return 0 if result["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
