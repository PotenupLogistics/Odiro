from __future__ import annotations

import json
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[2]
POLICY_CARDS_PATH = ROOT / "data" / "rag" / "policy_knowledge_cards.jsonl"
EXPECTED_DOCS = [
    ROOT / "docs" / "json_contracts" / "NATURAL_LANGUAGE_INPUT_PLAN.md",
    ROOT / "docs" / "architecture" / "LLM_WORLD_CONFIG_GENERATION_FLOW.md",
    ROOT / "docs" / "architecture" / "WORLD_CONFIG_PROMPT_SPEC.md",
    ROOT / "docs" / "json_contracts" / "NATURAL_LANGUAGE_GENERATION_CONTRACT.md",
]


def _base_result() -> dict[str, Any]:
    return {
        "check": "natural_language_plan",
        "passed": False,
        "warning": False,
        "missingDocs": [],
        "docsMissingWorldConfig": [],
        "policyCardCount": 0,
        "generatedArtifactsWarnings": [],
        "warnings": [],
        "errors": [],
    }


def _detect_forbidden_artifacts() -> list[str]:
    names = {
        "world_config.json",
        "policy_config.json",
        "decision_request.json",
        "decision_response.json",
    }
    found: list[str] = []
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
    for doc in EXPECTED_DOCS:
        if not doc.exists():
            result["missingDocs"].append(doc.relative_to(ROOT).as_posix())
            continue
        text = doc.read_text(encoding="utf-8-sig")
        if "World Config" not in text and "world_config" not in text:
            result["docsMissingWorldConfig"].append(doc.relative_to(ROOT).as_posix())

    if POLICY_CARDS_PATH.exists():
        result["policyCardCount"] = sum(
            1 for line in POLICY_CARDS_PATH.read_text(encoding="utf-8-sig").splitlines() if line.strip()
        )
    if result["policyCardCount"] != 9:
        result["errors"].append("policy card count must remain 9.")

    forbidden = _detect_forbidden_artifacts()
    result["generatedArtifactsWarnings"] = forbidden
    if forbidden:
        result["warnings"].append("Forbidden sample/fixture artifacts detected.")

    result["passed"] = not any(
        [
            result["errors"],
            result["missingDocs"],
            result["docsMissingWorldConfig"],
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
