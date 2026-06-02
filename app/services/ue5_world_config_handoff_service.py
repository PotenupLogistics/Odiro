from __future__ import annotations

from datetime import UTC, datetime
from typing import Callable
from uuid import uuid4

from app.core.contract_types import ContractType
from app.core.settings import Settings
from app.models.generation import WorldConfigGenerationError, WorldConfigGenerationRequest, WorldConfigGenerationResult
from app.models.handoff import (
    UE5HandoffMetadata,
    UE5HandoffValidationSummary,
    UE5HandoffWarning,
    UE5WorldConfigHandoffRequest,
    UE5WorldConfigHandoffResponse,
)
from app.models.llm import LlmProvider
from app.services.json_contract_validator import validate_payload
from app.services.episode_spec_validator import validate_episode_spec
from app.services.episode_spec_scenario_reflection import validate_episode_spec_scenario_reflection
from app.services.generation_trace_builder import build_generation_trace
from app.services.generation_trace_builder import infer_failure_stage
from app.services.world_config_to_episode_spec_adapter import (
    convert_world_config_to_episode_spec_with_warnings,
)
from app.services.world_config_generation_orchestrator import generate_world_config


Generator = Callable[[WorldConfigGenerationRequest, LlmProvider], WorldConfigGenerationResult]


def _model_name(provider: LlmProvider) -> str | None:
    if provider == LlmProvider.openai:
        return Settings().openaiModel
    if provider == LlmProvider.ollama:
        return Settings().ollamaModel
    if provider == LlmProvider.disabled:
        return "disabled"
    return None


def _metadata(
    request: UE5WorldConfigHandoffRequest,
    provider: LlmProvider,
) -> UE5HandoffMetadata:
    return UE5HandoffMetadata(
        generatedAt=datetime.now(UTC).isoformat(),
        provider=provider.value,
        model=_model_name(provider),
        policyId=request.generationRequest.policyId,
    )


def _diagnostics(
    result: WorldConfigGenerationResult,
    generation_trace: dict | None = None,
    generation_trace_error: str | None = None,
    failure_stage: str | None = None,
    error_summary: str | None = None,
) -> dict:
    diagnostics = {
        "generationRequestId": result.requestId,
        "attempts": [attempt.model_dump(mode="json") for attempt in result.attempts],
        "retrievedContextCount": len(result.retrievedContexts),
        "warnings": result.warnings,
        "validation": result.validation.model_dump(mode="json"),
        "fallbackTrace": [trace.model_dump(mode="json") for trace in result.fallbackTrace],
        "environmentSampling": result.environmentSampling,
    }
    if generation_trace is not None:
        diagnostics["generationTrace"] = generation_trace
    if generation_trace_error:
        diagnostics["generationTraceError"] = generation_trace_error
    if failure_stage:
        diagnostics["failureStage"] = failure_stage
    if error_summary:
        diagnostics["errorSummary"] = error_summary
    return diagnostics


def _warnings(values: list[str]) -> list[UE5HandoffWarning]:
    return [
        UE5HandoffWarning(code=f"generation_warning_{index + 1}", message=value)
        for index, value in enumerate(values)
    ]


def _requires_static_obstacle(request: UE5WorldConfigHandoffRequest, result: WorldConfigGenerationResult) -> bool:
    prompt = request.generationRequest.prompt.lower()
    if any(value in prompt for value in ["장애물", "정적 장애물", "obstacle", "경로를 막", "차단", "blocking"]):
        return True
    sampling = result.environmentSampling or {}
    parameters = sampling.get("parameters") if isinstance(sampling, dict) else None
    return bool(
        isinstance(parameters, dict)
        and isinstance(parameters.get("obstacleBlockingRatio"), (int, float))
        and float(parameters["obstacleBlockingRatio"]) > 0
    )


