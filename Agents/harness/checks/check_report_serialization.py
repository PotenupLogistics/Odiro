from __future__ import annotations

import json
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[2]
HELPER = ROOT / "app" / "utils" / "report_serialization.py"
GUIDE = ROOT / "docs" / "tooling" / "REPORT_SERIALIZATION_GUIDE.md"
SCRIPT_PATHS = [
    ROOT / "scripts" / "run_openai_world_config_smoke.py",
    ROOT / "scripts" / "run_ue5_handoff_smoke.py",
    ROOT / "scripts" / "run_ue5_episode_spec_controlled_smoke.py",
    ROOT / "scripts" / "export_ue5_handoff_payload.py",
    ROOT / "scripts" / "run_ollama_world_config_smoke.py",
]


def _detect_forbidden_artifacts() -> list[str]:
    found: list[str] = []
    for path in [
        ROOT / "samples",
        ROOT / "fixtures",
        ROOT / "data" / "rag" / "vector_db",
        ROOT / "data" / "rag" / "embeddings",
        ROOT / "data" / "rag" / "chroma",
        ROOT / "ue",
        ROOT / "UE",
    ]:
        if path.exists():
            found.append(path.relative_to(ROOT).as_posix())
    return sorted(found)


def run_check() -> dict[str, Any]:
    result: dict[str, Any] = {
        "check": "report_serialization",
        "passed": False,
        "warning": False,
        "helperExists": HELPER.exists(),
        "guideExists": GUIDE.exists(),
        "scriptsUseHelper": {},
        "hardcodedSecretWarnings": [],
        "forbiddenArtifacts": _detect_forbidden_artifacts(),
        "schemaDiffAbsent": True,
        "errors": [],
        "warnings": [],
    }
    if not HELPER.exists():
        result["errors"].append("app/utils/report_serialization.py is missing.")
    if not GUIDE.exists():
        result["errors"].append("docs/tooling/REPORT_SERIALIZATION_GUIDE.md is missing.")

    for path in SCRIPT_PATHS:
        text = path.read_text(encoding="utf-8-sig") if path.exists() else ""
        result["scriptsUseHelper"][path.relative_to(ROOT).as_posix()] = (
            "write_json_report" in text or "to_jsonable" in text
        )
    missing_helper = [path for path, uses in result["scriptsUseHelper"].items() if not uses]
    if missing_helper:
        result["errors"].append(f"Report-writing scripts missing helper usage: {missing_helper}")

    scan_paths = [HELPER, *SCRIPT_PATHS]
    for path in scan_paths:
        text = path.read_text(encoding="utf-8-sig") if path.exists() else ""
        for term in ["sk-", "api_key=\"", "api_key='"]:
            if term in text:
                result["hardcodedSecretWarnings"].append(f"{path.relative_to(ROOT).as_posix()} contains {term}")
    if result["hardcodedSecretWarnings"]:
        result["errors"].append("Potential hardcoded API key detected.")
    if result["forbiddenArtifacts"]:
        result["warnings"].append("Forbidden sample/fixture/vector/embedding/UE artifacts detected.")

    result["passed"] = not result["errors"]
    result["warning"] = result["passed"] and bool(result["warnings"])
    return result


def main() -> int:
    result = run_check()
    print(json.dumps(result, ensure_ascii=False, indent=2))
    return 0 if result["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
