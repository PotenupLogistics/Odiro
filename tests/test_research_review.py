from __future__ import annotations

import json
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
RESEARCH_REVIEW_STATUS_PATH = ROOT / "data" / "sources" / "review" / "research_review_status.json"
RESEARCH_REVIEW_DIR = ROOT / "data" / "sources" / "review" / "research"
RSR_001_PROCESSED_PATH = (
    ROOT / "data" / "sources" / "processed" / "research" / "RSR-001_METRANS_Sidewalk_ADR_Interactions.md"
)
POLICY_CARD_PATH = ROOT / "data" / "rag" / "policy_knowledge_cards.jsonl"
EXPECTED_RSR_IDS = {"RSR-001", "RSR-002", "RSR-003", "RSR-004", "RSR-005", "RSR-006"}
URL_ONLY_RSR_IDS = {"RSR-002", "RSR-003", "RSR-004", "RSR-005", "RSR-006"}


def load_research_review_status() -> list[dict]:
    assert RESEARCH_REVIEW_STATUS_PATH.exists()
    data = json.loads(RESEARCH_REVIEW_STATUS_PATH.read_text(encoding="utf-8-sig"))
    assert isinstance(data, list)
    return data


def test_research_review_status_exists() -> None:
    assert RESEARCH_REVIEW_STATUS_PATH.exists()


def test_research_review_status_includes_all_sources() -> None:
    entries = load_research_review_status()
    source_ids = {entry["sourceId"] for entry in entries}
    assert source_ids == EXPECTED_RSR_IDS


def test_rsr_001_source_mode_is_local_pdf() -> None:
    entries = {entry["sourceId"]: entry for entry in load_research_review_status()}
    assert entries["RSR-001"]["sourceMode"] == "local_pdf"


def test_url_only_sources_have_url_only_mode() -> None:
    entries = {entry["sourceId"]: entry for entry in load_research_review_status()}
    for source_id in URL_ONLY_RSR_IDS:
        assert entries[source_id]["sourceMode"] == "url_only"


def test_rsr_001_processed_markdown_exists() -> None:
    assert RSR_001_PROCESSED_PATH.exists()


def test_research_review_checklists_exist() -> None:
    for source_id in EXPECTED_RSR_IDS:
        assert (RESEARCH_REVIEW_DIR / f"{source_id}_review_checklist.md").exists()


def test_research_review_statuses_are_not_started() -> None:
    entries = load_research_review_status()
    assert {entry["reviewStatus"] for entry in entries} == {"not_started"}


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
