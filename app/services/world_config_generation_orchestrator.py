from __future__ import annotations

from typing import Any

from app.core.settings import Settings
from app.core.contract_types import ContractType
from app.models.generation import (
    WorldConfigFallbackTrace,
    WorldConfigGenerationAttempt,
    WorldConfigGenerationError,
    WorldConfigGenerationRequest,
    WorldConfigGenerationResult,
    WorldConfigValidationSummary,
)
from app.models.llm import LlmGenerationRequest, LlmGenerationResponse, LlmProvider
from app.services.json_contract_validator import validate_payload
from app.services.json_output_extractor import JsonExtractionError, extract_json_object
from app.services.llm_client import BaseLlmClient
from app.services.llm_client_factory import create_llm_client
from app.services.llm_provider_policy import select_next_provider
from app.services.world_config_prompt_builder import (
    build_world_config_prompt_package,
    build_world_config_repair_prompt_package,
)
from app.services.world_config_output_contract_builder import build_world_config_output_contract
from app.services.world_config_scenario_post_processor import apply_scenario_intent_to_world_config
from app.services.world_config_scenario_repair_prompt_builder import build_scenario_repair_prompt
from app.services.world_config_scenario_reflection import validate_scenario_reflection


def _llm_request(
    request: WorldConfigGenerationRequest,
    provider: LlmProvider,
    system_prompt: str,
    user_prompt: str,
    timeout_sec: int | None = None,
) -> LlmGenerationRequest:
    return LlmGenerationRequest(
        provider=provider,
        model=f"{provider.value}-world-config",
        systemPrompt=system_prompt,
        userPrompt=user_prompt,
        temperature=0.0,
        maxTokens=4096,
        responseFormat="json_object",
        requestId=request.requestId,
        timeoutSec=timeout_sec,
    )


def _preview_text(text: str | None, limit: int = 1000) -> str | None:
    if text is None:
        return None
    return text[:limit]


def _json_preview(payload: dict[str, Any] | None) -> dict[str, Any] | None:
    if payload is None:
        return None
    preview: dict[str, Any] = {}
    for key, value in payload.items():
        if isinstance(value, (str, int, float, bool)) or value is None:
            preview[key] = value
        elif isinstance(value, list):
            preview[key] = f"list[{len(value)}]"
        elif isinstance(value, dict):
            preview[key] = f"object[{len(value)}]"
        else:
            preview[key] = type(value).__name__
    return preview


def _attempt(
    attempt_number: int,
    prompt_type: str,
    llm_success: bool,
    raw_content: str | None,
    extracted_json: dict[str, Any] | None,
    validation_passed: bool,
    validation_errors: list[str] | None = None,
    validation_error_summary: dict[str, Any] | None = None,
    repair_prompt: str | None = None,
    scenario_reflection_passed: bool | None = None,
    scenario_reflection_issues: list[dict[str, Any]] | None = None,
    scenario_repair_prompt: str | None = None,
    provider_error_code: str | None = None,
) -> WorldConfigGenerationAttempt:
    return WorldConfigGenerationAttempt(
        attemptNumber=attempt_number,
        promptType=prompt_type,  # type: ignore[arg-type]
        llmSuccess=llm_success,
        rawContent=raw_content,
        rawContentPreview=_preview_text(raw_content),
        rawContentLength=len(raw_content or ""),
        extractedJson=extracted_json,
        extractedJsonPreview=_json_preview(extracted_json),
        extractedJsonKeys=sorted(extracted_json.keys()) if extracted_json else [],
        jsonExtractionSuccess=extracted_json is not None,
        validationPassed=validation_passed,
        validationErrors=validation_errors or [],
        validationErrorSummary=validation_error_summary or {},
        repairPromptPreview=_preview_text(repair_prompt),
        scenarioReflectionPassed=scenario_reflection_passed,
        scenarioReflectionIssues=scenario_reflection_issues or [],
        scenarioRepairPromptPreview=_preview_text(scenario_repair_prompt),
        providerErrorCode=provider_error_code,
    )


