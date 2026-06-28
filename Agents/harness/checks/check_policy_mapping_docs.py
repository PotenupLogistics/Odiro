from __future__ import annotations

import json
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[2]
POLICY_CARDS_PATH = ROOT / "data" / "rag" / "policy_knowledge_cards.jsonl"
COVERAGE_DOC = ROOT / "docs" / "policy" / "POLICY_CARD_COVERAGE.md"
PARAMETER_DOC = ROOT / "docs" / "policy" / "POLICY_PARAMETER_CATALOG.md"
ACTION_DOC = ROOT / "docs" / "policy" / "DECISION_ACTION_MAPPING.md"
REQUEST_FIELD_DOC = ROOT / "docs" / "policy" / "DECISION_REQUEST_FIELD_MAPPING.md"
COVERAGE_REPORT_JSON = ROOT / "data" / "rag" / "policy_card_coverage_report.json"
COVERAGE_REPORT_MD = ROOT / "data" / "rag" / "policy_card_coverage_report.md"
EXPECTED_CARD_COUNT = 11


def _base_result() -> dict[str, Any]:
    return {
        "check": "policy_mapping_docs",
        "passed": False,
        "warning": False,
        "policyCardsExists": False,
        "cardCount": 0,
        "missingDocs": [],
        "missingReports": [],
        "docsWithoutCardIds": [],
        "actionListPresent": False,
        "parameterListPresent": False,
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
    for folder in ["data", "docs", "harness", "schemas", "scripts", "tests", "app"]:
        root = ROOT / folder
        if not root.exists():
            continue
        for path in root.rglob("*"):
            if path.is_file() and path.name in names:
                found.append(path.relative_to(ROOT).as_posix())
    return sorted(set(found))


def _read_cards() -> list[dict[str, Any]]:
    return [
        json.loads(line)
        for line in POLICY_CARDS_PATH.read_text(encoding="utf-8-sig").splitlines()
        if line.strip()
    ]


def run_check() -> dict[str, Any]:
    result = _base_result()
    if not POLICY_CARDS_PATH.exists():
        result["errors"].append("policy_knowledge_cards.jsonl does not exist.")
        return result

    result["policyCardsExists"] = True
    try:
        cards = _read_cards()
    except json.JSONDecodeError as exc:
        result["errors"].append(f"policy card JSONL parse failed: {exc}")
        return result

    result["cardCount"] = len(cards)
    if len(cards) != EXPECTED_CARD_COUNT:
        result["errors"].append(f"policy card count must be {EXPECTED_CARD_COUNT}.")

    docs = [COVERAGE_DOC, PARAMETER_DOC, ACTION_DOC, REQUEST_FIELD_DOC]
    reports = [COVERAGE_REPORT_JSON, COVERAGE_REPORT_MD]
    result["missingDocs"] = [path.relative_to(ROOT).as_posix() for path in docs if not path.exists()]
    result["missingReports"] = [path.relative_to(ROOT).as_posix() for path in reports if not path.exists()]

    for path in docs:
        if path.exists() and "CARD-KOR-003" not in path.read_text(encoding="utf-8-sig"):
            result["docsWithoutCardIds"].append(path.relative_to(ROOT).as_posix())

    if ACTION_DOC.exists():
        action_text = ACTION_DOC.read_text(encoding="utf-8-sig")
        result["actionListPresent"] = all(
            action in action_text for action in ["Continue", "SlowDown", "EmergencyStop", "RequestOperator"]
        )
    if PARAMETER_DOC.exists():
        parameter_text = PARAMETER_DOC.read_text(encoding="utf-8-sig")
        result["parameterListPresent"] = all(
            parameter in parameter_text
            for parameter in ["maxSpeedKmh", "emergencyStopDistanceCm", "traversabilityThreshold"]
        )

    if not result["actionListPresent"]:
        result["errors"].append("action mapping list is incomplete.")
    if not result["parameterListPresent"]:
        result["errors"].append("parameter catalog list is incomplete.")

    forbidden = _detect_forbidden_artifacts()
    result["generatedArtifactsWarnings"] = forbidden
    if forbidden:
        result["warnings"].append("Forbidden sample/schema artifacts detected.")

    result["passed"] = not any(
        [
            result["errors"],
            result["missingDocs"],
            result["missingReports"],
            result["docsWithoutCardIds"],
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