def create_ue5_world_config_handoff(
    request: UE5WorldConfigHandoffRequest,
    provider: LlmProvider,
    generator: Generator | None = None,
) -> UE5WorldConfigHandoffResponse:
    run_generation = generator or (
        lambda generation_request, generation_provider: generate_world_config(
            generation_request,
            provider=generation_provider,
        )
    )
    generation_result = run_generation(request.generationRequest, provider)
    metadata = _metadata(request, provider)
    handoff_id = f"UE5-HANDOFF-{uuid4().hex[:12]}"

    contract_validation = None
    episode_spec = None
    episode_validation = None
    episode_scenario_reflection = None
    conversion_warnings = []
    failure_stage = None
    error_summary = None
    handoff_error = generation_result.error
    if generation_result.generatedPayload is not None:
        contract_validation = validate_payload(
            ContractType.world_config,
            generation_result.generatedPayload,
        )
        if contract_validation.valid and request.responseFormat in {"episode_spec", "both"}:
            try:
                episode_spec_model, conversion_warnings = convert_world_config_to_episode_spec_with_warnings(
                    generation_result.generatedPayload
                )
            except Exception as exc:
                failure_stage = "episode_spec_adapter"
                error_summary = str(exc)
                handoff_error = WorldConfigGenerationError(
                    code="episode_spec_adapter_error",
                    message=error_summary,
                )
            else:
                try:
                    episode_validation = validate_episode_spec(episode_spec_model)
                except Exception as exc:
                    failure_stage = "episode_validation"
                    error_summary = str(exc)
                    handoff_error = WorldConfigGenerationError(
                        code="episode_validation_error",
                        message=error_summary,
                    )
                else:
                    if episode_validation.valid:
                        episode_spec = episode_spec_model.model_dump(mode="json", by_alias=True, exclude_none=True)
                    else:
                        failure_stage = "episode_validation"
                        error_summary = "; ".join(episode_validation.errors[:3]) if episode_validation.errors else "EpisodeSpec validation failed."
                    if episode_spec is not None:
                        try:
                            episode_scenario_reflection = validate_episode_spec_scenario_reflection(
                                request.generationRequest.prompt,
                                episode_spec,
                                environment_sampling=generation_result.environmentSampling,
                            )
                        except Exception as exc:
                            failure_stage = "episode_scenario_reflection"
                            error_summary = str(exc)
                            handoff_error = WorldConfigGenerationError(
                                code="episode_scenario_reflection_error",
                                message=error_summary,
                            )
                        else:
                            if (
                                episode_scenario_reflection is not None
                                and (
                                    not episode_scenario_reflection.passed
                                    or not episode_scenario_reflection.ueCompilerReadiness
                                )
                            ):
                                failure_stage = "episode_scenario_reflection"
                                error_summary = (
                                    "; ".join(issue.message for issue in episode_scenario_reflection.issues[:3])
                                    if episode_scenario_reflection.issues
                                    else "EpisodeSpec scenario reflection failed."
                                )
    elif not generation_result.success:
        failure_stage, error_summary = infer_failure_stage(generation_result)

    schema_passed = generation_result.validation.status == "passed"
    scenario_passed = bool(
        generation_result.scenarioReflection and generation_result.scenarioReflection.passed
    )
    contract_passed = bool(contract_validation and contract_validation.valid)
    episode_passed = request.responseFormat == "world_config" or bool(
        episode_validation
        and episode_validation.valid
        and episode_spec
        and episode_scenario_reflection
        and episode_scenario_reflection.passed
        and episode_scenario_reflection.ueCompilerReadiness
    )
    if (
        request.responseFormat in {"episode_spec", "both"}
        and _requires_static_obstacle(request, generation_result)
        and episode_scenario_reflection is not None
        and episode_scenario_reflection.staticObstacleCount == 0
    ):
        episode_passed = False
        failure_stage = failure_stage or "episode_scenario_reflection"
        error_summary = error_summary or "Static obstacle requirement was not reflected in EpisodeSpec."
    success = bool(generation_result.success and generation_result.generatedPayload and contract_passed and episode_passed)
    if not success and failure_stage is None:
        if not generation_result.success:
            failure_stage, error_summary = infer_failure_stage(generation_result)
        elif not contract_passed:
            failure_stage = "world_config_validation"
            error_summary = (
                "; ".join(contract_validation.errors[:3])
                if contract_validation and contract_validation.errors
                else "WorldConfig contract validation failed."
            )
        elif request.responseFormat in {"episode_spec", "both"} and episode_spec is None:
            failure_stage = "episode_spec_adapter"
            error_summary = "EpisodeSpec was not produced."
        else:
            failure_stage = "unknown"
            error_summary = "Handoff failed before a precise failure stage could be determined."

    validation = UE5HandoffValidationSummary(
        schemaValidationPassed=schema_passed,
        scenarioReflectionPassed=scenario_passed,
        contractValidationPassed=contract_passed,
    )

    warnings = _warnings(generation_result.warnings)
    if contract_validation is not None:
        warnings.extend(_warnings(contract_validation.warnings))

    world_config_payload = generation_result.generatedPayload if success and request.responseFormat in {"world_config", "both"} else None
    generation_trace = None
    generation_trace_error = None
    if request.includeDiagnostics:
        try:
            generation_trace = build_generation_trace(
                request.generationRequest,
                generation_result,
                episode_spec=episode_spec,
                episode_validation=episode_validation,
                episode_scenario_reflection=episode_scenario_reflection,
                failure_stage=None if success else failure_stage,
                error_summary=None if success else error_summary,
            ).model_dump(mode="json")
        except Exception as exc:
            generation_trace_error = str(exc)

    return UE5WorldConfigHandoffResponse(
        schemaVersion=request.schemaVersion,
        requestId=request.requestId,
        handoffId=handoff_id,
        handoffTarget=request.handoffTarget,
        success=success,
        worldConfig=world_config_payload,
        episodeSpec=episode_spec if success and request.responseFormat in {"episode_spec", "both"} else None,
        episodeValidation=episode_validation,
        episodeScenarioReflection=episode_scenario_reflection,
        conversionWarnings=conversion_warnings,
        metadata=metadata,
        validation=validation,
        scenarioReflection=generation_result.scenarioReflection,
        postProcessing=generation_result.scenarioPostProcessing,
        diagnostics={
            **_diagnostics(
                generation_result,
                generation_trace=generation_trace,
                generation_trace_error=generation_trace_error,
                failure_stage=None if success else failure_stage,
                error_summary=None if success else error_summary,
            ),
            "effectiveResponseFormat": request.responseFormat,
        } if request.includeDiagnostics else None,
        warnings=warnings,
        error=None if success else handoff_error,
    )
