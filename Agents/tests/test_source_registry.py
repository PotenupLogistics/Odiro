from __future__ import annotations

import json
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
REGISTRY_PATH = ROOT / "data" / "sources" / "policy_source_registry.json"
EXPECTED_SOURCE_IDS = {
    "KOR-001",
    "KOR-002",
    "KOR-003",
    "KOR-004",
    "KOR-005",
}
REQUIRED_FIELDS = {
    "sourceId",
    "title",
    "filePath",
    "url",
    "sourceType",
    "accessType",
    "usagePurpose",
    "status",
    "notes",
}


def load_sources() -> list[dict]:
    assert REGISTRY_PATH.exists(), f"Registry file not found: {REGISTRY_PATH}"
    data = json.loads(REGISTRY_PATH.read_text(encoding="utf-8"))
    assert isinstance(data, list), "Registry must be a JSON list"
    return data


def test_registry_file_exists() -> None:
    assert REGISTRY_PATH.exists()


def test_registry_contains_five_sources() -> None:
    sources = load_sources()
    kor_sources = [source for source in sources if source["sourceId"].startswith("KOR-")]
    assert len(kor_sources) == 5


def test_registry_contains_expected_source_ids() -> None:
    sources = load_sources()
    source_ids = {
        source["sourceId"]
        for source in sources
        if source["sourceId"].startswith("KOR-")
    }
    assert source_ids == EXPECTED_SOURCE_IDS


def test_registry_file_paths_exist() -> None:
    sources = load_sources()
    for source in sources:
        if not source["filePath"]:
            continue
        file_path = ROOT / source["filePath"]
        assert file_path.exists(), f"Missing source file: {file_path}"


def test_registry_statuses_are_to_review() -> None:
    sources = load_sources()
    for source in sources:
        if not source["sourceId"].startswith("KOR-"):
            continue
        assert source["status"] == "to_review"


def test_registry_sources_have_required_fields() -> None:
    sources = load_sources()
    for source in sources:
        assert REQUIRED_FIELDS.issubset(source.keys())
