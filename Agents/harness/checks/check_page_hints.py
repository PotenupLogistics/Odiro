from __future__ import annotations

import json
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[2]
PAGE_HINTS_DIR = ROOT / "data" / "sources" / "review" / "high_priority" / "page_hints"
PAGE_HINTS_JSON_PATH = PAGE_HINTS_DIR / "high_priority_page_hints.json"
PAGE_HINTS_MD_PATH = PAGE_HINTS_DIR / "high_priority_page_hints.md"
MANUAL_CONFIRMATION_PATH = ROOT / "data" / "sources" / "review" / "confirmed" / "manual_confirmation_results.json"
POLICY_CARD_PATH = ROOT / "data" / "rag" / "policy_knowledge_cards.jsonl"
EXPECTED_SOURCE_FILES = {
    "KOR-001_page_hints.md",
    "KOR-002_page_hints.md",
    "KOR-003_page_hints.md",
    "KOR-004_page_hints.md",
    "KOR-005_page_hints.md",
}
VALID_HINT_STATUSES = {"found", "partial", "not_found", "needs_manual_page_search"}
EXPECTED_COUNT = 43


def _base_result() -> dict[str, Any]:
    return {
        "check": "page_hints",
        "passed": False,
        "warning": False,
        "pageHintsJsonExists": False,
        "pageHintsMarkdownExists": False,
        "totalCandidates": 0,
        "hintSummary": {"found": 0, "partial": 0, "notFound": 0, "needsManualPageSearch": 0},
        "sourceSummary": {},
        "missingSourceFiles": [],
        "invalidHintStatuses": [],
        "invalidPageHints": [],
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
    result["pageHintsJsonExists"] = PAGE_HINTS_JSON_PATH.exists()
    result["pageHintsMarkdownExists"] = PAGE_HINTS_MD_PATH.exists()

    if not result["pageHintsJsonExists"]:
        result["errors"].append("high_priority_page_hints.json does not exist.")
        return result
    if not result["pageHintsMarkdownExists"]:
        result["errors"].append("high_priority_page_hints.md does not exist.")

    try:
        payload = json.loads(PAGE_HINTS_JSON_PATH.read_text(encoding="utf-8-sig"))
    except json.JSONDecodeError as exc:
        result["errors"].append(f"high_priority_page_hints.json parse failed: {exc}")
        return result

    items = payload.get("items")
    if not isinstance(items, list):
        result["errors"].append("items must be a list.")
        items = []

    result["totalCandidates"] = payload.get("totalCandidates", len(items))
    result["hintSummary"] = payload.get("hintSummary", result["hintSummary"])
    if result["totalCandidates"] != EXPECTED_COUNT:
        result["errors"].append(f"totalCandidates must be {EXPECTED_COUNT}.")
    if len(items) != EXPECTED_COUNT:
        result["errors"].append(f"items must contain {EXPECTED_COUNT} entries.")

    source_summary: dict[str, dict[str, int]] = {}
    for item in items:
        source_id = item.get("sourceId")
        status = item.get("hintStatus")
        source_summary.setdefault(source_id, {"found": 0, "partial": 0, "not_found": 0, "needs_manual_page_search": 0})
        if status in source_summary[source_id]:
            source_summary[source_id][status] += 1

        if status not in VALID_HINT_STATUSES:
            result["invalidHintStatuses"].append(
                {"candidateId": item.get("candidateId"), "hintStatus": status}
            )

        page_hints = item.get("pageHints", [])
        if page_hints:
            for hint in page_hints:
                page_number = hint.get("pageNumber")
                if not isinstance(page_number, int) or page_number <= 0:
                    result["invalidPageHints"].append(
                        {"candidateId": item.get("candidateId"), "pageNumber": page_number}
                    )

    result["sourceSummary"] = dict(sorted(source_summary.items()))

    existing_files = {path.name for path in PAGE_HINTS_DIR.glob("KOR-*_page_hints.md")}
    result["missingSourceFiles"] = sorted(EXPECTED_SOURCE_FILES - existing_files)

    if MANUAL_CONFIRMATION_PATH.exists():
        manual = json.loads(MANUAL_CONFIRMATION_PATH.read_text(encoding="utf-8-sig"))
        statuses = {item.get("manualReviewStatus") for item in manual.get("items", [])}
        if statuses - {"pending_manual_confirmation", "confirmed", "rejected"}:
            result["errors"].append("manual_confirmation_results.json contains invalid statuses.")
        if statuses & {"confirmed", "rejected"}:
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
            result["missingSourceFiles"],
            result["invalidHintStatuses"],
            result["invalidPageHints"],
        ]
    )
    result["warning"] = result["passed"] and bool(result["warnings"])
    return result
