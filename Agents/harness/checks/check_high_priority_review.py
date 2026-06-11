from __future__ import annotations

import json
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[2]
HIGH_PRIORITY_DIR = ROOT / "data" / "sources" / "review" / "high_priority"
QUEUE_JSON_PATH = HIGH_PRIORITY_DIR / "high_priority_review_queue.json"
QUEUE_MD_PATH = HIGH_PRIORITY_DIR / "high_priority_review_queue.md"
POLICY_CARD_PATH = ROOT / "data" / "rag" / "policy_knowledge_cards.jsonl"
EXPECTED_SOURCE_FILES = {
    "KOR-001_high_priority_review.md",
    "KOR-002_high_priority_review.md",
    "KOR-003_high_priority_review.md",
    "KOR-004_high_priority_review.md",
    "KOR-005_high_priority_review.md",
}
EXPECTED_HIGH_COUNT = 43


def _base_result() -> dict[str, Any]:
    return {
        "check": "high_priority_review",
        "passed": False,
        "warning": False,
        "queueJsonExists": False,
        "queueMarkdownExists": False,
        "totalHighPriorityCandidates": 0,
        "sourceReviewFilesExist": {},
        "missingSourceReviewFiles": [],
        "invalidManualReviewStatuses": [],
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
    result["queueJsonExists"] = QUEUE_JSON_PATH.exists()
    result["queueMarkdownExists"] = QUEUE_MD_PATH.exists()

    if not result["queueJsonExists"]:
        result["errors"].append("high_priority_review_queue.json does not exist.")
        return result
    if not result["queueMarkdownExists"]:
        result["errors"].append("high_priority_review_queue.md does not exist.")

    try:
        queue = json.loads(QUEUE_JSON_PATH.read_text(encoding="utf-8-sig"))
    except json.JSONDecodeError as exc:
        result["errors"].append(f"high_priority_review_queue.json parse failed: {exc}")
        return result

    result["totalHighPriorityCandidates"] = queue.get("totalHighPriorityCandidates", 0)
    if result["totalHighPriorityCandidates"] != EXPECTED_HIGH_COUNT:
        result["warnings"].append(
            f"Expected {EXPECTED_HIGH_COUNT} high priority candidates, got {result['totalHighPriorityCandidates']}."
        )

    items = queue.get("items")
    if not isinstance(items, list) or not items:
        result["errors"].append("items must be a non-empty list.")
        items = []

    for item in items:
        status = item.get("manualReviewStatus")
        if status != "pending_manual_confirmation":
            result["invalidManualReviewStatuses"].append(
                {"candidateId": item.get("candidateId"), "manualReviewStatus": status}
            )

    existing_files = {path.name for path in HIGH_PRIORITY_DIR.glob("KOR-*_high_priority_review.md")}
    for filename in sorted(EXPECTED_SOURCE_FILES):
        exists = filename in existing_files
        result["sourceReviewFilesExist"][filename] = exists
        if not exists:
            result["missingSourceReviewFiles"].append(filename)

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
            result["missingSourceReviewFiles"],
            result["invalidManualReviewStatuses"],
        ]
    )
    result["warning"] = result["passed"] and bool(result["warnings"])
    return result
