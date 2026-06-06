from __future__ import annotations

import json
from pathlib import Path
from typing import Any

from app.main import app


ROOT = Path(__file__).resolve().parents[2]
REQUIRED_FILES = {
    "app/main.py",
    "app/api/routes.py",
    "docs/tooling/API_SHELL_GUIDE.md",
}
REQUIRED_ROUTES = {
    "/health",
    "/api/v1/scenarios/generate",
    "/api/v1/scenarios/generate-artifacts",
    "/api/v1/scenarios/generate-drive",
}
FORBIDDEN_API_V1_ROUTES = {
    "/api/v1/generation/world-config",
    "/api/v1/generation/world-config/prompt-package",
    "/api/v1/contracts/validate/{contract_type}",
    "/api/v1/ue5/world-config/handoff",
}


def _base_result() -> dict[str, Any]:
    return {
        "check": "api_shell",
        "passed": False,
        "warning": False,
        "missingFiles": [],
        "missingRoutes": [],
        "forbiddenRoutes": [],
        "llmCallWarnings": [],
        "generatedArtifactsWarnings": [],
        "warnings": [],
        "errors": [],
    }


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


def _detect_llm_call_code() -> list[str]:
    warnings: list[str] = []
    scanned_files = [ROOT / "app" / "main.py", ROOT / "app" / "api" / "routes.py"]
    forbidden_terms = ["OpenAI(", "AsyncOpenAI(", "google.generativeai", "genai.", "chat.completions", "responses.create"]
    for path in scanned_files:
        if not path.exists():
            continue
        text = path.read_text(encoding="utf-8-sig")
        for term in forbidden_terms:
            if term in text:
                warnings.append(f"{path.relative_to(ROOT).as_posix()} contains {term}")
    return warnings


def run_check() -> dict[str, Any]:
    result = _base_result()
    result["missingFiles"] = sorted(path for path in REQUIRED_FILES if not (ROOT / path).exists())
    if result["missingFiles"]:
        return result

    route_paths = {route.path for route in app.routes}
    result["missingRoutes"] = sorted(REQUIRED_ROUTES - route_paths)
    if result["missingRoutes"]:
        result["errors"].append("Required API shell routes are missing.")
    result["forbiddenRoutes"] = sorted(FORBIDDEN_API_V1_ROUTES & route_paths)
    if result["forbiddenRoutes"]:
        result["errors"].append("Removed API v1 routes are still registered.")

    llm_warnings = _detect_llm_call_code()
    result["llmCallWarnings"] = llm_warnings
    if llm_warnings:
        result["errors"].append("External LLM API call code detected in API shell.")

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
