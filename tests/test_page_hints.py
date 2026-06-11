from __future__ import annotations

import json
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
PAGE_HINTS_DIR = ROOT / "data" / "sources" / "review" / "high_priority" / "page_hints"
PAGE_HINTS_JSON_PATH = PAGE_HINTS_DIR / "high_priority_page_hints.json"
MANUAL_CONFIRMATION_PATH = ROOT / "data" / "sources" / "review" / "confirmed" / "manual_confirmation_results.json"
POLICY_CARD_PATH = ROOT / "data" / "rag" / "policy_knowledge_cards.jsonl"
VALID_HINT_STATUSES = {"found", "partial", "not_found", "needs_manual_page_search"}
EXPECTED_SOURCE_FILES = {
    "KOR-001_page_hints.md",
    "KOR-002_page_hints.md",
    "KOR-003_page_hints.md",
    "KOR-004_page_hints.md",
    "KOR-005_page_hints.md",
}


def load_page_hints() -> dict:
    assert PAGE_HINTS_JSON_PATH.exists()
    return json.loads(PAGE_HINTS_JSON_PATH.read_text(encoding="utf-8-sig"))


def test_high_priority_page_hints_json_exists() -> None:
    assert PAGE_HINTS_JSON_PATH.exists()


def test_page_hint_item_count_is_43() -> None:
    page_hints = load_page_hints()
    assert len(page_hints["items"]) == 43


def test_page_hint_statuses_are_valid() -> None:
    page_hints = load_page_hints()
    statuses = {item["hintStatus"] for item in page_hints["items"]}
    assert statuses.issubset(VALID_HINT_STATUSES)


def test_source_page_hint_markdown_files_exist() -> None:
    existing = {path.name for path in PAGE_HINTS_DIR.glob("KOR-*_page_hints.md")}
    assert EXPECTED_SOURCE_FILES.issubset(existing)


def test_manual_confirmation_statuses_are_valid() -> None:
    results = json.loads(MANUAL_CONFIRMATION_PATH.read_text(encoding="utf-8-sig"))
    statuses = {item["manualReviewStatus"] for item in results["items"]}
    assert statuses.issubset({"pending_manual_confirmation", "confirmed", "rejected"})
    counts = {status: 0 for status in ["pending_manual_confirmation", "confirmed", "rejected"]}
    for item in results["items"]:
        counts[item["manualReviewStatus"]] += 1
    assert counts == {"pending_manual_confirmation": 30, "confirmed": 9, "rejected": 4}


def test_policy_knowledge_cards_if_present_are_confirmed_only() -> None:
    if not POLICY_CARD_PATH.exists():
        return
    cards = [
        json.loads(line)
        for line in POLICY_CARD_PATH.read_text(encoding="utf-8-sig").splitlines()
        if line.strip()
    ]
    assert len(cards) == 11
    assert {card["sourceIds"][0] for card in cards} == {"KOR-003", "KOR-004"}
