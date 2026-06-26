from __future__ import annotations

import argparse
import json
import sys
from datetime import UTC, datetime
from pathlib import Path
from typing import Any
from uuid import uuid4


ROOT = Path(__file__).resolve().parents[1]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from app.core.settings import Settings  # noqa: E402
from app.models.generation import (  # noqa: E402
    WorldConfigGenerationConstraints,
    WorldConfigGenerationRequest,
    WorldConfigGenerationResult,
    WorldConfigPromptPackage,
)
from app.models.llm import LlmProvider  # noqa: E402
from app.models.llm import LlmGenerationRequest  # noqa: E402
from app.services.llm_ollama_client import OllamaLlmClient  # noqa: E402
from app.services.world_config_generation_orchestrator import generate_world_config  # noqa: E402
from app.services.world_config_prompt_builder import build_world_config_prompt_package  # noqa: E402
from app.utils.report_serialization import write_json_report  # noqa: E402
from app.services.world_config_scenario_intent_extractor import (  # noqa: E402
    build_scenario_requirements,
    extract_scenario_intent,
)


def _parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description=(
            "Manual Ollama live smoke runner for World Config generation. "
            "Use --dry-run to build the prompt package without calling Ollama."
        )
    )
    parser.add_argument("--prompt", required=True, help="Natural-language World Config scenario prompt.")
    parser.add_argument("--model", help="Ollama model override. Defaults to OLLAMA_MODEL/settings.")
    parser.add_argument("--base-url", help="Ollama base URL override. Defaults to OLLAMA_BASE_URL/settings.")
    parser.add_argument(
        "--max-repair-attempts",
        type=int,
        default=None,
        help="Maximum validation repair attempts for the orchestrator.",
    )
    parser.add_argument("--report", help="Optional path for a JSON smoke report. No report is created by default.")
    parser.add_argument("--timeout-sec", type=int, default=None, help="Ollama request timeout override in seconds.")
    parser.add_argument("--context-top-k", type=int, default=5, help="Maximum retrieved policy contexts to include.")
    parser.add_argument("--compact-prompt", action="store_true", help="Use compact retrieved context in the prompt.")
    parser.add_argument("--warm-up", action="store_true", help="Run one simple JSON-only warm-up request before live smoke.")
    parser.add_argument("--print-payload", action="store_true", help="Print generated payload when generation succeeds.")
    parser.add_argument(
        "--include-payload",
        action="store_true",
        help="Include full generatedPayload in the optional report.",
    )
    parser.add_argument(
        "--include-raw-attempts",
        action="store_true",
        help="Include full rawContent for each attempt in the optional report.",
    )
    parser.add_argument(
        "--include-extracted-json",
        action="store_true",
        help="Include full extractedJson for each attempt in the optional report.",
    )
    parser.add_argument(
        "--raw-preview-chars",
        type=int,
        default=1000,
        help="Maximum rawContentPreview characters stored in reports.",
    )
    parser.add_argument("--dry-run", action="store_true", help="Build prompt package only; do not call Ollama.")
    return parser


def _settings(args: argparse.Namespace) -> Settings:
    settings = Settings()
    updates: dict[str, Any] = {}
    if args.model:
        updates["ollamaModel"] = args.model
    if args.base_url:
        updates["ollamaBaseUrl"] = args.base_url
    if args.timeout_sec:
        updates["ollamaTimeoutSec"] = args.timeout_sec
    return settings.model_copy(update=updates) if updates else settings


