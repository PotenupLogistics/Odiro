from __future__ import annotations

import json
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
PROCESSED_DIR = ROOT / "data" / "sources" / "processed" / "korea"
REPORT_PATH = ROOT / "data" / "sources" / "processed" / "source_processing_report.json"
EXPECTED_SOURCES = {
    "KOR-001": "KOR-001_지능형로봇법.md",
    "KOR-002": "KOR-002_도로교통법_실외이동로봇.md",
    "KOR-003": "KOR-003_KIRIA_실외이동로봇_운행안전인증_가이드북.md",
    "KOR-004": "KOR-004_산업통상자원부_운행안전인증_절차_및_기준_고시.md",
    "KOR-005": "KOR-005_도로교통법_제2조_하위법령_운행기준_참고자료.md",
}


def test_processed_markdown_files_exist() -> None:
    for filename in EXPECTED_SOURCES.values():
        assert (PROCESSED_DIR / filename).exists()


def test_processed_markdown_files_include_source_id() -> None:
    for source_id, filename in EXPECTED_SOURCES.items():
        content = (PROCESSED_DIR / filename).read_text(encoding="utf-8-sig")
        assert source_id in content


def test_processed_markdown_files_include_extraction_status() -> None:
    for filename in EXPECTED_SOURCES.values():
        content = (PROCESSED_DIR / filename).read_text(encoding="utf-8-sig")
        assert "extractionStatus:" in content


def test_source_processing_report_exists() -> None:
    assert REPORT_PATH.exists()


def test_source_processing_report_includes_all_sources() -> None:
    report = json.loads(REPORT_PATH.read_text(encoding="utf-8-sig"))
    entries = report["sources"]
    source_ids = {entry["sourceId"] for entry in entries}
    assert source_ids == set(EXPECTED_SOURCES)
