from __future__ import annotations

import json
import re
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[2]
RESEARCH_REVIEW_STATUS_PATH = ROOT / "data" / "sources" / "review" / "research_review_status.json"
POLICY_CARD_PATH = ROOT / "data" / "rag" / "policy_knowledge_cards.jsonl"
EXPECTED_RSR_IDS = {"RSR-001", "RSR-002", "RSR-003", "RSR-004", "RSR-005", "RSR-006"}
URL_ONLY_RSR_IDS = {"RSR-002", "RSR-003", "RSR-004", "RSR-005", "RSR-006"}
VALID_REVIEW_STATUSES = {"not_started", "in_progress", "reviewed", "blocked"}
VALID_EXTRACTION_STATUSES = {"success", "partial", "failed", "needs_manual_review"}


def _base_result() -> dict[str, Any]:
    return {
        "check": "research_review",
        "passed": False,
        "warning": False,
        "reviewStatusExists": False,
        "sourceCount": 0,
        "localPdfSources": [],
        "urlOnlySources": [],
        "rsr001ProcessedExists": False,
        "rsr001ExtractionStatus": "",
        "missingSourceIds": [],
        "unexpectedSourceIds": [],
        "missingChecklistFiles": [],
        "missingProcessedFiles": [],
        "invalidSourceModes": [],
        "invalidReviewStatuses": [],
        "invalidExtractionStatuses": [],
        "policyCardPresent": False,
        "policyCardHasContent": False,
        "warnings": [],
        "errors": [],
    }


def _extract_status(content: str) -> str | None:
    match = re.search(r"^\* extractionStatus:\s*(\S+)\s*$", content, flags=re.MULTILINE)
    if match:
        return match.group(1)
    return None


def run_check() -> dict[str, Any]:
    result = _base_result()

    if not RESEARCH_REVIEW_STATUS_PATH.exists():
        result["errors"].append("research_review_status.json does not exist.")
        return result

    result["reviewStatusExists"] = True

    try:
        entries = json.loads(RESEARCH_REVIEW_STATUS_PATH.read_text(encoding="utf-8-sig"))
    except json.JSONDecodeError as exc:
        result["errors"].append(f"research_review_status.json parse failed: {exc}")
        return result

    if not isinstance(entries, list):
        result["errors"].append("research_review_status.json root must be a JSON list.")
        return result

    result["sourceCount"] = len(entries)
    source_ids = {entry.get("sourceId") for entry in entries if isinstance(entry, dict)}
    result["missingSourceIds"] = sorted(EXPECTED_RSR_IDS - source_ids)
    result["unexpectedSourceIds"] = sorted(source_ids - EXPECTED_RSR_IDS)

    for entry in entries:
        if not isinstance(entry, dict):
            result["errors"].append("research review entry must be an object.")
            continue

        source_id = entry.get("sourceId")
        source_mode = entry.get("sourceMode")
        review_status = entry.get("reviewStatus")
        checklist_path_value = entry.get("reviewChecklistPath")
        processed_path_value = entry.get("processedFilePath")

        if review_status not in VALID_REVIEW_STATUSES:
            result["invalidReviewStatuses"].append(
                {"sourceId": source_id, "reviewStatus": review_status}
            )

        if source_id == "RSR-001":
            if source_mode != "local_pdf":
                result["invalidSourceModes"].append({"sourceId": source_id, "sourceMode": source_mode})
            else:
                result["localPdfSources"].append(source_id)
        elif source_id in URL_ONLY_RSR_IDS:
            if source_mode != "url_only":
                result["invalidSourceModes"].append({"sourceId": source_id, "sourceMode": source_mode})
            else:
                result["urlOnlySources"].append(source_id)

        if not checklist_path_value or not (ROOT / checklist_path_value).exists():
            result["missingChecklistFiles"].append(
                {"sourceId": source_id, "reviewChecklistPath": checklist_path_value}
            )

        if source_id == "RSR-001":
            if not processed_path_value or not (ROOT / processed_path_value).exists():
                result["missingProcessedFiles"].append(
                    {"sourceId": source_id, "processedFilePath": processed_path_value}
                )
            else:
                result["rsr001ProcessedExists"] = True
                content = (ROOT / processed_path_value).read_text(encoding="utf-8-sig")
                status = _extract_status(content)
                result["rsr001ExtractionStatus"] = status or ""
                if status not in VALID_EXTRACTION_STATUSES:
                    result["invalidExtractionStatuses"].append(
                        {"sourceId": source_id, "extractionStatus": status}
                    )
        elif source_id in URL_ONLY_RSR_IDS and processed_path_value:
            result["errors"].append(f"{source_id} is url_only but has processedFilePath.")

    if POLICY_CARD_PATH.exists():
        result["policyCardPresent"] = True
        has_content = bool(POLICY_CARD_PATH.read_text(encoding="utf-8-sig").strip())
        result["policyCardHasContent"] = has_content
        if has_content:
            result["warnings"].append("policy_knowledge_cards.jsonl has content.")
        else:
            result["warnings"].append("policy_knowledge_cards.jsonl exists but is empty.")

    result["passed"] = not any(
        [
            result["errors"],
            result["missingSourceIds"],
            result["unexpectedSourceIds"],
            result["missingChecklistFiles"],
            result["missingProcessedFiles"],
            result["invalidSourceModes"],
            result["invalidReviewStatuses"],
            result["invalidExtractionStatuses"],
        ]
    )
    result["warning"] = result["passed"] and bool(result["warnings"])
    return result