def _request(args: argparse.Namespace, settings: Settings) -> WorldConfigGenerationRequest:
    return WorldConfigGenerationRequest(
        schemaVersion="1.0",
        requestId=f"manual-ollama-smoke-{uuid4().hex[:12]}",
        generationType="world_config",
        prompt=args.prompt,
        targetContractType="world_config",
        policyId="POLICY-MVP",
        constraints=WorldConfigGenerationConstraints(
            unitSystem="cm_kmh_sec_degree",
            allowedMapTypes=["sidewalk", "crosswalk", "mixed_sidewalk"],
            allowedObjectTypes=[
                "Pedestrian",
                "Obstacle",
                "Animal",
                "Bicycle",
                "Kickboard",
                "Vehicle",
                "Unknown",
            ],
            fixedPolicyId="POLICY-MVP",
            defaultSeed=42,
            requireValidation=True,
        ),
        maxRepairAttempts=(
            args.max_repair_attempts
            if args.max_repair_attempts is not None
            else min(settings.llmMaxTotalAttempts, 2)
        ),
    )


def _payload_summary(payload: dict[str, Any] | None) -> dict[str, Any]:
    if not payload:
        return {}
    return {
        "worldId": payload.get("worldId"),
        "scenarioId": payload.get("scenarioId"),
        "mapType": (payload.get("map") or {}).get("type") if isinstance(payload.get("map"), dict) else None,
        "obstacleCount": len(payload.get("obstacles") or []),
        "pedestrianCount": len(payload.get("pedestrians") or []),
        "environmentObjectCount": len(payload.get("environmentObjects") or []),
    }


def _empty_diagnostics() -> dict[str, Any]:
    return {
        "attemptsDetail": [],
        "validationErrorSummary": {},
        "extractionSummary": {
            "attempts": 0,
            "successCount": 0,
            "failureCount": 0,
            "providerErrorCodes": [],
        },
        "promptRepairSummary": {
            "repairAttempts": 0,
            "repairPromptPreviewAvailable": False,
        },
        "providerErrorSummary": {},
        "finalErrorClassification": "dry_run",
        "validationActuallyRun": False,
        "recommendedNextAction": "Dry run only. Run live smoke to diagnose model output.",
    }


def _scenario_requirement_paths(prompt: str) -> list[str]:
    return [
        requirement.expectedPath
        for requirement in build_scenario_requirements(extract_scenario_intent(prompt))
    ]


def _attempt_detail(
    attempt: Any,
    include_raw_attempts: bool,
    include_extracted_json: bool,
    raw_preview_chars: int,
) -> dict[str, Any]:
    detail = {
        "attemptNumber": attempt.attemptNumber,
        "promptType": attempt.promptType,
        "llmSuccess": attempt.llmSuccess,
        "providerErrorCode": attempt.providerErrorCode,
        "rawContentPreview": (attempt.rawContent or "")[:raw_preview_chars] if attempt.rawContent else None,
        "rawContentLength": attempt.rawContentLength,
        "jsonExtractionSuccess": attempt.jsonExtractionSuccess,
        "extractedJsonPreview": attempt.extractedJsonPreview,
        "extractedJsonKeys": attempt.extractedJsonKeys,
        "validationPassed": attempt.validationPassed,
        "validationErrors": attempt.validationErrors,
        "validationErrorSummary": attempt.validationErrorSummary,
        "repairPromptPreview": attempt.repairPromptPreview,
        "scenarioReflectionPassed": attempt.scenarioReflectionPassed,
        "scenarioReflectionIssues": attempt.scenarioReflectionIssues,
        "scenarioRepairPromptPreview": attempt.scenarioRepairPromptPreview,
        "scenarioPostProcessingApplied": attempt.scenarioPostProcessingApplied,
        "scenarioPostProcessingPatches": attempt.scenarioPostProcessingPatches,
    }
    if include_raw_attempts:
        detail["rawContent"] = attempt.rawContent
    if include_extracted_json:
        detail["extractedJson"] = attempt.extractedJson
    return detail


