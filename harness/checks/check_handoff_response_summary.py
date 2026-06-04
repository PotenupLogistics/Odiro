from __future__ import annotations

import json
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[2]
HELPER = ROOT / "app" / "utils" / "handoff_response_summary.py"
GUIDE = ROOT / "docs" / "handoff" / "GENERIC_OBSTACLE_SMOKE_REPORTING.md"
SCRIPT_PATHS = [
    ROOT / "scripts" / "run_ue5_handoff_smoke.py",
    ROOT / "scripts" / "run_ue5_episode_spec_controlled_smoke.py",
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


def _read(path: Path) -> str:
    return path.read_text(encoding="utf-8-sig") if path.exists() else ""


def run_check() -> dict[str, Any]:
    helper_text = _read(HELPER)
    guide_text = _read(GUIDE)
    forbidden = [path.relative_to(ROOT).as_posix() for path in FORBIDDEN_ARTIFACTS if path.exists()]
    scripts_use_helper = {
        path.relative_to(ROOT).as_posix(): "summarize_handoff_response" in _read(path)
        for path in SCRIPT_PATHS
    }
    result: dict[str, Any] = {
        "check": "handoff_response_summary",
        "passed": False,
        "warning": False,
        "helperExists": HELPER.exists(),
        "guideExists": GUIDE.exists(),
        "scriptsUseSummaryHelper": scripts_use_helper,
        "helperAvoidsFullPayloadKeys": '"worldConfig":' not in helper_text
        and '"episodeSpec":' not in helper_text
        and "'worldConfig':" not in helper_text
        and "'episodeSpec':" not in helper_text,
        "helperExtractsGenericObstacleFields": all(
            term in helper_text
            for term in [
                "sidewalkWidthCm",
                "blockingRatio",
                "staticObstacleBlockingRatio",
                "penaltiesFieldAbsent",
                "obstacleLocation",
                "obstacleNearRouteMidpoint",
                "checkedRequirementsCount",
                "appliedPatches",
            ]
        ),
        "docsStateNoFullPayload": "full payload 저장 금지" in guide_text
        and "API key 저장 금지" in guide_text,
        "forbiddenArtifacts": forbidden,
        "schemaFilesPresent": (ROOT / "schemas" / "world_config.schema.json").exists(),
        "errors": [],
        "warnings": [],
    }

    for key, message in [
        ("helperExists", "app/utils/handoff_response_summary.py is missing."),
        ("guideExists", "docs/handoff/GENERIC_OBSTACLE_SMOKE_REPORTING.md is missing."),
        ("helperAvoidsFullPayloadKeys", "Summary helper must not return full worldConfig or episodeSpec keys."),
        ("helperExtractsGenericObstacleFields", "Summary helper must extract generic obstacle smoke fields."),
        ("docsStateNoFullPayload", "Generic obstacle smoke reporting guide must state no full payload/API key storage."),
        ("schemaFilesPresent", "world_config JSON Schema file is missing."),
    ]:
        if not result[key]:
            result["errors"].append(message)
    missing_helper = [path for path, uses in scripts_use_helper.items() if not uses]
    if missing_helper:
        result["errors"].append(f"Smoke scripts missing summarize_handoff_response usage: {missing_helper}")
    if forbidden:
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