def _failed_disabled_result(
    request: WorldConfigGenerationRequest,
    llm_response: LlmGenerationResponse,
    retrieved_contexts: list,
    warnings: list[str],
) -> WorldConfigGenerationResult:
    message = (
        llm_response.error.message
        if llm_response.error
        else "LLM provider is disabled. Configure a real provider in a later step."
    )
    return WorldConfigGenerationResult(
        requestId=request.requestId,
        generationType="world_config",
        targetContractType="world_config",
        success=False,
        generatedPayload=None,
        validation=WorldConfigValidationSummary(status="skipped"),
        attempts=[
            WorldConfigGenerationAttempt(
                **_attempt(
                    attempt_number=1,
                    prompt_type="initial",
                    llm_success=False,
                    raw_content=llm_response.rawContent,
                    extracted_json=None,
                    validation_passed=False,
                    provider_error_code=llm_response.error.code if llm_response.error else None,
                ).model_dump()
            )
        ],
        retrievedContexts=retrieved_contexts,
        assumptions=[],
        warnings=warnings + llm_response.warnings,
        error=WorldConfigGenerationError(code="provider_disabled", message=message),
    )


def _with_fallback_trace(
    result: WorldConfigGenerationResult,
    trace: WorldConfigFallbackTrace,
) -> WorldConfigGenerationResult:
    result.fallbackTrace.insert(0, trace)
    return result


def _fallback_or_result(
    result: WorldConfigGenerationResult,
    request: WorldConfigGenerationRequest,
    provider: LlmProvider,
    error_code: str,
    settings: Settings,
    timeout_sec: int | None,
    context_top_k: int,
    compact_prompt: bool,
    fallback_client_overrides: dict[LlmProvider, BaseLlmClient] | None,
    allow_fallback: bool,
) -> WorldConfigGenerationResult:
    if not allow_fallback:
        return result
    next_provider = select_next_provider(provider, settings, error_code)
    if next_provider is None:
        return result
    fallback_result = generate_world_config(
        request,
        provider=next_provider,
        client_override=(fallback_client_overrides or {}).get(next_provider),
        timeout_sec=timeout_sec,
        context_top_k=context_top_k,
        compact_prompt=compact_prompt,
        settings=settings,
        fallback_client_overrides=fallback_client_overrides,
        allow_fallback=False,
    )
    trace = WorldConfigFallbackTrace(
        fromProvider=provider.value,
        toProvider=next_provider.value,
        reason=f"{error_code} triggered provider fallback.",
        errorCode=error_code,
        success=fallback_result.success,
    )
    if fallback_result.success:
        return _with_fallback_trace(fallback_result, trace)
    messages = []
    if result.error:
        messages.append(f"{provider.value}: {result.error.code} - {result.error.message}")
    if fallback_result.error:
        messages.append(
            f"{next_provider.value}: {fallback_result.error.code} - {fallback_result.error.message}"
        )
    fallback_result.error = WorldConfigGenerationError(
        code="provider_chain_failed",
        message="Both provider attempts failed. " + " | ".join(messages),
    )
    return _with_fallback_trace(fallback_result, trace)


def _attempt_from_response(
    attempt_number: int,
    prompt_type: str,
    llm_response: LlmGenerationResponse,
) -> tuple[WorldConfigGenerationAttempt, dict | None]:
    if not llm_response.success or not llm_response.content:
        message = llm_response.error.message if llm_response.error else "LLM generation failed."
        return (
            _attempt(
                attempt_number=attempt_number,
                prompt_type=prompt_type,
                llm_success=llm_response.success,
                raw_content=llm_response.rawContent or llm_response.content,
                extracted_json=None,
                validation_passed=False,
                validation_errors=[message],
                validation_error_summary={
                    "totalErrors": 1,
                    "providerErrors": [llm_response.error.code] if llm_response.error else [],
                },
                provider_error_code=llm_response.error.code if llm_response.error else None,
            ),
            None,
        )

    try:
        extracted = extract_json_object(llm_response.content)
    except JsonExtractionError as exc:
        return (
            _attempt(
                attempt_number=attempt_number,
                prompt_type=prompt_type,
                llm_success=True,
                raw_content=llm_response.rawContent or llm_response.content,
                extracted_json=None,
                validation_passed=False,
                validation_errors=[str(exc)],
                validation_error_summary={
                    "totalErrors": 1,
                    "extractionErrorCode": exc.code,
                    "errorsByType": {exc.code: 1},
                },
            ),
            None,
        )

    validation = validate_payload(ContractType.world_config, extracted)
    return (
        _attempt(
            attempt_number=attempt_number,
            prompt_type=prompt_type,
            llm_success=True,
            raw_content=llm_response.rawContent or llm_response.content,
            extracted_json=extracted,
            validation_passed=validation.valid,
            validation_errors=validation.errors,
            validation_error_summary=validation.errorSummary,
        ),
        extracted,
    )