def _summarize_attempts(result: WorldConfigGenerationResult) -> dict[str, Any]:
    attempts = result.attempts
    extraction_failures = [
        attempt.validationErrorSummary.get("extractionErrorCode")
        for attempt in attempts
        if attempt.validationErrorSummary.get("extractionErrorCode")
    ]
    provider_errors = [attempt.providerErrorCode for attempt in attempts if attempt.providerErrorCode]
    aggregate_errors: dict[str, int] = {}
    missing_fields: set[str] = set()
    extra_fields: set[str] = set()
    enum_errors: set[str] = set()
    type_errors: set[str] = set()
    for attempt in attempts:
        summary = attempt.validationErrorSummary or {}
        for key, value in (summary.get("errorsByType") or {}).items():
            aggregate_errors[key] = aggregate_errors.get(key, 0) + int(value)
        missing_fields.update(summary.get("missingRequiredFields") or [])
        extra_fields.update(summary.get("extraFields") or [])
        enum_errors.update(summary.get("enumErrors") or [])
        type_errors.update(summary.get("typeErrors") or [])
    return {
        "validationErrorSummary": {
            "totalErrors": sum(aggregate_errors.values()),
            "errorsByType": aggregate_errors,
            "missingRequiredFields": sorted(missing_fields),
            "extraFields": sorted(extra_fields),
            "enumErrors": sorted(enum_errors),
            "typeErrors": sorted(type_errors),
        },
        "extractionSummary": {
            "attempts": len(attempts),
            "successCount": sum(1 for attempt in attempts if attempt.jsonExtractionSuccess),
            "failureCount": sum(1 for attempt in attempts if not attempt.jsonExtractionSuccess),
            "extractionErrorCodes": sorted({code for code in extraction_failures if code}),
            "providerErrorCodes": sorted({code for code in provider_errors if code}),
        },
        "promptRepairSummary": {
            "repairAttempts": sum(1 for attempt in attempts if attempt.promptType in {"repair", "schema_repair", "scenario_repair"}),
            "schemaRepairAttempts": sum(1 for attempt in attempts if attempt.promptType in {"repair", "schema_repair"}),
            "scenarioRepairAttempts": sum(1 for attempt in attempts if attempt.promptType == "scenario_repair"),
            "repairPromptPreviewAvailable": any(attempt.repairPromptPreview or attempt.scenarioRepairPromptPreview for attempt in attempts),
            "validationPassedAfterRepair": any(
                attempt.promptType in {"repair", "schema_repair", "scenario_repair"} and attempt.validationPassed for attempt in attempts
            ),
            "scenarioReflectionPassedAfterRepair": any(
                attempt.promptType == "scenario_repair" and attempt.scenarioReflectionPassed for attempt in attempts
            ),
            "postProcessingApplied": any(attempt.scenarioPostProcessingApplied for attempt in attempts),
            "postProcessingPatchCount": sum(len(attempt.scenarioPostProcessingPatches) for attempt in attempts),
        },
        "providerErrorSummary": {
            "providerErrorCodes": sorted({code for code in provider_errors if code}),
            "providerErrorCount": len(provider_errors),
        },
    }


def _recommended_next_action(result: WorldConfigGenerationResult, summary: dict[str, Any]) -> str:
    validation_summary = summary["validationErrorSummary"]
    extraction_summary = summary["extractionSummary"]
    repair_summary = summary["promptRepairSummary"]
    if extraction_summary["failureCount"] > extraction_summary["successCount"]:
        if extraction_summary["providerErrorCodes"]:
            return "Provider errors occurred before JSON extraction. Increase Ollama timeout, warm the model, or use a smaller/faster model."
        return "Strengthen JSON-only prompt guidance and inspect raw model output."
    if validation_summary["missingRequiredFields"] and validation_summary.get("extraFields"):
        return "Add stronger output contract instruction and extra key prohibition."
    if validation_summary["missingRequiredFields"]:
        return "Add stronger output contract instruction for missing required fields."
    if validation_summary.get("extraFields"):
        return "Strengthen allowed field limitation and remove-extra-fields repair guidance."
    if validation_summary["enumErrors"] or validation_summary["typeErrors"]:
        return "Provide clearer enum/type constraints in the prompt and repair prompt."
    if result.error and result.error.code == "scenario_reflection_failed":
        return "Schema passed but scenario reflection failed. Check scenario repair prompt and semantic binding rules."
    if result.attempts and not result.success and repair_summary["repairAttempts"] > 0:
        return "Repeated validation failure after repair. Consider prompt repair improvements before a separate manual OpenAI smoke."
    if result.success:
        return "Validated payload generated. Review scenario quality before UE5 handoff."
    return "Inspect attempt details and improve prompt constraints."


