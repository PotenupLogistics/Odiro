from __future__ import annotations

import json
from pathlib import Path
from typing import Any

from app.models.llm import LlmProvider
from app.services.llm_client_factory import create_llm_client
from app.services.llm_ollama_client import OllamaLlmClient


ROOT = Path(__file__).resolve().parents[2]
REQUIRED_FILES = {
    "app/services/llm_ollama_client.py",
    "docs/OLLAMA_PROVIDER_GUIDE.md",
}


def _base_result() -> dict[str, Any]:
    return {
        "check": "ollama_provider",
        "passed": False,
        "warning": False,
        "missingFiles": [],
        "factoryReturnsOllamaClient": False,
        "externalOpenAiImports": [],
        "hardcodedSecretWarnings": [],
        "generatedArtifactsWarnings": [],
        "warnings": [],
        "errors": [],
    }


def _scan_files() -> list[Path]:
    return [
        ROOT / "app" / "services" / "llm_ollama_client.py",
        ROOT / "app" / "services" / "llm_client_factory.py",
    ]


def _detect_openai_code() -> list[str]:
    forbidden_terms = [
        "from openai",
        "import openai",
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
    forbidden_terms = ["OPENAI_API_KEY=", "sk-", "AIza", "api_key=", "apiKey=", "secret="]
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

    result["factoryReturnsOllamaClient"] = isinstance(
        create_llm_client(LlmProvider.ollama),
        OllamaLlmClient,
    )
    if not result["factoryReturnsOllamaClient"]:
        result["errors"].append("factory must return OllamaLlmClient for provider=ollama.")

    openai_code = _detect_openai_code()
    result["externalOpenAiImports"] = openai_code
    if openai_code:
        result["errors"].append("OpenAI SDK import or call code detected.")

    hardcoded = _detect_hardcoded_secrets()
    result["hardcodedSecretWarnings"] = hardcoded
    if hardcoded:
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
