from __future__ import annotations

import json
import subprocess
import sys
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[2]
SCRIPT_PATH = ROOT / "scripts" / "run_ollama_world_config_smoke.py"
GUIDE_PATH = ROOT / "docs" / "OLLAMA_LIVE_SMOKE_GUIDE.md"


def _base_result() -> dict[str, Any]:
    return {
        "check": "ollama_live_smoke_tooling",
        "passed": False,
        "warning": False,
        "scriptExists": False,
        "guideExists": False,
        "helpWorks": False,
        "dryRunWorks": False,
        "openAiImports": [],
        "hardcodedSecretWarnings": [],
        "forbiddenArtifacts": [],
        "warnings": [],
        "errors": [],
    }


def _run_script(*args: str) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        [sys.executable, str(SCRIPT_PATH), *args],
        cwd=ROOT,
        text=True,
        capture_output=True,
        timeout=60,
    )


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
    if not SCRIPT_PATH.exists():
        return found
    text = SCRIPT_PATH.read_text(encoding="utf-8-sig")
    for term in forbidden_terms:
        if term in text:
            found.append(f"{SCRIPT_PATH.relative_to(ROOT).as_posix()} contains {term}")
    return found


def _detect_hardcoded_secrets() -> list[str]:
    forbidden_terms = ["OPENAI_API_KEY=", "sk-", "AIza", "api_key=", "apiKey=", "secret="]
    found: list[str] = []
    if not SCRIPT_PATH.exists():
        return found
    text = SCRIPT_PATH.read_text(encoding="utf-8-sig")
    for term in forbidden_terms:
        if term in text:
            found.append(f"{SCRIPT_PATH.relative_to(ROOT).as_posix()} contains {term}")
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
    return sorted(set(found))


def run_check() -> dict[str, Any]:
    result = _base_result()
    result["scriptExists"] = SCRIPT_PATH.exists()
    result["guideExists"] = GUIDE_PATH.exists()

    if not result["scriptExists"]:
        result["errors"].append("scripts/run_ollama_world_config_smoke.py is missing.")
        return result
    if not result["guideExists"]:
        result["errors"].append("docs/OLLAMA_LIVE_SMOKE_GUIDE.md is missing.")

    help_result = _run_script("--help")
    result["helpWorks"] = help_result.returncode == 0 and "--dry-run" in help_result.stdout
    if not result["helpWorks"]:
        result["errors"].append("Ollama smoke runner --help failed.")

    dry_run = _run_script("--prompt", "narrow sidewalk with blocked path", "--dry-run")
    result["dryRunWorks"] = (
        dry_run.returncode == 0
        and "DRY RUN" in dry_run.stdout
        and "retrievedContexts" in dry_run.stdout
        and "ollama_connection_failed" not in dry_run.stdout
    )
    if not result["dryRunWorks"]:
        result["errors"].append("Ollama smoke runner --dry-run failed or attempted a live call.")

    result["openAiImports"] = _detect_openai_code()
    if result["openAiImports"]:
        result["errors"].append("OpenAI SDK import or call code detected in smoke runner.")

    result["hardcodedSecretWarnings"] = _detect_hardcoded_secrets()
    if result["hardcodedSecretWarnings"]:
        result["errors"].append("Potential hardcoded API key or secret detected in smoke runner.")

    result["forbiddenArtifacts"] = _detect_forbidden_artifacts()
    if result["forbiddenArtifacts"]:
        result["warnings"].append("Forbidden sample/fixture/vector/embedding artifacts detected.")

    result["passed"] = not bool(result["errors"])
    result["warning"] = result["passed"] and bool(result["warnings"])
    return result


def main() -> int:
    result = run_check()
    print(json.dumps(result, ensure_ascii=False, indent=2))
    return 0 if result["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