def _warm_up(client: OllamaLlmClient, settings: Settings, request_id: str, timeout_sec: int | None) -> dict[str, Any]:
    warm_up_request = LlmGenerationRequest(
        provider=LlmProvider.ollama,
        model=settings.ollamaModel,
        systemPrompt="Return one JSON object only.",
        userPrompt='Return exactly {"ok": true}.',
        temperature=0.0,
        maxTokens=64,
        responseFormat="json_object",
        requestId=f"{request_id}-warm-up",
        timeoutSec=timeout_sec,
    )
    response = client.generate(warm_up_request)
    return {
        "enabled": True,
        "success": response.success,
        "error": response.error.model_dump() if response.error else None,
        "contentPreview": response.content[:200] if response.content else None,
    }


def _prompt_package_report(
    prompt: str,
    settings: Settings,
    package: WorldConfigPromptPackage,
    include_payload: bool,
    args: argparse.Namespace,
) -> dict[str, Any]:
    return {
        "prompt": prompt,
        "provider": "ollama",
        "model": settings.ollamaModel,
        "success": False,
        "error": {
            "code": "dry_run",
            "message": "Dry run only; Ollama was not called.",
        },
        "attemptsCount": 0,
        "validationPassed": False,
        "retrievedContextCount": len(package.retrievedContexts),
        "generatedPayloadSummary": {},
        "timeoutSec": args.timeout_sec or settings.ollamaTimeoutSec,
        "maxRepairAttempts": args.max_repair_attempts,
        "contextTopK": args.context_top_k,
        "compactPrompt": args.compact_prompt,
        "warmUpEnabled": False,
        "warmUpResult": None,
        "outputContractIncluded": "Output Contract" in package.systemPrompt + package.userPrompt,
        "scenarioRequirementCount": len(package.scenarioRequirements),
        "scenarioRequirementPaths": [item.expectedPath for item in package.scenarioRequirements],
        "checkedAt": datetime.now(UTC).isoformat(),
        **_empty_diagnostics(),
        **({"generatedPayload": None} if include_payload else {}),
    }


def _result_report(
    prompt: str,
    settings: Settings,
    result: WorldConfigGenerationResult,
    include_payload: bool,
    include_raw_attempts: bool,
    include_extracted_json: bool,
    raw_preview_chars: int,
    args: argparse.Namespace,
    warm_up_result: dict[str, Any] | None,
) -> dict[str, Any]:
    diagnostics = _summarize_attempts(result)
    validation_actually_run = any(attempt.jsonExtractionSuccess for attempt in result.attempts)
    report = {
        "prompt": prompt,
        "provider": "ollama",
        "model": settings.ollamaModel,
        "success": result.success,
        "error": result.error.model_dump() if result.error else None,
        "attemptsCount": len(result.attempts),
        "validationPassed": result.validation.status == "passed",
        "retrievedContextCount": len(result.retrievedContexts),
        "generatedPayloadSummary": _payload_summary(result.generatedPayload),
        "timeoutSec": args.timeout_sec or settings.ollamaTimeoutSec,
        "maxRepairAttempts": result.attempts[-1].attemptNumber - 1 if result.attempts else 0,
        "contextTopK": args.context_top_k,
        "compactPrompt": args.compact_prompt,
        "warmUpEnabled": args.warm_up,
        "warmUpResult": warm_up_result,
        "outputContractIncluded": True,
        "scenarioRequirementCount": len(_scenario_requirement_paths(prompt)),
        "scenarioRequirementPaths": _scenario_requirement_paths(prompt),
        "scenarioReflection": result.scenarioReflection.model_dump(mode="json") if result.scenarioReflection else None,
        "scenarioPostProcessing": result.scenarioPostProcessing.model_dump(mode="json") if result.scenarioPostProcessing else None,
        "attemptsDetail": [
            _attempt_detail(
                attempt,
                include_raw_attempts=include_raw_attempts,
                include_extracted_json=include_extracted_json,
                raw_preview_chars=raw_preview_chars,
            )
            for attempt in result.attempts
        ],
        "validationErrorSummary": diagnostics["validationErrorSummary"],
        "extractionSummary": diagnostics["extractionSummary"],
        "promptRepairSummary": diagnostics["promptRepairSummary"],
        "providerErrorSummary": diagnostics["providerErrorSummary"],
        "finalErrorClassification": result.error.code if result.error else None,
        "validationActuallyRun": validation_actually_run,
        "recommendedNextAction": _recommended_next_action(result, diagnostics),
        "checkedAt": datetime.now(UTC).isoformat(),
    }
    if include_payload:
        report["generatedPayload"] = result.generatedPayload
    return report


