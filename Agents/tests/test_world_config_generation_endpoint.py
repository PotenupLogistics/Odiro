from __future__ import annotations

from pathlib import Path

from app.models.generation import WorldConfigGenerationRequest
from app.models.generation import WorldConfigGenerationResult, WorldConfigValidationSummary
from app.models.llm import LlmProvider
from app.models.scenario import ScenarioPostProcessPatch, ScenarioPostProcessResult
from app.services.world_config_generation_orchestrator import generate_world_config
from app.services.world_config_prompt_builder import build_world_config_prompt_package


ROOT = Path(__file__).resolve().parents[1]


def _generation_request() -> WorldConfigGenerationRequest:
    return WorldConfigGenerationRequest.model_validate({
        "schemaVersion": "1.0",
        "requestId": "REQ-GENERATE-ENDPOINT-001",
        "generationType": "world_config",
        "prompt": "좁은 보도에서 킥보드가 경로를 막고 보행자가 횡단",
        "targetContractType": "world_config",
        "policyId": "POLICY-MVP",
        "constraints": {
            "unitSystem": "cm_kmh_sec_degree",
            "allowedMapTypes": ["sidewalk"],
            "allowedObjectTypes": ["Pedestrian", "Obstacle", "Kickboard"],
            "fixedPolicyId": "POLICY-MVP",
            "defaultSeed": 12,
            "requireValidation": True,
        },
        "maxRepairAttempts": 2,
    })


def _korean_generation_request() -> WorldConfigGenerationRequest:
    return _generation_request().model_copy(
        update={"prompt": "좁은 보도에서 공유 킥보드가 로봇 경로를 막고, 오른쪽에서 보행자가 횡단하는 상황을 만들어줘."}
    )


def test_world_config_generation_service_disabled_provider_returns_failed_result() -> None:
    result = generate_world_config(_generation_request(), provider=LlmProvider.disabled)
    payload = result.model_dump(mode="json")

    assert payload["success"] is False
    assert payload["error"]["code"] == "provider_disabled"
    assert payload["generatedPayload"] is None
    assert payload["retrievedContexts"] == []
    assert "No related policy RAG chunks were retrieved." in payload["warnings"]
    assert payload["attempts"]


def test_world_config_generation_result_serializes_scenario_post_processing() -> None:
    result = WorldConfigGenerationResult(
        requestId="REQ-GENERATE-ENDPOINT-001",
        generationType="world_config",
        targetContractType="world_config",
        success=True,
        generatedPayload={"schemaVersion": "1.0"},
        validation=WorldConfigValidationSummary(status="passed"),
        attempts=[],
        retrievedContexts=[],
        scenarioPostProcessing=ScenarioPostProcessResult(
            applied=True,
            patches=[
                ScenarioPostProcessPatch(
                    patchId="PATCH-001",
                    patchType="add_kickboard_obstacle",
                    targetPath="obstacles[]",
                    beforeValue=None,
                    afterValue={"type": "Kickboard"},
                    reason="test",
                )
            ],
            patchedPayload={"schemaVersion": "1.0"},
        ),
        assumptions=[],
        warnings=[],
        error=None,
    )

    payload = result.model_dump(mode="json")
    assert payload["scenarioPostProcessing"]["applied"] is True
    assert payload["scenarioPostProcessing"]["patches"][0]["patchType"] == "add_kickboard_obstacle"


def test_prompt_package_builder_still_returns_context() -> None:
    package = build_world_config_prompt_package(_generation_request())
    assert package.retrievedContexts == []
    assert "No related policy RAG chunks were retrieved." in package.warnings


def test_prompt_package_builder_returns_context_and_scenario_intent_for_korean_prompt() -> None:
    payload = build_world_config_prompt_package(_korean_generation_request()).model_dump(mode="json")
    assert payload["retrievedContexts"] == []
    assert "No related policy RAG chunks were retrieved." in payload["warnings"]
    assert payload["scenarioIntent"]["pathBlockingHints"] is True
    assert payload["scenarioRequirements"]


def test_world_config_generation_service_does_not_create_forbidden_artifacts() -> None:
    generate_world_config(_generation_request(), provider=LlmProvider.disabled)

    assert not (ROOT / "samples").exists()
    assert not (ROOT / "fixtures").exists()
    assert not (ROOT / "data" / "rag" / "vector_db").exists()
    assert not (ROOT / "data" / "rag" / "embeddings").exists()
