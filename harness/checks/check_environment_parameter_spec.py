from __future__ import annotations

import json
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[2]
SPEC = ROOT / "docs" / "environment" / "ENVIRONMENT_PARAMETER_SPEC.md"
ADAPTER_DOC = ROOT / "docs" / "archive" / "previous_episode_spec" / "UE5_EPISODE_SPEC_ADAPTER.md"
FIELD_MAPPING_DOC = ROOT / "docs" / "archive" / "previous_episode_spec" / "UE5_WORLD_CONFIG_FIELD_MAPPING.md"


REQUIRED_TERMS = [
    "sidewalkWidthCm",
    "pedestrianCount",
    "obstacleBlockingRatio",
    "pedestrianSpeedMps",
    "Do not use low / middle / high as JSON values",
    "same label must map to the same numeric value",
    "sidewalkWidthCm=100 or 120",
    "pedestrianCount=5",
    'pedestrianDensity: "high"',
    "sidewalkWidthCm: 120",
]


def _detect_forbidden_artifacts() -> list[str]:
    forbidden_paths = [
        ROOT / "samples",
        ROOT / "fixtures",
        ROOT / "data" / "rag" / "vector_db",
        ROOT / "data" / "rag" / "embeddings",
        ROOT / "data" / "rag" / "chroma",
        ROOT / "ue",
        ROOT / "UE",
    ]
    return sorted(path.relative_to(ROOT).as_posix() for path in forbidden_paths if path.exists())


def run_check() -> dict[str, Any]:
    spec_text = SPEC.read_text(encoding="utf-8-sig") if SPEC.exists() else ""
    adapter_text = ADAPTER_DOC.read_text(encoding="utf-8-sig") if ADAPTER_DOC.exists() else ""
    field_mapping_text = (
        FIELD_MAPPING_DOC.read_text(encoding="utf-8-sig") if FIELD_MAPPING_DOC.exists() else ""
    )

    missing_terms = [term for term in REQUIRED_TERMS if term not in spec_text]
    result: dict[str, Any] = {
        "check": "environment_parameter_spec",
        "passed": False,
        "warning": False,
        "specExists": SPEC.exists(),
        "missingTerms": missing_terms,
        "adapterMentionsCmToM": "cm to m" in adapter_text,
        "adapterMentionsBlockingRatio": "properties.blocking_ratio" in adapter_text,
        "fieldMappingMentionsConversion": "Environment Parameter Conversion" in field_mapping_text,
        "fieldMappingMentionsLengthConversion": "map.lengthCm" in field_mapping_text
        and "12.0" in field_mapping_text,
        "readmeLinksSpec": "ENVIRONMENT_PARAMETER_SPEC.md"
        in (ROOT / "README.md").read_text(encoding="utf-8-sig"),
        "docsReadmeLinksSpec": "ENVIRONMENT_PARAMETER_SPEC.md"
        in (ROOT / "docs" / "README.md").read_text(encoding="utf-8-sig"),
        "forbiddenArtifacts": [],
        "errors": [],
        "warnings": [],
    }

    if not result["specExists"]:
        result["errors"].append("ENVIRONMENT_PARAMETER_SPEC.md is missing.")
    if missing_terms:
        result["errors"].append("ENVIRONMENT_PARAMETER_SPEC.md is missing required terms.")
    if not result["adapterMentionsCmToM"]:
        result["errors"].append("UE5_EPISODE_SPEC_ADAPTER.md does not document cm to m conversion.")
    if not result["adapterMentionsBlockingRatio"]:
        result["errors"].append("UE5_EPISODE_SPEC_ADAPTER.md does not document blocking ratio mapping.")
    if not result["fieldMappingMentionsConversion"]:
        result["errors"].append("UE5_WORLD_CONFIG_FIELD_MAPPING.md does not document parameter conversion.")
    if not result["fieldMappingMentionsLengthConversion"]:
        result["errors"].append("UE5_WORLD_CONFIG_FIELD_MAPPING.md does not document map length conversion.")
    if not result["readmeLinksSpec"]:
        result["errors"].append("README.md does not link ENVIRONMENT_PARAMETER_SPEC.md.")
    if not result["docsReadmeLinksSpec"]:
        result["errors"].append("docs/README.md does not link ENVIRONMENT_PARAMETER_SPEC.md.")

    result["forbiddenArtifacts"] = _detect_forbidden_artifacts()
    if result["forbiddenArtifacts"]:
        result["warnings"].append("Forbidden sample/fixture/vector/embedding/UE code artifacts detected.")

    result["passed"] = not result["errors"]
    result["warning"] = result["passed"] and bool(result["warnings"])
    return result


def main() -> int:
    result = run_check()
    print(json.dumps(result, ensure_ascii=False, indent=2))
    return 0 if result["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
