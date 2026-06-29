from __future__ import annotations

import json
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
POLICY_CARDS_PATH = ROOT / "data" / "rag" / "policy_knowledge_cards.jsonl"
COVERAGE_DOC = ROOT / "docs" / "policy" / "POLICY_CARD_COVERAGE.md"
PARAMETER_DOC = ROOT / "docs" / "policy" / "POLICY_PARAMETER_CATALOG.md"
ACTION_DOC = ROOT / "docs" / "policy" / "DECISION_ACTION_MAPPING.md"
REQUEST_FIELD_DOC = ROOT / "docs" / "policy" / "DECISION_REQUEST_FIELD_MAPPING.md"
COVERAGE_REPORT_JSON = ROOT / "data" / "rag" / "policy_card_coverage_report.json"
COVERAGE_REPORT_MD = ROOT / "data" / "rag" / "policy_card_coverage_report.md"


def test_policy_card_count_is_eleven() -> None:
    cards = [
        json.loads(line)
        for line in POLICY_CARDS_PATH.read_text(encoding="utf-8-sig").splitlines()
        if line.strip()
    ]
    assert len(cards) == 11


def test_coverage_report_exists() -> None:
    assert COVERAGE_REPORT_JSON.exists()
    assert COVERAGE_REPORT_MD.exists()
    report = json.loads(COVERAGE_REPORT_JSON.read_text(encoding="utf-8-sig"))
    assert report["totalCards"] == 11
    assert report["cardsByCategory"]["crosswalk_operation"] == 1
    assert report["cardsByCategory"]["speed_policy"] == 3


def test_mapping_documents_exist_and_reference_cards() -> None:
    for path in [COVERAGE_DOC, PARAMETER_DOC, ACTION_DOC, REQUEST_FIELD_DOC]:
        assert path.exists()
        assert "CARD-KOR-003" in path.read_text(encoding="utf-8-sig")


def test_parameter_catalog_contains_core_parameters() -> None:
    text = PARAMETER_DOC.read_text(encoding="utf-8-sig")
    assert "maxSpeedKmh" in text
    assert "emergencyStopDistanceCm" in text
    assert "traversabilityThreshold" in text


def test_decision_action_mapping_contains_core_actions() -> None:
    text = ACTION_DOC.read_text(encoding="utf-8-sig")
    assert "EmergencyStop" in text
    assert "SlowDown" in text
    assert "RequestOperator" in text


def test_decision_request_field_mapping_contains_core_fields() -> None:
    text = REQUEST_FIELD_DOC.read_text(encoding="utf-8-sig")
    assert "detectedObjects[].distanceCm" in text
    assert "botState.speedKmh" in text
    assert "terrain.traversabilityScore" in text


def test_sample_fixture_api_artifacts_are_not_created() -> None:
    forbidden_names = {
        "world_config.json",
        "policy_config.json",
        "decision_request.json",
        "decision_response.json",
    }
    for folder in ["data", "docs", "harness", "scripts", "tests", "app"]:
        root = ROOT / folder
        for path in root.rglob("*"):
            assert path.name not in forbidden_names
