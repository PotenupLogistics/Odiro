from __future__ import annotations

import json
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
REVIEW_STATUS_PATH = ROOT / "data" / "sources" / "review" / "review_status.json"
REVIEW_DIR = ROOT / "data" / "sources" / "review" / "korea"
EXPECTED_SOURCES = {
    "KOR-001": "KOR-001_review_checklist.md",
    "KOR-002": "KOR-002_review_checklist.md",
    "KOR-003": "KOR-003_review_checklist.md",
    "KOR-004": "KOR-004_review_checklist.md",
    "KOR-005": "KOR-005_review_checklist.md",
}


def load_review_status() -> list[dict]:
    assert REVIEW_STATUS_PATH.exists()
    data = json.loads(REVIEW_STATUS_PATH.read_text(encoding="utf-8-sig"))
    assert isinstance(data, list)
    return data


def test_review_status_file_exists() -> None:
    assert REVIEW_STATUS_PATH.exists()


def test_review_status_includes_all_sources() -> None:
    entries = load_review_status()
    source_ids = {entry["sourceId"] for entry in entries}
    assert source_ids == set(EXPECTED_SOURCES)


def test_review_checklist_files_exist() -> None:
    for filename in EXPECTED_SOURCES.values():
        assert (REVIEW_DIR / filename).exists()


def test_review_checklists_include_source_id() -> None:
    for source_id, filename in EXPECTED_SOURCES.items():
        content = (REVIEW_DIR / filename).read_text(encoding="utf-8-sig")
        assert source_id in content


def test_review_checklists_include_policy_extraction_area() -> None:
    for filename in EXPECTED_SOURCES.values():
        content = (REVIEW_DIR / filename).read_text(encoding="utf-8-sig")
        assert "정책 추출 후보 영역" in content


def test_review_statuses_are_not_started() -> None:
    entries = load_review_status()
    assert {entry["reviewStatus"] for entry in entries} == {"not_started"}
