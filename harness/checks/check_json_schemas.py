from __future__ import annotations

import json
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[2]
SCHEMA_DIR = ROOT / "schemas"
POLICY_CARDS_PATH = ROOT / "data" / "rag" / "policy_knowledge_cards.jsonl"
EXPECTED_SCHEMAS = {
    "policy_config.schema.json",
    "world_config.schema.json",
    "decision_request.schema.json",
    "decision_response.schema.json",
    "evaluation_spec.schema.json",
    "run_result.schema.json",
}
EXPECTED_MODEL_FILES = {
    "app/models/policy.py",
    "app/models/world.py",
    "app/models/decision.py",
    "app/models/evaluation.py",
    "app/models/run_result.py",
}


def _base_result() -> dict[str, Any]:
    return {
        "check": "json_schemas",
        "passed": False,
        "warning": False,
        "schemaCount": 0,
        "missingSchemas": [],
        "jsonParseErrors": [],
        "schemasMissingSchemaVersion": [],
        "schemasMissingRequired": [],
        "missingModelFiles": [],
        "policyCardCount": 0,
        "generatedArtifactsWarnings": [],
        "warnings": [],
        "errors": [],
    }


def _detect_forbidden_artifacts() -> list[str]:
    names = {
        "world_config.json",
        "policy_config.json",
        "decision_request.json",
        "decision_response.json",
    }
    found: list[str] = []
    for folder in ["data", "docs", "harness", "scripts", "tests", "app"]:
        root = ROOT / folder
        if not root.exists():
            continue
        for path in root.rglob("*"):
            if path.is_file() and path.name in names:
                found.append(path.relative_to(ROOT).as_posix())
    if (ROOT / "fixtures").exists():
        found.append("fixtures/")
    return sorted(set(found))


def run_check() -> dict[str, Any]:
    result = _base_result()

    existing_schemas = {path.name for path in SCHEMA_DIR.glob("*.schema.json")}
    result["schemaCount"] = len(existing_schemas)
    result["missingSchemas"] = sorted(EXPECTED_SCHEMAS - existing_schemas)

    for schema_name in sorted(EXPECTED_SCHEMAS & existing_schemas):
        schema_path = SCHEMA_DIR / schema_name
        try:
            schema = json.loads(schema_path.read_text(encoding="utf-8-sig"))
        except json.JSONDecodeError as exc:
            result["jsonParseErrors"].append({"schema": schema_name, "error": str(exc)})
            continue
        if "schemaVersion" not in schema.get("properties", {}):
            result["schemasMissingSchemaVersion"].append(schema_name)
        if not schema.get("required"):
            result["schemasMissingRequired"].append(schema_name)

    result["missingModelFiles"] = sorted(
        path for path in EXPECTED_MODEL_FILES if not (ROOT / path).exists()
    )

    if POLICY_CARDS_PATH.exists():
        result["policyCardCount"] = sum(
            1 for line in POLICY_CARDS_PATH.read_text(encoding="utf-8-sig").splitlines() if line.strip()
        )
    if result["policyCardCount"] != 9:
        result["errors"].append("policy card count must remain 9.")

    forbidden = _detect_forbidden_artifacts()
    result["generatedArtifactsWarnings"] = forbidden
    if forbidden:
        result["warnings"].append("Forbidden sample/fixture artifacts detected.")

    result["passed"] = not any(
        [
            result["errors"],
            result["missingSchemas"],
            result["jsonParseErrors"],
            result["schemasMissingSchemaVersion"],
            result["schemasMissingRequired"],
            result["missingModelFiles"],
        ]
    )
    result["warning"] = result["passed"] and bool(result["warnings"])
    return result


def main() -> int:
    result = run_check()
    print(json.dumps(result, ensure_ascii=False, indent=2))
    return 0 if result["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
