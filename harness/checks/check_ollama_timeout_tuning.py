from __future__ import annotations

import json
import subprocess
import sys
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[2]
SCRIPT_PATH = ROOT / "scripts" / "run_ollama_world_config_smoke.py"
GUIDE_PATH = ROOT / "docs" / "providers" / "OLLAMA_TIMEOUT_TUNING_GUIDE.md"


def _base_result() -> dict[str, Any]:
    return {
        "check": "ollama_timeout_tuning",
        "passed": False,
        "warning": False,
        "guideExists": False,
        "scriptOptionsPresent": False,
        "openAiImports": [],
        "forbiddenArtifacts": [],
        "warnings": [],
        "errors": [],
    }


def _help_text() -> str:
    completed = subprocess.run(
        [sys.executable, str(SCRIPT_PATH), "--help"],
        cwd=ROOT,
        text=True,
        capture_output=True,
        timeout=60,
    )
    return completed.stdout + completed.stderr


def _detect_openai_code() -> list[str]:
    forbidden_terms = ["from openai", "import openai", "OpenAI(", "AsyncOpenAI(", "responses.create"]
    found: list[str] = []
    for path in [
        SCRIPT_PATH,
        ROOT / "app" / "services" / "llm_ollama_client.py",
        ROOT / "app" / "services" / "world_config_generation_orchestrator.py",
    ]:
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
    return sorted(set(found))


def run_check() -> dict[str, Any]:
    result = _base_result()
    result["guideExists"] = GUIDE_PATH.exists()
    if not result["guideExists"]:
        result["errors"].append("docs/providers/OLLAMA_TIMEOUT_TUNING_GUIDE.md is missing.")

    help_text = _help_text() if SCRIPT_PATH.exists() else ""
    required_options = ["--timeout-sec", "--warm-up", "--compact-prompt", "--context-top-k"]
    result["scriptOptionsPresent"] = all(option in help_text for option in required_options)
    if not result["scriptOptionsPresent"]:
        result["errors"].append("Timeout tuning CLI options are missing.")

    result["openAiImports"] = _detect_openai_code()
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
