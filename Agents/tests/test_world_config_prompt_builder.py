from __future__ import annotations

from pathlib import Path

from app.models.generation import (
    WorldConfigGenerationConstraints,
    WorldConfigGenerationRequest,
)
from app.services.world_config_prompt_builder import (
    build_world_config_prompt_package,
    build_world_config_repair_prompt_package,
)


ROOT = Path(__file__).resolve().parents[1]


def _request() -> WorldConfigGenerationRequest:
    return WorldConfigGenerationRequest(
        schemaVersion="1.0",
        requestId="REQ-PROMPT-001",
        generationType="world_config",
        prompt="\uc881\uc740 \ubcf4\ub3c4\uc5d0\uc11c \ud0a5\ubcf4\ub4dc\uac00 \ub9c9\uace0 \ubcf4\ud589\uc790\uac00 \ud6a1\ub2e8",
        targetContractType="world_config",
        policyId="POLICY-MVP",
        constraints=WorldConfigGenerationConstraints(
            unitSystem="cm_kmh_sec_degree",
            allowedMapTypes=["sidewalk"],
            allowedObjectTypes=["Pedestrian", "Obstacle", "Kickboard"],
            fixedPolicyId="POLICY-MVP",
            defaultSeed=7,
            requireValidation=True,
        ),
        maxRepairAttempts=2,
    )


def test_prompt_builder_creates_system_and_user_prompt() -> None:
    package = build_world_config_prompt_package(_request())

    assert package.requestId == "REQ-PROMPT-001"
    assert "JSON object only" in package.systemPrompt
    assert "cm, kmh, sec, degree" in package.systemPrompt
    assert "world_config.schema.json" in package.systemPrompt
    assert "Do not claim certification" in package.systemPrompt
    assert "World Config required fields" in package.userPrompt
    assert "Output Contract" in package.systemPrompt
    assert "map.lengthCm" in package.systemPrompt + package.userPrompt
    assert "robot.botId" in package.systemPrompt + package.userPrompt
    assert "runtime.maxDurationSec" in package.systemPrompt + package.userPrompt
    assert "Extra keys are not allowed" in package.systemPrompt + package.userPrompt
    assert "You must return exactly one JSON object" in package.systemPrompt
    assert "Do not invent keys outside the schema" in package.systemPrompt
    assert package.retrievedContexts == []
    assert "No related policy RAG chunks were retrieved." in package.warnings


def test_prompt_builder_includes_scenario_requirements_for_korean_prompt() -> None:
    request = _request()
    request.prompt = "좁은 보도에서 공유 킥보드가 로봇 경로를 막고, 오른쪽에서 보행자가 횡단하는 상황을 만들어줘."

    package = build_world_config_prompt_package(request)

    assert "Scenario Intent Summary" in package.userPrompt
    assert "Scenario Requirements" in package.userPrompt
    assert "type Kickboard" in package.userPrompt
    assert "pedestrian crossing" in package.userPrompt
    assert "obstacles[].type" in package.userPrompt
    assert "pedestrians[].behavior" in package.userPrompt


def test_prompt_builder_includes_numeric_environment_constraints() -> None:
    request = _request()
    request.constraints.environmentSampling = {
        "enabled": True,
        "seed": 1001,
        "scenarioType": "obstacle_ahead",
        "fixedParameters": {
            "sidewalkWidthCm": 120,
            "obstacleBlockingRatio": 0.6,
            "timeLimitSec": 60,
        },
    }

    package = build_world_config_prompt_package(request, compact_prompt=True)

    assert "Numeric Environment Constraints" in package.userPrompt
    assert "map.sidewalkWidthCm must be 120" in package.userPrompt
    assert "obstacleBlockingRatio must be 0.6" in package.userPrompt
    assert "runtime.maxDurationSec must be 60" in package.userPrompt
    assert "Never use low/middle/high as JSON values." in package.userPrompt
    assert package.environmentSampling is not None
    assert package.environmentSampling["parameters"]["sidewalkWidthCm"] == 120


def test_prompt_builder_includes_server_robot_profile_constraints() -> None:
    package = build_world_config_prompt_package(_request(), compact_prompt=True)
    combined_prompt = package.systemPrompt + package.userPrompt

    assert "Robot physical size" in combined_prompt
    assert "width: 0.44 m" in combined_prompt
    assert "depth/length: 1.00 m" in combined_prompt
    assert "height: 0.64 m" in combined_prompt
    assert "robot width plus safety margin" in combined_prompt
    assert "fixed server-side constraint" in combined_prompt


def test_compact_prompt_still_contains_required_field_checklist() -> None:
    package = build_world_config_prompt_package(_request(), context_top_k=2, compact_prompt=True)

    assert "Required field checklist" in package.systemPrompt + package.userPrompt
    assert "map.sidewalkWidthCm" in package.systemPrompt + package.userPrompt
    assert "robot.goal.x" in package.systemPrompt + package.userPrompt
    assert len(package.retrievedContexts) <= 2


def test_repair_prompt_includes_validation_errors() -> None:
    package = build_world_config_repair_prompt_package(
        _request(),
        invalid_payload={"schemaVersion": "1.0"},
        validation_errors=["worldId is required", "seed must be an integer"],
    )

    assert package.requestId == "REQ-PROMPT-001"
    assert "worldId is required" in package.repairPrompt
    assert "seed must be an integer" in package.repairPrompt
    assert "JSON object only" in package.repairPrompt
    assert "Missing required fields" in package.repairPrompt
    assert "Remove schema-extra fields" in package.repairPrompt
    assert "Return the corrected JSON object only" in package.repairPrompt
    assert "Preserve valid parts from the previous JSON" in package.repairPrompt
    assert "Satisfy the scenario requirements" in package.repairPrompt


def test_no_sample_fixture_vector_or_embedding_artifacts_exist() -> None:
    assert not (ROOT / "samples").exists()
    assert not (ROOT / "fixtures").exists()
    assert not (ROOT / "data" / "rag" / "vector_db").exists()
    assert not (ROOT / "data" / "rag" / "embeddings").exists()