def _apply_post_processing(
    request: WorldConfigGenerationRequest,
    payload: dict[str, Any],
    attempt: WorldConfigGenerationAttempt,
) -> tuple[dict[str, Any] | None, Any, Any, list[str], dict[str, Any]]:
    post_processing = apply_scenario_intent_to_world_config(request.prompt, payload)
    attempt.scenarioPostProcessingApplied = post_processing.applied
    attempt.scenarioPostProcessingPatches = [
        patch.model_dump(mode="json") for patch in post_processing.patches
    ]

    if not post_processing.applied:
        return None, None, post_processing, [], {}

    post_validation = validate_payload(ContractType.world_config, post_processing.patchedPayload)
    if not post_validation.valid:
        return (
            post_processing.patchedPayload,
            None,
            post_processing,
            post_validation.errors,
            post_validation.errorSummary,
        )

    post_reflection = validate_scenario_reflection(request.prompt, post_processing.patchedPayload)
    attempt.scenarioReflectionPassed = post_reflection.passed
    attempt.scenarioReflectionIssues = [
        issue.model_dump(mode="json") for issue in post_reflection.issues
    ]
    return post_processing.patchedPayload, post_reflection, post_processing, [], {}


def generate_world_config(
    request: WorldConfigGenerationRequest,
    provider: LlmProvider = LlmProvider.disabled,
    client_override: BaseLlmClient | None = None,
    timeout_sec: int | None = None,
    context_top_k: int = 5,
    compact_prompt: bool = False,
    settings: Settings | None = None,
    fallback_client_overrides: dict[LlmProvider, BaseLlmClient] | None = None,
    allow_fallback: bool = True,
) -> WorldConfigGenerationResult:
    settings = settings or Settings()
    prompt_package = build_world_config_prompt_package(
        request,
        context_top_k=context_top_k,
        compact_prompt=compact_prompt,
    )
    client = client_override or create_llm_client(provider, settings=settings)
    warnings = list(prompt_package.warnings)

    initial_response = client.generate(
        _llm_request(
            request,
            provider,
            prompt_package.systemPrompt,
            prompt_package.userPrompt,
            timeout_sec=timeout_sec,
        )
    )
    if provider == LlmProvider.disabled and not initial_response.success:
        return _failed_disabled_result(
            request,
            initial_response,
            prompt_package.retrievedContexts,
            warnings,
        )

    attempts: list[WorldConfigGenerationAttempt] = []
    attempt, extracted = _attempt_from_response(1, "initial", initial_response)
    attempts.append(attempt)
    last_reflection = None
    last_post_processing = None
    next_repair_kind = "schema"
    if attempt.validationPassed and extracted is not None:
        reflection = validate_scenario_reflection(request.prompt, extracted)
        last_reflection = reflection
        attempt.scenarioReflectionPassed = reflection.passed
        attempt.scenarioReflectionIssues = [issue.model_dump(mode="json") for issue in reflection.issues]
        if not reflection.passed and request.constraints.requireValidation:
            (
                post_payload,
                post_reflection,
                post_processing,
                post_errors,
                post_error_summary,
            ) = _apply_post_processing(request, extracted, attempt)
            last_post_processing = post_processing
            warnings.extend(post_processing.warnings)
            if post_payload is not None and post_reflection is not None:
                last_reflection = post_reflection
                if post_reflection.passed:
                    return WorldConfigGenerationResult(
                        requestId=request.requestId,
                        generationType="world_config",
                        targetContractType="world_config",
                        success=True,
                        generatedPayload=post_payload,
                        validation=WorldConfigValidationSummary(status="passed"),
                        attempts=attempts,
                        retrievedContexts=prompt_package.retrievedContexts,
                        scenarioReflection=post_reflection,
                        scenarioPostProcessing=post_processing,
                        assumptions=[],
                        warnings=warnings + initial_response.warnings,
                        error=None,
                    )
                last_errors = [issue.message for issue in post_reflection.issues]
                invalid_payload = post_payload
                next_repair_kind = "scenario"
            elif post_errors:
                last_errors = post_errors
                last_error_summary = post_error_summary
                invalid_payload = post_payload or extracted
                next_repair_kind = "schema"
            else:
                last_errors = [issue.message for issue in reflection.issues]
                invalid_payload = extracted
                next_repair_kind = "scenario"
        else:
            return WorldConfigGenerationResult(
                requestId=request.requestId,
                generationType="world_config",
                targetContractType="world_config",
                success=True,
                generatedPayload=extracted,
                validation=WorldConfigValidationSummary(status="passed"),
                attempts=attempts,
                retrievedContexts=prompt_package.retrievedContexts,
                scenarioReflection=reflection,
                scenarioPostProcessing=last_post_processing,
                assumptions=[],
                warnings=warnings + initial_response.warnings,
                error=None,
            )
    else:
        reflection = None
        last_errors = attempt.validationErrors
        invalid_payload = extracted or attempt.extractedJson or {}

    last_error_summary = attempt.validationErrorSummary
    repair_index = 1
    scenario_extra_used = False
    while repair_index <= request.maxRepairAttempts or (
        next_repair_kind == "scenario"
        and request.maxRepairAttempts >= 1
        and not scenario_extra_used
    ):
        scenario_repair_prompt = None
        if next_repair_kind == "scenario" and last_reflection is not None:
            scenario_extra_used = True
            scenario_repair_prompt = build_scenario_repair_prompt(
                invalid_payload,
                last_reflection,
                build_world_config_output_contract(),
            )
            system_prompt = prompt_package.systemPrompt
            user_prompt = scenario_repair_prompt
            prompt_type = "scenario_repair"
        else:
            repair_package = build_world_config_repair_prompt_package(
                request,
                invalid_payload,
                last_errors,
                last_error_summary,
            )
            system_prompt = repair_package.systemPrompt
            user_prompt = repair_package.repairPrompt
            prompt_type = "schema_repair"
        repair_response = client.generate(
            _llm_request(
                request,
                provider,
                system_prompt,
                user_prompt,
                timeout_sec=timeout_sec,
            )
        )
        repair_attempt, repair_extracted = _attempt_from_response(
            repair_index + 1,
            prompt_type,
            repair_response,
        )
        if scenario_repair_prompt is not None:
            repair_attempt.scenarioRepairPromptPreview = _preview_text(scenario_repair_prompt)
        else:
            repair_attempt.repairPromptPreview = _preview_text(user_prompt)
        attempts.append(repair_attempt)
        if repair_attempt.validationPassed and repair_extracted is not None:
            repair_reflection = validate_scenario_reflection(request.prompt, repair_extracted)
            last_reflection = repair_reflection
            repair_attempt.scenarioReflectionPassed = repair_reflection.passed
            repair_attempt.scenarioReflectionIssues = [
                issue.model_dump(mode="json") for issue in repair_reflection.issues
            ]
            if not repair_reflection.passed and request.constraints.requireValidation:
                (
                    post_payload,
                    post_reflection,
                    post_processing,
                    post_errors,
                    post_error_summary,
                ) = _apply_post_processing(request, repair_extracted, repair_attempt)
                last_post_processing = post_processing
                warnings.extend(post_processing.warnings)
                if post_payload is not None and post_reflection is not None:
                    last_reflection = post_reflection
                    if post_reflection.passed:
                        return WorldConfigGenerationResult(
                            requestId=request.requestId,
                            generationType="world_config",
                            targetContractType="world_config",
                            success=True,
                            generatedPayload=post_payload,
                            validation=WorldConfigValidationSummary(status="passed"),
                            attempts=attempts,
                            retrievedContexts=prompt_package.retrievedContexts,
                            scenarioReflection=post_reflection,
                            scenarioPostProcessing=post_processing,
                            assumptions=[],
                            warnings=warnings + repair_response.warnings,
                            error=None,
                        )
                    last_errors = [issue.message for issue in post_reflection.issues]
                    invalid_payload = post_payload
                    next_repair_kind = "scenario"
                    repair_index += 1
                    continue
                if post_errors:
                    last_errors = post_errors
                    last_error_summary = post_error_summary
                    invalid_payload = post_payload or repair_extracted
                    next_repair_kind = "schema"
                    repair_index += 1
                    continue
                last_errors = [issue.message for issue in repair_reflection.issues]
                invalid_payload = repair_extracted
                next_repair_kind = "scenario"
                repair_index += 1
                continue
            return WorldConfigGenerationResult(
                requestId=request.requestId,
                generationType="world_config",
                targetContractType="world_config",
                success=True,
                generatedPayload=repair_extracted,
                validation=WorldConfigValidationSummary(status="passed"),
                attempts=attempts,
                retrievedContexts=prompt_package.retrievedContexts,
                scenarioReflection=repair_reflection,
                scenarioPostProcessing=last_post_processing,
                assumptions=[],
                warnings=warnings + repair_response.warnings,
                error=None,
            )
        last_errors = repair_attempt.validationErrors
        last_error_summary = repair_attempt.validationErrorSummary
        invalid_payload = repair_extracted or repair_attempt.extractedJson or invalid_payload
        next_repair_kind = "schema"
        repair_index += 1

    if last_reflection is not None and not last_reflection.passed:
        failed_result = WorldConfigGenerationResult(
            requestId=request.requestId,
            generationType="world_config",
            targetContractType="world_config",
            success=False,
            generatedPayload=None,
            validation=WorldConfigValidationSummary(status="passed"),
            attempts=attempts,
            retrievedContexts=prompt_package.retrievedContexts,
            scenarioReflection=last_reflection,
            scenarioPostProcessing=last_post_processing,
            assumptions=[],
            warnings=warnings,
            error=WorldConfigGenerationError(
                code="scenario_reflection_failed",
                message=last_reflection.summary,
            ),
        )
        return _fallback_or_result(
            failed_result,
            request,
            provider,
            "scenario_reflection_failed",
            settings,
            timeout_sec,
            context_top_k,
            compact_prompt,
            fallback_client_overrides,
            allow_fallback,
        )

    provider_error_codes = [attempt.providerErrorCode for attempt in attempts if attempt.providerErrorCode]
    validation_actually_run = any(attempt.jsonExtractionSuccess for attempt in attempts)
    if provider_error_codes and len(provider_error_codes) == len(attempts) and not validation_actually_run:
        provider_error = provider_error_codes[-1]
        final_code = (
            provider_error
            if provider_error in {"ollama_timeout", "ollama_connection_failed"}
            else "provider_runtime_error"
        )
        failed_result = WorldConfigGenerationResult(
            requestId=request.requestId,
            generationType="world_config",
            targetContractType="world_config",
            success=False,
            generatedPayload=None,
            validation=WorldConfigValidationSummary(status="skipped", errors=[]),
            attempts=attempts,
            retrievedContexts=prompt_package.retrievedContexts,
            scenarioPostProcessing=last_post_processing,
            assumptions=[],
            warnings=warnings,
            error=WorldConfigGenerationError(
                code=final_code,
                message="Provider failed before JSON extraction or world_config validation could run.",
            ),
        )
        return _fallback_or_result(
            failed_result,
            request,
            provider,
            provider_error,
            settings,
            timeout_sec,
            context_top_k,
            compact_prompt,
            fallback_client_overrides,
            allow_fallback,
        )

    failed_result = WorldConfigGenerationResult(
        requestId=request.requestId,
        generationType="world_config",
        targetContractType="world_config",
        success=False,
        generatedPayload=None,
        validation=WorldConfigValidationSummary(status="failed", errors=last_errors),
        attempts=attempts,
        retrievedContexts=prompt_package.retrievedContexts,
        scenarioPostProcessing=last_post_processing,
        assumptions=[],
        warnings=warnings,
        error=WorldConfigGenerationError(
            code="validation_failed",
            message="World Config generation did not produce a valid payload.",
        ),
    )
    return _fallback_or_result(
        failed_result,
        request,
        provider,
        "validation_failed_after_repair",
        settings,
        timeout_sec,
        context_top_k,
        compact_prompt,
        fallback_client_overrides,
        allow_fallback,
    )
