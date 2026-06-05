from __future__ import annotations

import json
from pathlib import Path
from typing import Any

from app.main import app
from app.models.llm import LlmProvider
from app.services.llm_client_factory import create_llm_client
from app.services.llm_disabled_client import DisabledLlmClient


ROOT = Path(__file__).resolve().parents[2]
REQUIRED_FILES = {
    "app/models/llm.py",
    "app/services/llm_client.py",
    "app/services/llm_disabled_client.py",
    "app/services/llm_client_factory.py",
    "docs/providers/LLM_CLIENT_ABSTRACTION.md",
}
EXPECTED_ROUTES = {
    "/health",
    "/api/v1/scenarios/generate",
}


def _base_result() -> dict[str, Any]:
    return {
        "check": "llm_client_abstraction",
        "passed": False,
        "warning": False,
        "missingFiles": [],
        "providers": [],
        "routeCountUnchanged": False,
        "externalSdkImports": [],
        "hardcodedSecretWarnings": [],
        "generatedArtifactsWarnings": [],
        "warnings": [],
        "errors": [],
    }


def _scan_files() -> list[Path]:
    return [
        ROOT / "app" / "models" / "llm.py",
        ROOT / "app" / "services" / "llm_client.py",
        ROOT / "app" / "services" / "llm_disabled_client.py",
        ROOT / "app" / "services" / "llm_client_factory.py",
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
        if not path.exists():
            continue
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
        if not path.exists():
            continue
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


def run_check() -> dict[str, Any]:
    result = _base_result()
    result["missingFiles"] = sorted(path for path in REQUIRED_FILES if not (ROOT / path).exists())
    if result["missingFiles"]:
        return result

    providers = {provider.value for provider in LlmProvider}
    result["providers"] = sorted(providers)
    expected_providers = {"disabled", "openai", "gemini", "ollama", "custom"}
    missing_providers = expected_providers - providers
    if missing_providers:
        result["errors"].append(f"Missing LLM providers: {sorted(missing_providers)}")

    if not isinstance(create_llm_client(LlmProvider.disabled), DisabledLlmClient):
        result["errors"].append("disabled provider must return DisabledLlmClient.")

    route_paths = {route.path for route in app.routes}
    result["routeCountUnchanged"] = (
        "/api/v1/ue5/world-config/handoff" not in route_paths
        and EXPECTED_ROUTES.issubset(route_paths)
        and len(
        {path for path in route_paths if path.startswith("/api/v1/") or path == "/health"}
        ) == len(EXPECTED_ROUTES)
    )
    if not result["routeCountUnchanged"]:
        result["errors"].append("FastAPI public API v1 set must expose only scenario generation.")

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

    result["passed"] = not any([result["errors"], result["missingFiles"]])
    result["warning"] = result["passed"] and bool(result["warnings"])
    return result


def main() -> int:
    result = run_check()
    print(json.dumps(result, ensure_ascii=False, indent=2))
    return 0 if result["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
