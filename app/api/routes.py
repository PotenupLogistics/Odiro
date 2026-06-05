from __future__ import annotations

from typing import Any

from fastapi import APIRouter
from fastapi.responses import JSONResponse

from app.core.contract_types import ContractType
from app.models.generation import (
    WorldConfigGenerationError,
    WorldConfigGenerationRequest,
    WorldConfigGenerationResult,
    WorldConfigPromptPackage,
    WorldConfigValidationSummary,
)
from app.models.llm import LlmProvider
from app.models.run_queue import EpisodeRunQueue
from app.models.scenario_generation import ScenarioGenerateRequest
from app.services.json_contract_validator import ValidationResult, validate_payload
from app.services.scenario_generation_service import generate_scenario_run_queue
from app.services.world_config_generation_orchestrator import generate_world_config
from app.services.world_config_prompt_builder import build_world_config_prompt_package


router = APIRouter()


@router.get("/health")
def health() -> dict[str, str]:
    return {"status": "ok", "service": "proto-ai", "version": "0.1.0"}


@router.post(
    "/api/v1/generation/world-config/prompt-package",
    response_model=WorldConfigPromptPackage,
)
def build_world_config_prompt_package_endpoint(
    request: WorldConfigGenerationRequest,
) -> WorldConfigPromptPackage:
    return build_world_config_prompt_package(request)


@router.post(
    "/api/v1/generation/world-config",
    response_model=WorldConfigGenerationResult,
    responses={501: {"model": WorldConfigGenerationResult}},
)
def generate_world_config_endpoint(
    request: WorldConfigGenerationRequest,
    provider: LlmProvider = LlmProvider.disabled,
) -> WorldConfigGenerationResult | JSONResponse:
    if provider not in {LlmProvider.disabled, LlmProvider.openai, LlmProvider.ollama}:
        result = WorldConfigGenerationResult(
            requestId=request.requestId,
            generationType="world_config",
            targetContractType="world_config",
            success=False,
            generatedPayload=None,
            validation=WorldConfigValidationSummary(status="skipped"),
            attempts=[],
            retrievedContexts=build_world_config_prompt_package(request).retrievedContexts,
            assumptions=[],
            warnings=[],
            error=WorldConfigGenerationError(
                code="provider_not_implemented",
                message=f"LLM provider '{provider.value}' is not implemented in this stage.",
            ),
        )
        return JSONResponse(status_code=501, content=result.model_dump(mode="json"))
    return generate_world_config(request, provider=provider)


@router.post(
    "/api/v1/contracts/validate/{contract_type}",
    response_model=ValidationResult,
)
def validate_contract_endpoint(
    contract_type: ContractType,
    payload: dict[str, Any],
) -> ValidationResult:
    return validate_payload(contract_type, payload)


@router.post(
    "/api/v1/scenarios/generate",
    response_model=EpisodeRunQueue,
)
def scenario_generate_endpoint(
    request: ScenarioGenerateRequest,
) -> EpisodeRunQueue:
    return generate_scenario_run_queue(request)
