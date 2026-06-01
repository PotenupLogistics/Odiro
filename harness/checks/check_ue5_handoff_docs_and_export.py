from __future__ import annotations

import json
import subprocess
import sys
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[2]
DOCS = [
    ROOT / "docs" / "UE5_WORLD_CONFIG_FIELD_MAPPING.md",
    ROOT / "docs" / "UE5_CONTROLLED_INTEGRATION_TEST_PLAN.md",
    ROOT / "docs" / "UE5_WORLD_CONFIG_PARSER_PSEUDOCODE.md",
    ROOT / "docs" / "UE5_HANDOFF_ACCEPTANCE_CHECKLIST.md",
]
SCRIPT_PATH = ROOT / "scripts" / "export_ue5_handoff_payload.py"


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


def _detect_openai_code() -> list[str]:
    terms = ["from openai", "import openai", "OpenAI(", "AsyncOpenAI(", "responses.create"]
    found: list[str] = []
    for path in [SCRIPT_PATH, *DOCS]:
        if not path.exists():
            continue
        text = path.read_text(encoding="utf-8-sig")
        for term in terms:
            if term in text:
                found.append(f"{path.relative_to(ROOT).as_posix()} contains {term}")
    return found


def _help_works() -> bool:
    if not SCRIPT_PATH.exists():
        return False
    completed = subprocess.run(
        [sys.executable, str(SCRIPT_PATH), "--help"],
        cwd=ROOT,
        text=True,
        capture_output=True,
        check=False,
    )
    return completed.returncode == 0 and "--out" in completed.stdout and "--format" in completed.stdout


def run_check() -> dict[str, Any]:
    missing_docs = [path.relative_to(ROOT).as_posix() for path in DOCS if not path.exists()]
    result: dict[str, Any] = {
        "check": "ue5_handoff_docs_and_export",
        "passed": False,
        "warning": False,
        "missingDocs": missing_docs,
        "scriptExists": SCRIPT_PATH.exists(),
        "cliHelpWorks": _help_works(),
        "openAiImports": [],
        "forbiddenArtifacts": [],
        "errors": [],
        "warnings": [],
    }

    if missing_docs:
        result["errors"].append("UE5 handoff implementation docs are missing.")
    if not result["scriptExists"]:
        result["errors"].append("scripts/export_ue5_handoff_payload.py is missing.")
    if not result["cliHelpWorks"]:
        result["errors"].append("export CLI --help does not work.")

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
