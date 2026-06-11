from __future__ import annotations

import json
import re
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[2]
CANDIDATE_INDEX_PATH = (
    ROOT / "data" / "sources" / "review" / "candidates" / "policy_candidate_index.json"
)
POLICY_CARD_PATH = ROOT / "data" / "rag" / "policy_knowledge_cards.jsonl"
EXPECTED_SOURCE_IDS = {"KOR-001", "KOR-002", "KOR-003", "KOR-004", "KOR-005"}


def _base_result() -> dict[str, Any]:
    return {
        "check": "policy_candidates",
        "passed": False,
        "warning": False,
        "candidateIndexExists": False,
        "sourceCount": 0,
        "totalCandidateCount": 0,
        "sourceCandidateCounts": {},
        "missingSourceIds": [],
        "unexpectedSourceIds": [],
        "missingCandidateFiles": [],
        "missingMetadataSections": [],
        "missingCandidateListSections": [],
        "invalidReviewStatuses": [],
        "zeroCandidateSources": [],
        "policyCardPresent": False,
        "policyCardHasContent": False,
        "warnings": [],
        "errors": [],
    }


def _candidate_rows(content: str) -> list[str]:
    rows: list[str] = []
    for line in content.splitlines():
        if re.match(r"^\|\s*CAND-", line):
            rows.append(line)
    return rows


def _candidate_row_has_needs_pdf_check(row: str) -> bool:
    return row.rstrip().endswith("| needs_pdf_check |")


def run_check() -> dict[str, Any]:
    result = _base_result()

    if not CANDIDATE_INDEX_PATH.exists():
        result["errors"].append("policy_candidate_index.json does not exist.")
        return result

    result["candidateIndexExists"] = True

    try:
        index = json.loads(CANDIDATE_INDEX_PATH.read_text(encoding="utf-8-sig"))
    except json.JSONDecodeError as exc:
        result["errors"].append(f"policy_candidate_index.json parse failed: {exc}")
        return result

    entries = index.get("sources")
    if not isinstance(entries, list):
        result["errors"].append("policy candidate index must include a sources list.")
        return result

    result["sourceCount"] = len(entries)
    result["totalCandidateCount"] = index.get("totalCandidateCount", 0)
    source_ids = {entry.get("sourceId") for entry in entries if isinstance(entry, dict)}
    result["missingSourceIds"] = sorted(EXPECTED_SOURCE_IDS - source_ids)
    result["unexpectedSourceIds"] = sorted(source_ids - EXPECTED_SOURCE_IDS)

    for entry in entries:
        if not isinstance(entry, dict):
            result["errors"].append("policy candidate index entry must be an object.")
            continue

        source_id = entry.get("sourceId")
        candidate_count = entry.get("candidateCount", 0)
        result["sourceCandidateCounts"][source_id] = candidate_count
        if candidate_count == 0:
            result["zeroCandidateSources"].append(source_id)

        if entry.get("reviewStatus") != "needs_pdf_check":
            result["invalidReviewStatuses"].append(
                {"sourceId": source_id, "reviewStatus": entry.get("reviewStatus")}
            )

        candidate_path_value = entry.get("candidateFilePath")
        if not isinstance(candidate_path_value, str) or not candidate_path_value:
            result["missingCandidateFiles"].append(
                {"sourceId": source_id, "candidateFilePath": candidate_path_value}
            )
            continue

        candidate_path = ROOT / candidate_path_value
        if not candidate_path.exists():
            result["missingCandidateFiles"].append(
                {"sourceId": source_id, "candidateFilePath": candidate_path_value}
            )
            continue

        content = candidate_path.read_text(encoding="utf-8-sig")
        if "## 1. Source Metadata" not in content:
            result["missingMetadataSections"].append(source_id)
        if "정책 후보 목록" not in content:
            result["missingCandidateListSections"].append(source_id)

        for row in _candidate_rows(content):
            if not _candidate_row_has_needs_pdf_check(row):
                result["invalidReviewStatuses"].append(
                    {"sourceId": source_id, "reviewStatus": "not_needs_pdf_check"}
                )

    if POLICY_CARD_PATH.exists():
        result["policyCardPresent"] = True
        has_content = bool(POLICY_CARD_PATH.read_text(encoding="utf-8-sig").strip())
        result["policyCardHasContent"] = has_content
        if has_content:
            result["warnings"].append("policy_knowledge_cards.jsonl has content.")
        else:
            result["warnings"].append("policy_knowledge_cards.jsonl exists but is empty.")

    if result["zeroCandidateSources"]:
        result["warnings"].append("One or more sources have zero policy candidates.")

    result["passed"] = not any(
        [
            result["errors"],
            result["missingSourceIds"],
            result["unexpectedSourceIds"],
            result["missingCandidateFiles"],
            result["missingMetadataSections"],
            result["missingCandidateListSections"],
            result["invalidReviewStatuses"],
        ]
    )
    result["warning"] = result["passed"] and bool(result["warnings"])

    return result
