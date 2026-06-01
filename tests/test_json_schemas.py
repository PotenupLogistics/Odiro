from __future__ import annotations

import json
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SCHEMA_DIR = ROOT / "schemas"
EXPECTED_SCHEMAS = {
    "policy_config.schema.json",
    "world_config.schema.json",
    "decision_request.schema.json",
    "decision_response.schema.json",
    "evaluation_spec.schema.json",
    "run_result.schema.json",
}


def test_schema_files_exist() -> None:
    existing = {path.name for path in SCHEMA_DIR.glob("*.schema.json")}
    assert EXPECTED_SCHEMAS == existing


def test_schema_files_parse_and_define_required_fields() -> None:
    for schema_name in EXPECTED_SCHEMAS:
        schema = json.loads((SCHEMA_DIR / schema_name).read_text(encoding="utf-8-sig"))
        assert schema["type"] == "object"
        assert "schemaVersion" in schema["properties"]
        assert "schemaVersion" in schema["required"]
        assert schema["required"]


def test_policy_config_schema_contains_core_parameters_and_actions() -> None:
    schema = json.loads((SCHEMA_DIR / "policy_config.schema.json").read_text(encoding="utf-8-sig"))
    params = schema["properties"]["parameters"]["properties"]
    assert "maxSpeedKmh" in params
    assert "emergencyStopDistanceCm" in params
    assert "traversabilityThreshold" in params
    actions = schema["properties"]["availableActions"]["items"]["enum"]
    assert actions == [
        "Continue",
        "SlowDown",
        "Stop",
        "EmergencyStop",
        "LocalAvoidance",
        "ReplanPath",
        "YieldWait",
        "RequestOperator",
    ]


def test_decision_request_schema_contains_core_fields() -> None:
    schema = json.loads((SCHEMA_DIR / "decision_request.schema.json").read_text(encoding="utf-8-sig"))
    assert "detectedObjects" in schema["required"]
    assert "botState" in schema["required"]
    assert "terrain" in schema["properties"]
    assert "pathContext" in schema["properties"]


def test_decision_response_schema_contains_core_fields() -> None:
    schema = json.loads((SCHEMA_DIR / "decision_response.schema.json").read_text(encoding="utf-8-sig"))
    assert "selectedAction" in schema["required"]
    assert "command" in schema["required"]
    assert "appliedRules" in schema["properties"]


def test_sample_json_fixture_api_artifacts_are_not_created() -> None:
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
    assert not (ROOT / "fixtures").exists()
    assert not (ROOT / "app" / "api" / "main.py").exists()
