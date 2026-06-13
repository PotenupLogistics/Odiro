from __future__ import annotations

import json
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[2]
CONTRACT_SCHEMA_DIR = ROOT.parent / "contracts" / "schemas"
BUILDER = ROOT / "app" / "services" / "environment_generation_constraints_builder.py"
GUIDE = ROOT / "docs" / "environment" / "ENVIRONMENT_SAMPLER_GENERATION_INTEGRATION.md"
GENERATION_MODEL = ROOT / "app" / "models" / "generation.py"
PROMPT_BUILDER = ROOT / "app" / "services" / "world_config_prompt_builder.py"
POST_PROCESSOR = ROOT / "app" / "services" / "world_config_scenario_post_processor.py"
EPISODE_REFLECTION = ROOT / "app" / "services" / "episode_spec_scenario_reflection.py"
HANDOFF_SERVICE = ROOT / "app" / "services" / "ue5_world_config_handoff_service.py"
EXPORT_SCRIPT = ROOT / "scripts" / "export_ue5_handoff_payload.py"
SMOKE_SCRIPT = ROOT / "scripts" / "run_ue5_handoff_smoke.py"

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
    builder_text = _read(BUILDER)
    guide_text = _read(GUIDE)
    generation_text = _read(GENERATION_MODEL)
    prompt_text = _read(PROMPT_BUILDER)
    post_text = _read(POST_PROCESSOR)
    episode_reflection_text = _read(EPISODE_REFLECTION)
    handoff_text = _read(HANDOFF_SERVICE)
    cli_text = _read(EXPORT_SCRIPT) + "\n" + _read(SMOKE_SCRIPT)
    forbidden = [path.relative_to(ROOT).as_posix() for path in FORBIDDEN_ARTIFACTS if path.exists()]
    result: dict[str, Any] = {
        "check": "environment_sampler_generation_integration",
        "passed": False,
        "warning": False,
        "builderExists": BUILDER.exists(),
        "guideExists": GUIDE.exists(),
        "generationModelHasEnvironmentSampling": "environmentSampling" in generation_text,
        "builderCallsSampler": "sample_environment_parameters" in builder_text,
        "promptHasNumericEnvironmentConstraints": "Numeric Environment Constraints" in prompt_text,
        "postProcessorHasSamplerPatchTypes": all(
            term in post_text
            for term in [
                "set_sidewalk_width_from_environment_sampler",
                "set_obstacle_blocking_ratio_from_environment_sampler",
                "set_runtime_limit_from_environment_sampler",
            ]
        ),
        "episodeReflectionEnforcesObstacleAndBlocking": all(
            term in episode_reflection_text
            for term in [
                "missing_static_obstacle",
                "missing_blocking_ratio",
                "environment_sidewalk_width_mismatch",
                "ueCompilerReadiness",
            ]
        ),
        "handoffRequiresEpisodeReflectionReadiness": "ueCompilerReadiness" in handoff_text
        and "staticObstacleCount == 0" in handoff_text,
        "cliHasEnvironmentSamplingOptions": all(
            term in cli_text for term in ["--environment-sampling", "--scenario-type", "--seed", "--fixed"]
        ),
        "docsStateNoDoe": "DOE matrix" in guide_text and "DOE/batch는 후속 단계" in guide_text,
        "forbiddenArtifacts": forbidden,
        "schemaFilesPresent": (CONTRACT_SCHEMA_DIR / "world_config.schema.json").exists(),
        "errors": [],
        "warnings": [],
    }

    for key, message in [
        ("builderExists", "app/services/environment_generation_constraints_builder.py is missing."),
        ("guideExists", "docs/environment/ENVIRONMENT_SAMPLER_GENERATION_INTEGRATION.md is missing."),
        ("generationModelHasEnvironmentSampling", "generation.py must include environmentSampling constraints."),
        ("builderCallsSampler", "environment generation builder must call sample_environment_parameters."),
        ("promptHasNumericEnvironmentConstraints", "Prompt builder must include Numeric Environment Constraints."),
        ("postProcessorHasSamplerPatchTypes", "Post-processor must include environment sampler patch types."),
        ("episodeReflectionEnforcesObstacleAndBlocking", "EpisodeSpec reflection must fail missing static obstacles/blocking ratio/environment width."),
        ("handoffRequiresEpisodeReflectionReadiness", "Handoff success must require EpisodeSpec reflection readiness."),
        ("cliHasEnvironmentSamplingOptions", "CLI must expose environment sampling options."),
        ("docsStateNoDoe", "Docs must state sampler integration is not DOE/batch generation."),
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
