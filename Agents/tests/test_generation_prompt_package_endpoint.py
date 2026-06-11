from __future__ import annotations

from pathlib import Path

from app.models.generation import WorldConfigGenerationRequest
from app.services.world_config_prompt_builder import build_world_config_prompt_package


ROOT = Path(__file__).resolve().parents[1]


def _generation_request() -> WorldConfigGenerationRequest:
    return WorldConfigGenerationRequest.model_validate({
        "schemaVersion": "1.0",
        "requestId": "REQ-API-PROMPT-001",
        "generationType": "world_config",
        "prompt": "좁은 보도에서 킥보드가 경로를 막고 보행자가 횡단",
        "targetContractType": "world_config",
        "policyId": "POLICY-MVP",
        "constraints": {
            "unitSystem": "cm_kmh_sec_degree",
            "allowedMapTypes": ["sidewalk"],
            "allowedObjectTypes": ["Pedestrian", "Obstacle", "Kickboard"],
            "fixedPolicyId": "POLICY-MVP",
            "defaultSeed": 11,
            "requireValidation": True,
        },
        "maxRepairAttempts": 2,
    })


def test_prompt_package_builder_returns_prompt_package_without_llm_output() -> None:
    package = build_world_config_prompt_package(_generation_request())
    payload = package.model_dump(mode="json")

    assert payload["requestId"] == "REQ-API-PROMPT-001"
    assert payload["systemPrompt"]
    assert payload["userPrompt"]
    assert payload["retrievedContexts"]
    assert payload["schemaSummary"]
    assert payload["validationPolicy"]
    assert "generatedPayload" not in payload


def test_prompt_package_builder_does_not_create_forbidden_artifacts() -> None:
    build_world_config_prompt_package(_generation_request())

    assert not (ROOT / "samples").exists()
    assert not (ROOT / "fixtures").exists()
    assert not (ROOT / "data" / "rag" / "vector_db").exists()
    assert not (ROOT / "data" / "rag" / "embeddings").exists()
