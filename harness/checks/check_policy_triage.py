from __future__ import annotations

import json
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[2]
TRIAGE_JSON_PATH = ROOT / "data" / "sources" / "review" / "triage" / "policy_candidate_triage.json"
TRIAGE_MD_PATH = ROOT / "data" / "sources" / "review" / "triage" / "policy_candidate_triage.md"
MANUAL_REVIEW_QUEUE_PATH = ROOT / "docs" / "MANUAL_REVIEW_QUEUE.md"
POLICY_CARD_PATH = ROOT / "data" / "rag" / "policy_knowledge_cards.jsonl"
VALID_PRIORITIES = {"high", "medium", "low"}
EXPECTED_TOTAL_CANDIDATES = 201


def _base_result() -> dict[str, Any]:
    return {
        "check": "policy_triage",
        "passed": False,
        "warning": False,
        "triageJsonExists": False,
        "triageMarkdownExists": False,
        "manualReviewQueueExists": False,
        "totalCandidates": 0,
        "byPriority": {"high": 0, "medium": 0, "low": 0},
        "highPriorityCount": 0,
        "highPriorityBySource": {},
        "invalidPriorities": [],
        "invalidReviewStatuses": [],
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
    for folder in ["data", "docs", "harness", "schemas", "scripts", "tests"]:
        root = ROOT / folder
        if not root.exists():
            continue
        for path in root.rglob("*"):
            if path.is_file() and path.name in names:
                found.append(path.relative_to(ROOT).as_posix())
    return sorted(set(found))


def run_check() -> dict[str, Any]:
    result = _base_result()
    result["triageJsonExists"] = TRIAGE_JSON_PATH.exists()
    result["triageMarkdownExists"] = TRIAGE_MD_PATH.exists()
    result["manualReviewQueueExists"] = MANUAL_REVIEW_QUEUE_PATH.exists()

    if not result["triageJsonExists"]:
        result["errors"].append("policy_candidate_triage.json does not exist.")
        return result
    if not result["triageMarkdownExists"]:
        result["errors"].append("policy_candidate_triage.md does not exist.")
    if not result["manualReviewQueueExists"]:
        result["errors"].append("MANUAL_REVIEW_QUEUE.md does not exist.")

    try:
        triage = json.loads(TRIAGE_JSON_PATH.read_text(encoding="utf-8-sig"))
    except json.JSONDecodeError as exc:
        result["errors"].append(f"policy_candidate_triage.json parse failed: {exc}")
        return result

    result["totalCandidates"] = triage.get("totalCandidates", 0)
    result["byPriority"] = triage.get("byPriority", result["byPriority"])
    result["highPriorityCount"] = result["byPriority"].get("high", 0)
    if result["totalCandidates"] != EXPECTED_TOTAL_CANDIDATES:
        result["errors"].append(
            f"totalCandidates must be {EXPECTED_TOTAL_CANDIDATES}, got {result['totalCandidates']}."
        )

    review_queue = triage.get("reviewQueue")
    if not isinstance(review_queue, list) or not review_queue:
        result["errors"].append("reviewQueue must be a non-empty list.")
        review_queue = []

    high_by_source: dict[str, int] = {}
    for item in review_queue:
        priority = item.get("priority")
        review_status = item.get("reviewStatus")
        source_id = item.get("sourceId")

        if priority not in VALID_PRIORITIES:
            result["invalidPriorities"].append(
                {"candidateId": item.get("candidateId"), "priority": priority}
            )
        if review_status != "needs_pdf_check":
            result["invalidReviewStatuses"].append(
                {"candidateId": item.get("candidateId"), "reviewStatus": review_status}
            )
        if priority == "high":
            high_by_source[source_id] = high_by_source.get(source_id, 0) + 1

    result["highPriorityBySource"] = dict(sorted(high_by_source.items()))

    if result["highPriorityCount"] == 0:
        result["warnings"].append("No high priority policy candidates found.")

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
            result["invalidPriorities"],
            result["invalidReviewStatuses"],
        ]
    )
    result["warning"] = result["passed"] and bool(result["warnings"])
    return result
