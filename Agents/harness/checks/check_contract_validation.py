from __future__ import annotations

import json
from pathlib import Path
from typing import Any

from app.core.contract_types import CONTRACT_MODELS, CONTRACT_SCHEMA_FILES, ContractType


ROOT = Path(__file__).resolve().parents[2]
POLICY_CARDS_PATH = ROOT / "data" / "rag" / "policy_knowledge_cards.jsonl"
EXPECTED_TYPES = {
    "policy_config",
    "world_config",
    "decision_request",
    "decision_response",
    "evaluation_spec",
    "run_result",
}
EXPECTED_POLICY_CARD_COUNT = 11
REQUIRED_FILES = {
    "app/core/contract_types.py",
    "app/services/json_contract_validator.py",
    "scripts/validate_contract.py",
}


def _base_result() -> dict[str, Any]:
    return {
        "check": "contract_validation",
        "passed": False,
        "warning": False,
        "missingFiles": [],
        "contractTypes": [],
        "missingContractTypes": [],
        "missingSchemaMappings": [],
        "missingModelMappings": [],
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
    if (ROOT / "samples").exists():
        found.append("samples/")
    if (ROOT / "fixtures").exists():
        found.append("fixtures/")
    return sorted(set(found))


def run_check() -> dict[str, Any]:
    result = _base_result()
    result["missingFiles"] = sorted(path for path in REQUIRED_FILES if not (ROOT / path).exists())

    contract_types = {item.value for item in ContractType}
    result["contractTypes"] = sorted(contract_types)
    result["missingContractTypes"] = sorted(EXPECTED_TYPES - contract_types)

    for contract_type in ContractType:
        schema_path = CONTRACT_SCHEMA_FILES.get(contract_type)
        model = CONTRACT_MODELS.get(contract_type)
        if schema_path is None or not schema_path.exists():
            result["missingSchemaMappings"].append(contract_type.value)
        if model is None:
            result["missingModelMappings"].append(contract_type.value)

    if POLICY_CARDS_PATH.exists():
        result["policyCardCount"] = sum(
            1 for line in POLICY_CARDS_PATH.read_text(encoding="utf-8-sig").splitlines() if line.strip()
        )
    if result["policyCardCount"] != EXPECTED_POLICY_CARD_COUNT:
        result["errors"].append(f"policy card count must remain {EXPECTED_POLICY_CARD_COUNT}.")

    forbidden = _detect_forbidden_artifacts()
    result["generatedArtifactsWarnings"] = forbidden
    if forbidden:
        result["warnings"].append("Forbidden sample/fixture artifacts detected.")

    result["passed"] = not any(
        [
            result["errors"],
            result["missingFiles"],
            result["missingContractTypes"],
            result["missingSchemaMappings"],
            result["missingModelMappings"],
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
