from __future__ import annotations

import json
from collections import Counter
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[2]
RESULTS_JSON_PATH = ROOT / "data" / "sources" / "review" / "confirmed" / "manual_confirmation_results.json"
POLICY_CARD_PATH = ROOT / "data" / "rag" / "policy_knowledge_cards.jsonl"
EXPECTED_CANDIDATE_COUNT = 43
VALID_STATUSES = {"pending_manual_confirmation", "confirmed", "rejected"}


def _base_result() -> dict[str, Any]:
    return {
        "check": "manual_confirmation",
        "passed": False,
        "warning": False,
        "resultsJsonExists": False,
        "totalItems": 0,
        "pendingCount": 0,
        "confirmedCount": 0,
        "rejectedCount": 0,
        "duplicateCandidateIds": [],
        "invalidManualReviewStatuses": [],
        "invalidConfirmedItems": [],
        "invalidRejectedItems": [],
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


def _is_filled(value: object) -> bool:
    return isinstance(value, str) and bool(value.strip())


def run_check() -> dict[str, Any]:
    result = _base_result()

    if not RESULTS_JSON_PATH.exists():
        result["errors"].append("manual_confirmation_results.json does not exist.")
        return result

    result["resultsJsonExists"] = True

    try:
        payload = json.loads(RESULTS_JSON_PATH.read_text(encoding="utf-8-sig"))
    except json.JSONDecodeError as exc:
        result["errors"].append(f"manual_confirmation_results.json parse failed: {exc}")
        return result

    items = payload.get("items")
    if not isinstance(items, list):
        result["errors"].append("items must be a list.")
        return result

    result["totalItems"] = len(items)
    if len(items) != EXPECTED_CANDIDATE_COUNT:
        result["errors"].append(
            f"items must contain {EXPECTED_CANDIDATE_COUNT} candidates, got {len(items)}."
        )

    candidate_ids = [item.get("candidateId") for item in items if isinstance(item, dict)]
    counts = Counter(candidate_ids)
    result["duplicateCandidateIds"] = sorted(
        candidate_id for candidate_id, count in counts.items() if count > 1
    )

    status_counts = Counter()
    for item in items:
        if not isinstance(item, dict):
            result["errors"].append("manual confirmation item must be an object.")
            continue

        status = item.get("manualReviewStatus")
        candidate_id = item.get("candidateId")
        if status not in VALID_STATUSES:
            result["invalidManualReviewStatuses"].append(
                {"candidateId": candidate_id, "manualReviewStatus": status}
            )
            continue

        status_counts[status] += 1

        if status == "confirmed":
            missing = []
            if not (_is_filled(item.get("rawPdfPage")) or _is_filled(item.get("rawPdfSection"))):
                missing.append("rawPdfPage_or_rawPdfSection")
            for field in ["confirmedText", "reviewer", "reviewedAt", "decisionReason", "nextAction"]:
                if not _is_filled(item.get(field)):
                    missing.append(field)
            if missing:
                result["invalidConfirmedItems"].append(
                    {"candidateId": candidate_id, "missingFields": missing}
                )
        elif status == "rejected":
            missing = [
                field
                for field in ["reviewer", "reviewedAt", "decisionReason", "nextAction"]
                if not _is_filled(item.get(field))
            ]
            if missing:
                result["invalidRejectedItems"].append(
                    {"candidateId": candidate_id, "missingFields": missing}
                )

    result["pendingCount"] = status_counts["pending_manual_confirmation"]
    result["confirmedCount"] = status_counts["confirmed"]
    result["rejectedCount"] = status_counts["rejected"]

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
            result["duplicateCandidateIds"],
            result["invalidManualReviewStatuses"],
            result["invalidConfirmedItems"],
            result["invalidRejectedItems"],
        ]
    )
    result["warning"] = result["passed"] and bool(result["warnings"])
    return result
