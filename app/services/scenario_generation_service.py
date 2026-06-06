from __future__ import annotations

from dataclasses import dataclass
import hashlib

from app.core.settings import Settings
from app.models.generation import WorldConfigGenerationRequest
from app.models.llm import LlmProvider
from app.models.run_queue import EpisodeRunQueue
from app.models.scenario_generation import ScenarioGenerateRequest
from app.services.run_queue_export_service import RunQueueExportResult, export_run_queue_package
from app.services.setup_pair_queue_generator import SetupPairQueueResult
from app.services.setup_pair_queue_generator import generate_setup_pair_queue
from app.services.world_config_generation_orchestrator import generate_world_config
from app.services.world_config_scenario_intent_extractor import extract_scenario_intent


@dataclass(frozen=True)
class ScenarioGenerationArtifacts:
    queue: SetupPairQueueResult
    export: RunQueueExportResult


def _prompt_seed(prompt: str) -> int:
    digest = hashlib.sha256(prompt.strip().encode("utf-8")).hexdigest()
    return 1000 + int(digest[:8], 16) % 900000


def _scenario_type_from_prompt(prompt: str) -> str:
    intent = extract_scenario_intent(prompt)
    has_kickboard = "Kickboard" in intent.obstacleHints
    has_narrow_sidewalk = "narrow_sidewalk" in intent.mapHints
    has_crossing = "pedestrian_crossing" in intent.crossingHints
    if has_kickboard and (has_narrow_sidewalk or has_crossing):
        return "narrow_sidewalk_kickboard_crossing"
    if intent.pathBlockingHints or "Obstacle" in intent.obstacleHints:
        return "obstacle_ahead"
    if has_crossing:
        return "pedestrian_crossing"
    if intent.terrainHints:
        return "terrain_risk"
    return "generic_sidewalk"


def _fixed_parameters_from_prompt(prompt: str) -> dict[str, int | float]:
    intent = extract_scenario_intent(prompt)
    fixed: dict[str, int | float] = {}
    if intent.sidewalkWidthCm is not None:
        fixed["sidewalkWidthCm"] = intent.sidewalkWidthCm
    if intent.obstacleBlockingRatio is not None:
        fixed["obstacleBlockingRatio"] = intent.obstacleBlockingRatio
    return fixed


def _generation_request(request: ScenarioGenerateRequest) -> WorldConfigGenerationRequest:
    seed = _prompt_seed(request.prompt)
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
            "defaultSeed": seed,
            "requireValidation": True,
            "environmentSampling": {
                "enabled": True,
                "seed": seed,
                "scenarioType": _scenario_type_from_prompt(request.prompt),
                "fixedParameters": _fixed_parameters_from_prompt(request.prompt),
            },
        },
    )


def generate_scenario_artifacts(request: ScenarioGenerateRequest) -> ScenarioGenerationArtifacts:
    settings = Settings(llmAllowOpenaiFallback=False, llmProviderChain=["openai"], llmMaxTotalAttempts=1)
    generation = generate_world_config(
        _generation_request(request),
        provider=LlmProvider.openai,
        settings=settings,
        allow_fallback=False,
    )
    if generation.generatedPayload is None:
        raise RuntimeError(generation.error.message if generation.error else "WorldConfig generation failed.")
    generated_payload = dict(generation.generatedPayload)
    if generation.environmentSampling is not None:
        generated_payload["environmentSampling"] = generation.environmentSampling
    queue = generate_setup_pair_queue(
        generated_payload,
        episode_count=request.episode_count,
        request_id="SCENARIO-GENERATE-001",
    )
    export = export_run_queue_package(queue)
    return ScenarioGenerationArtifacts(queue=queue, export=export)


def generate_scenario_run_queue(request: ScenarioGenerateRequest) -> EpisodeRunQueue:
    return generate_scenario_artifacts(request).queue.run_queue
