from __future__ import annotations

import json
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
CANDIDATE_INDEX_PATH = (
    ROOT / "data" / "sources" / "review" / "candidates" / "policy_candidate_index.json"
)
CANDIDATE_DIR = ROOT / "data" / "sources" / "review" / "candidates" / "korea"
POLICY_CARD_PATH = ROOT / "data" / "rag" / "policy_knowledge_cards.jsonl"
EXPECTED_SOURCES = {
    "KOR-001": "KOR-001_policy_candidates.md",
    "KOR-002": "KOR-002_policy_candidates.md",
    "KOR-003": "KOR-003_policy_candidates.md",
    "KOR-004": "KOR-004_policy_candidates.md",
    "KOR-005": "KOR-005_policy_candidates.md",
}


def load_candidate_index() -> dict:
    assert CANDIDATE_INDEX_PATH.exists()
    return json.loads(CANDIDATE_INDEX_PATH.read_text(encoding="utf-8-sig"))


def test_policy_candidate_index_exists() -> None:
    assert CANDIDATE_INDEX_PATH.exists()


def test_policy_candidate_index_includes_all_sources() -> None:
    index = load_candidate_index()
    source_ids = {entry["sourceId"] for entry in index["sources"]}
    assert source_ids == set(EXPECTED_SOURCES)


def test_candidate_markdown_files_exist() -> None:
    for filename in EXPECTED_SOURCES.values():
        assert (CANDIDATE_DIR / filename).exists()


def test_candidate_markdown_files_include_source_id() -> None:
    for source_id, filename in EXPECTED_SOURCES.items():
        content = (CANDIDATE_DIR / filename).read_text(encoding="utf-8-sig")
        assert source_id in content


def test_candidate_markdown_files_include_policy_candidate_list() -> None:
    for filename in EXPECTED_SOURCES.values():
        content = (CANDIDATE_DIR / filename).read_text(encoding="utf-8-sig")
        assert "정책 후보 목록" in content


def test_candidate_review_statuses_are_needs_pdf_check() -> None:
    index = load_candidate_index()
    for entry in index["sources"]:
        assert entry["reviewStatus"] == "needs_pdf_check"

    for filename in EXPECTED_SOURCES.values():
        content = (CANDIDATE_DIR / filename).read_text(encoding="utf-8-sig")
        assert "needs_pdf_check" in content


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
