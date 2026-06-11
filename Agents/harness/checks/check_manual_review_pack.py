from __future__ import annotations

import json
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[2]
PACK_DIR = ROOT / "data" / "sources" / "review" / "manual_review_pack"
EXECUTION_PLAN_PATH = ROOT / "docs" / "manual_review" / "MANUAL_REVIEW_EXECUTION_PLAN.md"
INPUT_GUIDE_PATH = ROOT / "docs" / "manual_review" / "MANUAL_CONFIRMATION_INPUT_GUIDE.md"
MANUAL_CONFIRMATION_PATH = ROOT / "data" / "sources" / "review" / "confirmed" / "manual_confirmation_results.json"
POLICY_CARD_PATH = ROOT / "data" / "rag" / "policy_knowledge_cards.jsonl"
EXPECTED_PACK_FILES = {
    "KOR-001_review_pack.md",
    "KOR-002_review_pack.md",
    "KOR-003_review_pack.md",
    "KOR-004_review_pack.md",
    "KOR-005_review_pack.md",
}


def _base_result() -> dict[str, Any]:
    return {
        "check": "manual_review_pack",
        "passed": False,
        "warning": False,
        "packDirExists": False,
        "sourcePackFilesExist": {},
        "missingSourcePackFiles": [],
        "executionPlanExists": False,
        "inputGuideExists": False,
        "pendingCount": 0,
        "confirmedCount": 0,
        "rejectedCount": 0,
        "manualConfirmationModified": False,
        "policyCardPresent": False,
        "policyCardHasContent": False,
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


def run_check() -> dict[str, Any]:
    result = _base_result()
    result["packDirExists"] = PACK_DIR.exists()
    if not result["packDirExists"]:
        result["errors"].append("manual_review_pack directory does not exist.")

    existing_files = {path.name for path in PACK_DIR.glob("KOR-*_review_pack.md")} if PACK_DIR.exists() else set()
    for filename in sorted(EXPECTED_PACK_FILES):
        exists = filename in existing_files
        result["sourcePackFilesExist"][filename] = exists
        if not exists:
            result["missingSourcePackFiles"].append(filename)

    result["executionPlanExists"] = EXECUTION_PLAN_PATH.exists()
    result["inputGuideExists"] = INPUT_GUIDE_PATH.exists()
    if not result["executionPlanExists"]:
        result["errors"].append("MANUAL_REVIEW_EXECUTION_PLAN.md does not exist.")
    if not result["inputGuideExists"]:
        result["errors"].append("MANUAL_CONFIRMATION_INPUT_GUIDE.md does not exist.")

    if MANUAL_CONFIRMATION_PATH.exists():
        manual = json.loads(MANUAL_CONFIRMATION_PATH.read_text(encoding="utf-8-sig"))
        statuses = [item.get("manualReviewStatus") for item in manual.get("items", [])]
        result["pendingCount"] = statuses.count("pending_manual_confirmation")
        result["confirmedCount"] = statuses.count("confirmed")
        result["rejectedCount"] = statuses.count("rejected")
        if result["confirmedCount"] or result["rejectedCount"]:
            result["manualConfirmationModified"] = True
            result["warnings"].append("manual confirmation contains completed review statuses.")

    if POLICY_CARD_PATH.exists():
        result["policyCardPresent"] = True
        has_content = bool(POLICY_CARD_PATH.read_text(encoding="utf-8-sig").strip())
        result["policyCardHasContent"] = has_content
        if has_content:
            result["warnings"].append("policy_knowledge_cards.jsonl has content.")
        else:
            result["warnings"].append("policy_knowledge_cards.jsonl exists but is empty.")

    forbidden = _detect_forbidden_artifacts()
    result["generatedArtifactsWarnings"] = forbidden
    if forbidden:
        result["warnings"].append("Forbidden sample/schema artifacts detected.")

    result["passed"] = not any(
        [
            result["errors"],
            result["missingSourcePackFiles"],
        ]
    )
    result["warning"] = result["passed"] and bool(result["warnings"])
    return result
