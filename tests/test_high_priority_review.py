from __future__ import annotations

import json
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
HIGH_PRIORITY_DIR = ROOT / "data" / "sources" / "review" / "high_priority"
QUEUE_JSON_PATH = HIGH_PRIORITY_DIR / "high_priority_review_queue.json"
POLICY_CARD_PATH = ROOT / "data" / "rag" / "policy_knowledge_cards.jsonl"
EXPECTED_SOURCE_FILES = {
    "KOR-001_high_priority_review.md",
    "KOR-002_high_priority_review.md",
    "KOR-003_high_priority_review.md",
    "KOR-004_high_priority_review.md",
    "KOR-005_high_priority_review.md",
}


def load_queue() -> dict:
    assert QUEUE_JSON_PATH.exists()
    return json.loads(QUEUE_JSON_PATH.read_text(encoding="utf-8-sig"))


def test_high_priority_review_queue_json_exists() -> None:
    assert QUEUE_JSON_PATH.exists()


def test_total_high_priority_candidates_is_43() -> None:
    queue = load_queue()
    assert queue["totalHighPriorityCandidates"] == 43


def test_high_priority_items_length_is_43() -> None:
    queue = load_queue()
    assert len(queue["items"]) == 43


def test_high_priority_items_are_pending_manual_confirmation() -> None:
    queue = load_queue()
    assert {
        item["manualReviewStatus"]
        for item in queue["items"]
    } == {"pending_manual_confirmation"}


def test_source_high_priority_review_files_exist() -> None:
    existing = {path.name for path in HIGH_PRIORITY_DIR.glob("KOR-*_high_priority_review.md")}
    assert EXPECTED_SOURCE_FILES.issubset(existing)


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


def test_sample_json_fixture_artifacts_are_not_created() -> None:
    forbidden = {
        "world_config.json",
        "policy_config.json",
        "decision_request.json",
        "decision_response.json",
    }
    found = []
    for folder in ["data", "docs", "harness", "schemas", "scripts", "tests"]:
        root = ROOT / folder
        if not root.exists():
            continue
        found.extend(path for path in root.rglob("*") if path.is_file() and path.name in forbidden)
    assert found == []
