from __future__ import annotations

import json
from pathlib import Path
from typing import Any

from app.main import app


ROOT = Path(__file__).resolve().parents[2]
CARDS_PATH = ROOT / "data" / "rag" / "policy_knowledge_cards.jsonl"
CHUNKS_PATH = ROOT / "data" / "rag" / "policy_rag_chunks.jsonl"
REQUIRED_ROUTES = {
    "/api/v1/generation/world-config",
    "/api/v1/generation/world-config/prompt-package",
    "/api/v1/contracts/validate/{contract_type}",
}


def _base_result() -> dict[str, Any]:
    return {
        "check": "generation_endpoint",
        "passed": False,
        "warning": False,
        "missingRoutes": [],
        "policyCardCount": 0,
        "ragChunkCount": 0,
        "externalSdkImports": [],
        "hardcodedSecretWarnings": [],
        "generatedArtifactsWarnings": [],
        "warnings": [],
        "errors": [],
    }


def _scan_files() -> list[Path]:
    return [
        ROOT / "app" / "api" / "routes.py",
        ROOT / "app" / "services" / "world_config_generation_orchestrator.py",
    ]


def _detect_external_sdk_imports() -> list[str]:
    forbidden_terms = [
        "from openai",
        "import openai",
        "google.generativeai",
        "genai.",
        "anthropic",
        "import ollama",
        "from ollama",
        "OpenAI(",
        "AsyncOpenAI(",
        "chat.completions",
        "responses.create",
    ]
    found: list[str] = []
    for path in _scan_files():
        text = path.read_text(encoding="utf-8-sig")
        for term in forbidden_terms:
            if term in text:
                found.append(f"{path.relative_to(ROOT).as_posix()} contains {term}")
    return found


def _detect_hardcoded_secrets() -> list[str]:
    forbidden_terms = [
        "OPENAI_API_KEY",
        "GEMINI_API_KEY",
        "ANTHROPIC_API_KEY",
        "api_key=",
        "apiKey=",
        "secret=",
    ]
    found: list[str] = []
    for path in _scan_files():
        text = path.read_text(encoding="utf-8-sig")
        for term in forbidden_terms:
            if term in text:
                found.append(f"{path.relative_to(ROOT).as_posix()} contains {term}")
    return found


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
    names = {"world_config.json", "policy_config.json", "decision_request.json", "decision_response.json"}
    for folder in ["data", "docs", "app", "scripts", "harness", "tests"]:
        root = ROOT / folder
        if not root.exists():
            continue
        for path in root.rglob("*"):
            if path.is_file() and path.name in names:
                found.append(path.relative_to(ROOT).as_posix())
    return sorted(set(found))


def _jsonl_count(path: Path) -> int:
    if not path.exists():
        return 0
    return sum(1 for line in path.read_text(encoding="utf-8-sig").splitlines() if line.strip())


def run_check() -> dict[str, Any]:
    result = _base_result()
    route_paths = {route.path for route in app.routes}
    result["missingRoutes"] = sorted(REQUIRED_ROUTES - route_paths)
    if result["missingRoutes"]:
        result["errors"].append("Required generation endpoint routes are missing.")

    result["policyCardCount"] = _jsonl_count(CARDS_PATH)
    result["ragChunkCount"] = _jsonl_count(CHUNKS_PATH)
    if result["policyCardCount"] != 9:
        result["errors"].append("policy card count must remain 9.")
    if result["ragChunkCount"] != 9:
        result["errors"].append("policy RAG chunk count must remain 9.")

    sdk_imports = _detect_external_sdk_imports()
    result["externalSdkImports"] = sdk_imports
    if sdk_imports:
        result["errors"].append("External LLM SDK import or call code detected.")

    secret_warnings = _detect_hardcoded_secrets()
    result["hardcodedSecretWarnings"] = secret_warnings
    if secret_warnings:
        result["errors"].append("Potential hardcoded API key or secret detected.")

    forbidden = _detect_forbidden_artifacts()
    result["generatedArtifactsWarnings"] = forbidden
    if forbidden:
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
