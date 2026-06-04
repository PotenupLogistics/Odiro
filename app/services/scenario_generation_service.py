from __future__ import annotations

from app.core.settings import Settings
from app.models.generation import WorldConfigGenerationRequest
from app.models.llm import LlmProvider
from app.models.run_queue import EpisodeRunQueue
from app.models.scenario_generation import ScenarioGenerateRequest
from app.services.run_queue_export_service import export_run_queue_package
from app.services.setup_pair_queue_generator import generate_setup_pair_queue
from app.services.world_config_generation_orchestrator import generate_world_config


def _generation_request(request: ScenarioGenerateRequest) -> WorldConfigGenerationRequest:
    return WorldConfigGenerationRequest(
        schemaVersion="1.0.0",
        requestId="GEN-SCENARIO-GENERATE-001",
        generationType="world_config",
        targetContractType="world_config",
        prompt=request.prompt,
        policyId="policy_v1_basic_safety",
        maxRepairAttempts=0,
        constraints={
            "unitSystem": "cm_kmh_sec_degree",
            "allowedMapTypes": ["Sidewalk", "Crosswalk"],
            "allowedObjectTypes": ["Pedestrian", "Kickboard", "Obstacle"],
            "fixedPolicyId": "policy_v1_basic_safety",
            "defaultSeed": 1001,
            "requireValidation": True,
            "environmentSampling": {
                "enabled": True,
                "seed": 1001,
                "scenarioType": "obstacle_ahead",
                "fixedParameters": {},
            },
        },
    )


def generate_scenario_run_queue(request: ScenarioGenerateRequest) -> EpisodeRunQueue:
    settings = Settings(llmAllowOpenaiFallback=False, llmProviderChain=["openai"], llmMaxTotalAttempts=1)
    generation = generate_world_config(
        _generation_request(request),
        provider=LlmProvider.openai,
        settings=settings,
        allow_fallback=False,
    )
    if generation.generatedPayload is None:
        raise RuntimeError(generation.error.message if generation.error else "WorldConfig generation failed.")
    queue = generate_setup_pair_queue(generation.generatedPayload, request_id="SCENARIO-GENERATE-001")
    export_run_queue_package(queue)
    return queue.run_queue
