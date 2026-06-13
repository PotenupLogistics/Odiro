from __future__ import annotations

import json
import subprocess
import sys
from pathlib import Path
from typing import Any

from app.models.environment import EnvironmentSamplingRequest
from app.services.environment_parameter_catalog import get_environment_parameter_catalog
from app.services.environment_parameter_sampler import sample_environment_parameters


ROOT = Path(__file__).resolve().parents[2]
SCRIPT = ROOT / "scripts" / "sample_environment_parameters.py"
CONTRACT_SCHEMA_DIR = ROOT.parent / "contracts" / "schemas"


REQUIRED_PATHS = [
    ROOT / "app" / "models" / "environment.py",
    ROOT / "app" / "services" / "environment_parameter_catalog.py",
    ROOT / "app" / "services" / "environment_parameter_sampler.py",
    SCRIPT,
    ROOT / "docs" / "environment" / "ENVIRONMENT_SAMPLER_DESIGN.md",
]

FORBIDDEN_ARTIFACTS = [
    ROOT / "samples",
    ROOT / "fixtures",
    ROOT / "data" / "rag" / "vector_db",
    ROOT / "data" / "rag" / "embeddings",
    ROOT / "data" / "rag" / "chroma",
    ROOT / "ue",
    ROOT / "UE",
]

SCHEMA_PATHS = [
    CONTRACT_SCHEMA_DIR / "world_config.schema.json",
    CONTRACT_SCHEMA_DIR / "decision_request.schema.json",
    CONTRACT_SCHEMA_DIR / "decision_response.schema.json",
    CONTRACT_SCHEMA_DIR / "evaluation_spec.schema.json",
    CONTRACT_SCHEMA_DIR / "policy_config.schema.json",
    CONTRACT_SCHEMA_DIR / "run_result.schema.json",
]


def _read_text(path: Path) -> str:
    return path.read_text(encoding="utf-8-sig") if path.exists() else ""


def run_check() -> dict[str, Any]:
    sampler_text = _read_text(ROOT / "app" / "services" / "environment_parameter_sampler.py")
    cli_text = _read_text(SCRIPT)
    catalog = get_environment_parameter_catalog()
    deterministic_a = sample_environment_parameters(
        EnvironmentSamplingRequest(
            requestId="CHECK-ENV-SAMPLER-001",
            seed=1001,
            scenarioType="narrow_sidewalk_kickboard_crossing",
        )
    )
    deterministic_b = sample_environment_parameters(
        EnvironmentSamplingRequest(
            requestId="CHECK-ENV-SAMPLER-001",
            seed=1001,
            scenarioType="narrow_sidewalk_kickboard_crossing",
        )
    )
    help_result = subprocess.run(
        [sys.executable, str(SCRIPT), "--help"],
        cwd=ROOT,
        text=True,
        capture_output=True,
        check=False,
    )

    forbidden_artifacts = [
        path.relative_to(ROOT).as_posix() for path in FORBIDDEN_ARTIFACTS if path.exists()
    ]
    schema_files_present = all(path.exists() for path in SCHEMA_PATHS)
    result: dict[str, Any] = {
        "check": "environment_sampler",
        "passed": False,
        "warning": False,
        "requiredPathsExist": {
            path.relative_to(ROOT).as_posix(): path.exists() for path in REQUIRED_PATHS
        },
        "cliHelpWorks": help_result.returncode == 0 and "--scenario-type" in help_result.stdout,
        "deterministic": deterministic_a.parameters == deterministic_b.parameters,
        "usesNumericCatalog": all(
            all(isinstance(value, int | float) and not isinstance(value, bool) for value in values)
            for values in catalog.allowedValues.values()
        ),
        "forbiddenLabelValuesRejected": "FORBIDDEN_LABEL_VALUES" in sampler_text
        and "low" in sampler_text
        and "middle" in sampler_text
        and "high" in sampler_text,
        "noOpenAIImport": "openai" not in sampler_text.lower()
        and "openai" not in cli_text.lower(),
        "schemaFilesPresent": schema_files_present,
        "forbiddenArtifacts": forbidden_artifacts,
        "errors": [],
        "warnings": [],
    }

    missing_paths = [
        path for path, exists in result["requiredPathsExist"].items() if not exists
    ]
    if missing_paths:
        result["errors"].append(f"Missing environment sampler paths: {missing_paths}")
    if not result["cliHelpWorks"]:
        result["errors"].append("sample_environment_parameters.py --help failed.")
    if not result["deterministic"]:
        result["errors"].append("Sampler is not deterministic for same seed and scenarioType.")
    if not result["usesNumericCatalog"]:
        result["errors"].append("Catalog contains non-numeric values.")
    if not result["forbiddenLabelValuesRejected"]:
        result["errors"].append("Sampler does not explicitly reject low/middle/high fixed values.")
    if not result["noOpenAIImport"]:
        result["errors"].append("Environment sampler imports or references OpenAI.")
    if not result["schemaFilesPresent"]:
        result["errors"].append("Expected JSON Schema files are missing.")
    if forbidden_artifacts:
        result["warnings"].append("Forbidden sample/fixture/vector/embedding/UE artifacts detected.")

    result["passed"] = not result["errors"]
    result["warning"] = result["passed"] and bool(result["warnings"])
    return result


def main() -> int:
    result = run_check()
    print(json.dumps(result, ensure_ascii=False, indent=2))
    return 0 if result["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
