from __future__ import annotations

import json
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
REGISTRY_PATH = ROOT / "data" / "sources" / "policy_source_registry.json"
EXPECTED_KOR_IDS = {"KOR-001", "KOR-002", "KOR-003", "KOR-004", "KOR-005"}
EXPECTED_RSR_IDS = {"RSR-001", "RSR-002", "RSR-003", "RSR-004", "RSR-005", "RSR-006"}


def load_sources() -> list[dict]:
    return json.loads(REGISTRY_PATH.read_text(encoding="utf-8-sig"))


def test_registry_includes_research_sources() -> None:
    sources = load_sources()
    source_ids = {source["sourceId"] for source in sources}
    assert EXPECTED_RSR_IDS.issubset(source_ids)


def test_research_sources_have_urls() -> None:
    sources = {source["sourceId"]: source for source in load_sources()}
    for source_id in EXPECTED_RSR_IDS:
        assert sources[source_id]["url"]


def test_research_sources_are_to_review() -> None:
    sources = {source["sourceId"]: source for source in load_sources()}
    for source_id in EXPECTED_RSR_IDS:
        assert sources[source_id]["status"] == "to_review"


def test_research_sources_have_usage_purpose() -> None:
    sources = {source["sourceId"]: source for source in load_sources()}
    for source_id in EXPECTED_RSR_IDS:
        usage_purpose = sources[source_id]["usagePurpose"]
        assert isinstance(usage_purpose, list)
        assert usage_purpose


def test_existing_kor_sources_are_still_registered() -> None:
    sources = load_sources()
    source_ids = {source["sourceId"] for source in sources}
    assert EXPECTED_KOR_IDS.issubset(source_ids)
