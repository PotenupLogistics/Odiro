from __future__ import annotations

import json
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[2]

INTENT_EXTRACTOR = ROOT / "app" / "services" / "world_config_scenario_intent_extractor.py"
POST_PROCESSOR = ROOT / "app" / "services" / "world_config_scenario_post_processor.py"
HANDOFF_MODEL = ROOT / "app" / "models" / "handoff.py"
HANDOFF_SERVICE = ROOT / "app" / "services" / "ue5_world_config_handoff_service.py"
EPISODE_REFLECTION = ROOT / "app" / "services" / "episode_spec_scenario_reflection.py"
DOCS = [
    ROOT / "docs" / "architecture" / "SCENARIO_INTENT_EXTRACTION.md",
    ROOT / "docs" / "architecture" / "SCENARIO_REFLECTION_VALIDATION.md",
    ROOT / "docs" / "architecture" / "SCENARIO_POST_PROCESSING.md",
    ROOT / "docs" / "handoff" / "UE5_ENDPOINT_USAGE_FOR_UE_TEAM.md",
    ROOT / "docs" / "tooling" / "API_SHELL_GUIDE.md",
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
    intent_text = _read(INTENT_EXTRACTOR)
    post_text = _read(POST_PROCESSOR)
    handoff_model_text = _read(HANDOFF_MODEL)
    handoff_service_text = _read(HANDOFF_SERVICE)
    episode_reflection_text = _read(EPISODE_REFLECTION)
    docs_text = "\n".join(_read(path) for path in DOCS)
    forbidden = [path.relative_to(ROOT).as_posix() for path in FORBIDDEN_ARTIFACTS if path.exists()]

    result: dict[str, Any] = {
        "check": "generic_obstacle_scenario",
        "passed": False,
        "warning": False,
        "intentRecognizesGenericObstacle": all(
            term in intent_text
            for term in ["정적 장애물", "경로 중앙", "blockingRatio", "explicitNoPedestrian"]
        ),
        "postProcessorAddsGenericObstacle": "add_generic_obstacle" in post_text,
        "postProcessorPreservesNumericPromptValues": all(
            term in post_text
            for term in [
                "set_sidewalk_width_from_prompt",
                "set_obstacle_position_from_prompt",
                "set_obstacle_blocking_ratio_from_prompt",
                "remove_pedestrians_for_no_pedestrian_prompt",
            ]
        ),
        "handoffDefaultEpisodeSpec": "responseFormat:" in handoff_model_text
        and '"episode_spec"' in handoff_model_text
        and '= "episode_spec"' in handoff_model_text,
        "diagnosticsIncludeEffectiveResponseFormat": "effectiveResponseFormat" in handoff_service_text,
        "episodeReflectionAllowsNoPedestrian": "expectsNoPedestrian" in episode_reflection_text,
        "docsMentionScenarioGenerateAndRemovedLegacyRoute": "/api/v1/scenarios/generate" in docs_text
        and (
            "FastAPI/OpenAPI에서 제거" in docs_text
            or "FastAPI route와 OpenAPI에서 제거" in docs_text
        ),
        "forbiddenArtifacts": forbidden,
        "schemaFilesPresent": (ROOT / "schemas" / "world_config.schema.json").exists(),
        "errors": [],
        "warnings": [],
    }

    for key, message in [
        ("intentRecognizesGenericObstacle", "Scenario intent extractor must recognize generic/static obstacle prompts."),
        ("postProcessorAddsGenericObstacle", "Scenario post-processor must support add_generic_obstacle."),
        ("postProcessorPreservesNumericPromptValues", "Scenario post-processor must preserve explicit numeric/no-pedestrian prompt values."),
        ("handoffDefaultEpisodeSpec", "UE handoff request default responseFormat must be episode_spec."),
        ("diagnosticsIncludeEffectiveResponseFormat", "UE handoff diagnostics must include effectiveResponseFormat."),
        ("episodeReflectionAllowsNoPedestrian", "EpisodeSpec scenario reflection must support no-pedestrian prompts."),
        ("docsMentionScenarioGenerateAndRemovedLegacyRoute", "Docs must describe scenario generation and removed legacy handoff route."),
        ("schemaFilesPresent", "world_config JSON Schema file is missing."),
    ]:
        if not result[key]:
            result["errors"].append(message)
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
