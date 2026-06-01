from __future__ import annotations

import json
from pathlib import Path

from harness.checks.check_manual_confirmation import run_check


ROOT = Path(__file__).resolve().parents[1]
RESULTS_JSON_PATH = ROOT / "data" / "sources" / "review" / "confirmed" / "manual_confirmation_results.json"
RESULTS_MD_PATH = ROOT / "data" / "sources" / "review" / "confirmed" / "manual_confirmation_results.md"
POLICY_CARD_PATH = ROOT / "data" / "rag" / "policy_knowledge_cards.jsonl"


def load_results() -> dict:
    assert RESULTS_JSON_PATH.exists()
    return json.loads(RESULTS_JSON_PATH.read_text(encoding="utf-8-sig"))


def test_manual_confirmation_results_json_exists() -> None:
    assert RESULTS_JSON_PATH.exists()


def test_manual_confirmation_item_count_is_43() -> None:
    results = load_results()
    assert len(results["items"]) == 43


def test_manual_confirmation_candidate_ids_are_unique() -> None:
    results = load_results()
    candidate_ids = [item["candidateId"] for item in results["items"]]
    assert len(candidate_ids) == len(set(candidate_ids))


def test_manual_review_status_counts_match_current_review_state() -> None:
    results = load_results()
    counts = {status: 0 for status in ["pending_manual_confirmation", "confirmed", "rejected"]}
    for item in results["items"]:
        counts[item["manualReviewStatus"]] += 1
    assert counts == {"pending_manual_confirmation": 30, "confirmed": 9, "rejected": 4}


def test_manual_confirmation_results_markdown_exists() -> None:
    assert RESULTS_MD_PATH.exists()


def test_check_manual_confirmation_accepts_current_review_state() -> None:
    result = run_check()
    assert result["passed"] is True
    assert result["pendingCount"] == 30
    assert result["confirmedCount"] == 9
    assert result["rejectedCount"] == 4


def test_policy_knowledge_cards_if_present_are_confirmed_only() -> None:
    if not POLICY_CARD_PATH.exists():
        return
    results = load_results()
    confirmed_ids = {
        item["candidateId"]
        for item in results["items"]
        if item["manualReviewStatus"] == "confirmed"
    }
    cards = [
        json.loads(line)
        for line in POLICY_CARD_PATH.read_text(encoding="utf-8-sig").splitlines()
        if line.strip()
    ]
    assert {card["createdFromCandidateId"] for card in cards} == confirmed_ids
