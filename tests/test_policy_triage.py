from __future__ import annotations

import json
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
TRIAGE_JSON_PATH = ROOT / "data" / "sources" / "review" / "triage" / "policy_candidate_triage.json"
TRIAGE_MD_PATH = ROOT / "data" / "sources" / "review" / "triage" / "policy_candidate_triage.md"
MANUAL_REVIEW_QUEUE_PATH = ROOT / "docs" / "manual_review" / "MANUAL_REVIEW_QUEUE.md"
POLICY_CARD_PATH = ROOT / "data" / "rag" / "policy_knowledge_cards.jsonl"
VALID_PRIORITIES = {"high", "medium", "low"}


def load_triage() -> dict:
    assert TRIAGE_JSON_PATH.exists()
    return json.loads(TRIAGE_JSON_PATH.read_text(encoding="utf-8-sig"))


def test_policy_candidate_triage_json_exists() -> None:
    assert TRIAGE_JSON_PATH.exists()


def test_policy_candidate_triage_markdown_exists() -> None:
    assert TRIAGE_MD_PATH.exists()


def test_manual_review_queue_exists() -> None:
    assert MANUAL_REVIEW_QUEUE_PATH.exists()


def test_total_candidates_is_201() -> None:
    triage = load_triage()
    assert triage["totalCandidates"] == 201


def test_review_queue_is_not_empty() -> None:
    triage = load_triage()
    assert triage["reviewQueue"]


def test_review_queue_statuses_are_needs_pdf_check() -> None:
    triage = load_triage()
    assert {item["reviewStatus"] for item in triage["reviewQueue"]} == {"needs_pdf_check"}


def test_priorities_are_valid() -> None:
    triage = load_triage()
    priorities = {item["priority"] for item in triage["reviewQueue"]}
    assert priorities.issubset(VALID_PRIORITIES)


def test_policy_knowledge_cards_if_present_are_confirmed_only() -> None:
    if not POLICY_CARD_PATH.exists():
        return
    cards = [
        json.loads(line)
        for line in POLICY_CARD_PATH.read_text(encoding="utf-8-sig").splitlines()
        if line.strip()
    ]
    assert len(cards) == 9
    assert {card["sourceIds"][0] for card in cards} == {"KOR-003"}
