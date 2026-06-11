from __future__ import annotations

import json
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
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


def test_source_review_pack_files_exist() -> None:
    existing = {path.name for path in PACK_DIR.glob("KOR-*_review_pack.md")}
    assert EXPECTED_PACK_FILES.issubset(existing)


def test_manual_review_execution_plan_exists() -> None:
    assert EXECUTION_PLAN_PATH.exists()


def test_manual_confirmation_input_guide_exists() -> None:
    assert INPUT_GUIDE_PATH.exists()


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
