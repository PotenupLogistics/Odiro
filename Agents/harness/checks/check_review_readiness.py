from __future__ import annotations

import json
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[2]
REVIEW_STATUS_PATH = ROOT / "data" / "sources" / "review" / "review_status.json"
POLICY_CARD_PATH = ROOT / "data" / "rag" / "policy_knowledge_cards.jsonl"
EXPECTED_SOURCE_IDS = {"KOR-001", "KOR-002", "KOR-003", "KOR-004", "KOR-005"}
VALID_REVIEW_STATUSES = {"not_started", "in_progress", "reviewed", "blocked"}


def _base_result() -> dict[str, Any]:
    return {
        "check": "review_readiness",
        "passed": False,
        "warning": False,
        "reviewStatusExists": False,
        "sourceCount": 0,
        "missingSourceIds": [],
        "unexpectedSourceIds": [],
        "missingChecklistFiles": [],
        "missingMetadataSections": [],
        "missingPolicyExtractionAreas": [],
        "invalidReviewStatuses": [],
        "notStartedSources": [],
        "policyCardPresentBeforeReview": False,
        "warnings": [],
        "errors": [],
    }


def run_check() -> dict[str, Any]:
    result = _base_result()

    if not REVIEW_STATUS_PATH.exists():
        result["errors"].append("review_status.json does not exist.")
        return result

    result["reviewStatusExists"] = True

    try:
        entries = json.loads(REVIEW_STATUS_PATH.read_text(encoding="utf-8-sig"))
    except json.JSONDecodeError as exc:
        result["errors"].append(f"review_status.json parse failed: {exc}")
        return result

    if not isinstance(entries, list):
        result["errors"].append("review_status.json root must be a JSON list.")
        return result

    result["sourceCount"] = len(entries)
    source_ids = {entry.get("sourceId") for entry in entries if isinstance(entry, dict)}
    result["missingSourceIds"] = sorted(EXPECTED_SOURCE_IDS - source_ids)
    result["unexpectedSourceIds"] = sorted(source_ids - EXPECTED_SOURCE_IDS)

    for entry in entries:
        if not isinstance(entry, dict):
            result["errors"].append("review status entry must be an object.")
            continue

        source_id = entry.get("sourceId")
        checklist_value = entry.get("reviewChecklistPath")
        review_status = entry.get("reviewStatus")

        if review_status not in VALID_REVIEW_STATUSES:
            result["invalidReviewStatuses"].append(
                {"sourceId": source_id, "reviewStatus": review_status}
            )
        elif review_status == "not_started":
            result["notStartedSources"].append(source_id)

        if not isinstance(checklist_value, str) or not checklist_value:
            result["missingChecklistFiles"].append(
                {"sourceId": source_id, "reviewChecklistPath": checklist_value}
            )
            continue

        checklist_path = ROOT / checklist_value
        if not checklist_path.exists():
            result["missingChecklistFiles"].append(
                {"sourceId": source_id, "reviewChecklistPath": checklist_value}
            )
            continue

        content = checklist_path.read_text(encoding="utf-8-sig")
        if "## 1. Source Metadata" not in content:
            result["missingMetadataSections"].append(source_id)
        if "정책 추출 후보 영역" not in content:
            result["missingPolicyExtractionAreas"].append(source_id)

    reviewed_sources = {
        entry.get("sourceId")
        for entry in entries
        if isinstance(entry, dict) and entry.get("reviewStatus") == "reviewed"
    }
    if POLICY_CARD_PATH.exists() and reviewed_sources != EXPECTED_SOURCE_IDS:
        result["policyCardPresentBeforeReview"] = True
        result["warnings"].append(
            "policy_knowledge_cards.jsonl exists before all sources are reviewed."
        )

    result["passed"] = not any(
        [
            result["errors"],
            result["missingSourceIds"],
            result["unexpectedSourceIds"],
            result["missingChecklistFiles"],
            result["missingMetadataSections"],
            result["missingPolicyExtractionAreas"],
            result["invalidReviewStatuses"],
        ]
    )
    result["warning"] = result["passed"] and (
        bool(result["notStartedSources"]) or result["policyCardPresentBeforeReview"]
    )

    return result