def _write_report(path_text: str | None, report: dict[str, Any]) -> None:
    if not path_text:
        return
    write_json_report(path_text, report)


def _print_dry_run(package: WorldConfigPromptPackage, settings: Settings) -> None:
    print("DRY RUN: no Ollama call was made.")
    print("Provider: ollama")
    print(f"Model: {settings.ollamaModel}")
    print("Prompt package:")
    print(
        json.dumps(
            {
                "requestId": package.requestId,
                "retrievedContexts": [context.model_dump() for context in package.retrievedContexts],
                "schemaSummary": package.schemaSummary,
                "validationPolicy": package.validationPolicy,
                "warnings": package.warnings,
            },
            ensure_ascii=False,
            indent=2,
        )
    )


def _print_result(result: WorldConfigGenerationResult, settings: Settings, print_payload: bool) -> None:
    print("Ollama World Config smoke result:")
    print("- provider: ollama")
    print(f"- model: {settings.ollamaModel}")
    print(f"- success: {result.success}")
    print(f"- validation: {result.validation.status}")
    print(f"- attempts: {len(result.attempts)}")
    print(f"- retrievedContexts: {len(result.retrievedContexts)}")
    if result.error:
        print(f"- error.code: {result.error.code}")
        print(f"- error.message: {result.error.message}")
    if result.generatedPayload:
        print(f"- generatedPayloadSummary: {json.dumps(_payload_summary(result.generatedPayload), ensure_ascii=False)}")
    if print_payload and result.generatedPayload:
        print("generatedPayload:")
        print(json.dumps(result.generatedPayload, ensure_ascii=False, indent=2))


def main(argv: list[str] | None = None) -> int:
    args = _parser().parse_args(argv)
    settings = _settings(args)
    request = _request(args, settings)

    if args.dry_run:
        package = build_world_config_prompt_package(
            request,
            context_top_k=args.context_top_k,
            compact_prompt=args.compact_prompt,
        )
        _print_dry_run(package, settings)
        _write_report(args.report, _prompt_package_report(args.prompt, settings, package, args.include_payload, args))
        return 0

    client = OllamaLlmClient(settings=settings)
    warm_up_result = _warm_up(client, settings, request.requestId, args.timeout_sec) if args.warm_up else None
    result = generate_world_config(
        request,
        provider=LlmProvider.ollama,
        client_override=client,
        timeout_sec=args.timeout_sec,
        context_top_k=args.context_top_k,
        compact_prompt=args.compact_prompt,
    )
    _print_result(result, settings, args.print_payload)
    _write_report(
        args.report,
        _result_report(
            args.prompt,
            settings,
            result,
            args.include_payload,
            args.include_raw_attempts,
            args.include_extracted_json,
            args.raw_preview_chars,
            args,
            warm_up_result,
        ),
    )
    return 0 if result.success else 1


if __name__ == "__main__":
    raise SystemExit(main())
