from __future__ import annotations

import json
from pathlib import Path
from typing import Any

from app.models.generation import (
    WorldConfigGenerationRequest,
    WorldConfigPromptPackage,
    WorldConfigRepairPromptPackage,
)
from app.services.natural_language_normalizer import normalize_prompt
from app.services.world_config_rag_context_builder import build_policy_context_for_world_config
from app.services.world_config_output_contract_builder import build_world_config_output_contract
from app.services.environment_generation_constraints_builder import (
    apply_environment_parameters_to_scenario_requirements,
    build_environment_constraints_prompt_section,
    build_environment_sampling_context,
    environment_sampling_summary,
)
from app.services.world_config_schema_summary import (
    build_world_config_allowed_field_summary,
    build_world_config_enum_summary,
    build_world_config_generation_rules,
    build_world_config_required_field_checklist,
)
from app.services.world_config_scenario_intent_extractor import (
    build_scenario_requirements,
    extract_scenario_intent,
)


ROOT = Path(__file__).resolve().parents[2]
WORLD_SCHEMA_PATH = ROOT / "schemas" / "world_config.schema.json"

SYSTEM_PROMPT = """You are a World Config JSON generator for a UE5 delivery robot simulation.
Follow world_config.schema.json exactly.
Use cm, kmh, sec, degree units.
Output one JSON object only.
Do not create fields that are not in the schema.
Do not claim certification or legal safety guarantees.
Generated JSON must pass the validation layer before UE5 handoff."""

VALIDATION_POLICY = (
    "The generated payload must pass world_config.schema.json and the Pydantic WorldConfig model. "
    "If validation fails, repair prompts may be used before any UE5 handoff."
)


def _load_world_schema() -> dict[str, Any]:
    return json.loads(WORLD_SCHEMA_PATH.read_text(encoding="utf-8-sig"))


def _schema_summary() -> dict[str, Any]:
    schema = _load_world_schema()
    return {
        "schemaPath": "schemas/world_config.schema.json",
        "title": schema.get("title", "world_config"),
        "required": schema.get("required", []),
        "properties": sorted(schema.get("properties", {}).keys()),
    }


def _format_contexts(contexts: list[Any], compact: bool = False) -> str:
    if not contexts:
        return "- No policy context retrieved. Continue with schema constraints only."
    lines: list[str] = []
    for context in contexts:
        if compact:
            lines.append(
                "- "
                f"category={context.category}; "
                f"actions={', '.join(context.relatedActions)}; "
                f"params={', '.join(context.relatedPolicyParams)}; "
                f"location={context.evidenceLocation}"
            )
        else:
            lines.append(
                "- "
                f"chunkId={context.chunkId}; cardId={context.cardId}; category={context.category}; "
                f"actions={', '.join(context.relatedActions)}; "
                f"params={', '.join(context.relatedPolicyParams)}; "
                f"location={context.evidenceLocation}; text={context.shortText}"
            )
    return "\n".join(lines)


def _format_scenario_intent(intent: Any) -> str:
    return "\n".join(
        [
            f"- mapHints: {', '.join(intent.mapHints) or 'none'}",
            f"- sidewalkWidthCm: {intent.sidewalkWidthCm if intent.sidewalkWidthCm is not None else 'none'}",
            f"- obstacleHints: {', '.join(intent.obstacleHints) or 'none'}",
            f"- obstaclePositionHint: {intent.obstaclePositionHint or 'none'}",
            f"- obstacleBlockingRatio: {intent.obstacleBlockingRatio if intent.obstacleBlockingRatio is not None else 'none'}",
            f"- pedestrianHints: {', '.join(intent.pedestrianHints) or 'none'}",
            f"- explicitNoPedestrian: {intent.explicitNoPedestrian}",
            f"- crossingHints: {', '.join(intent.crossingHints) or 'none'}",
            f"- pathBlockingHints: {intent.pathBlockingHints}",
            f"- suggestedCategories: {', '.join(intent.suggestedCategories) or 'none'}",
            f"- suggestedActions: {', '.join(intent.suggestedActions) or 'none'}",
            f"- suggestedPolicyParams: {', '.join(intent.suggestedPolicyParams) or 'none'}",
        ]
    )


def _format_scenario_requirements(requirements: list[Any]) -> str:
    if not requirements:
        return "- No specific semantic scenario requirements were inferred."
    return "\n".join(
        f"- {item.requirementId}: {item.description} expectedPath={item.expectedPath}; expectedValueHint={item.expectedValueHint}"
        for item in requirements
    )


def build_world_config_prompt_package(
    request: WorldConfigGenerationRequest,
    context_top_k: int = 5,
    compact_prompt: bool = False,
) -> WorldConfigPromptPackage:
    scenario_intent = extract_scenario_intent(request.prompt)
    environment_context = build_environment_sampling_context(request)
    scenario_requirements = apply_environment_parameters_to_scenario_requirements(
        environment_context,
        build_scenario_requirements(scenario_intent),
    )
    contexts = build_policy_context_for_world_config(request, top_k=context_top_k, compact=compact_prompt)
    warnings = [] if contexts else ["No related policy RAG chunks were retrieved."]
    constraints_json = request.constraints.model_dump_json(indent=2)
    schema_summary = _schema_summary()
    required_checklist = build_world_config_required_field_checklist()
    allowed_summary = build_world_config_allowed_field_summary()
    enum_summary = build_world_config_enum_summary()
    generation_rules = build_world_config_generation_rules()
    output_contract = build_world_config_output_contract()
    hardened_system_prompt = "\n\n".join([SYSTEM_PROMPT, output_contract, required_checklist, generation_rules])

    user_prompt = f"""User scenario prompt:
{normalize_prompt(request.prompt)}

policyId:
{request.policyId}

constraints:
{constraints_json}

Retrieved policy context:
{_format_contexts(contexts, compact=compact_prompt)}

Scenario Intent Summary:
{_format_scenario_intent(scenario_intent)}

Scenario Requirements:
{_format_scenario_requirements(scenario_requirements)}

{build_environment_constraints_prompt_section(environment_context)}

World Config required fields:
{', '.join(schema_summary['required'])}

{required_checklist}

{allowed_summary}

{enum_summary}

{generation_rules}

{output_contract}

Instructions:
- Create a World Config payload for targetContractType=world_config.
- Use fixedPolicyId when provided.
- Use defaultSeed when provided.
- If the user mentions Kickboard, obstacles must include type Kickboard.
- If the user mentions a generic/static obstacle, obstacles must include a schema-valid obstacle such as type Obstacle.
- If the user explicitly provides numeric values, preserve them exactly.
- If the user specifies map.sidewalkWidthCm, preserve that exact cm value.
- If the user specifies an obstacle coordinate, place the obstacle at that coordinate.
- If the user specifies obstacle blockingRatio, preserve that exact value.
- If the user says no pedestrians, do not create pedestrians.
- If Numeric Environment Constraints are present, use those numeric values exactly.
- Numeric Environment Constraints override vague natural-language intensity words.
- Never use low/middle/high as JSON values.
- If the user mentions pedestrian crossing, pedestrians must include a crossing behavior.
- If the user says path is blocked, obstacles must be placed on or near the robot path and include blockingRatio.
- If the user says route center, path center, route midpoint, or middle of the path, place the obstacle at the midpoint between robot.spawn and robot.goal.
- If exact obstacle coordinates are provided, exact coordinates override route midpoint placement.
- For policy comparison scenarios, describe one fixed center-blocked narrow-sidewalk scene; backend RunQueue generation keeps EpisodeSetup fixed and varies only DeliveryBotSetup policy parameters.
- Keep unclear values conservative and record assumptions only if the schema supports them.
- Extra keys are not allowed.
- Do not include markdown, comments, explanations, or extra keys.
- Do not invent keys outside the schema.
- All required nested fields must be present.
- If a value is not specified by user, choose a safe default within scenario context.
- Return JSON object only."""

    return WorldConfigPromptPackage(
        requestId=request.requestId,
        systemPrompt=hardened_system_prompt,
        userPrompt=user_prompt,
        retrievedContexts=contexts,
        schemaSummary=schema_summary,
        validationPolicy=VALIDATION_POLICY,
        scenarioIntent=scenario_intent,
        scenarioRequirements=scenario_requirements,
        environmentSampling=environment_sampling_summary(environment_context),
        warnings=warnings,
    )


def build_world_config_repair_prompt_package(
    request: WorldConfigGenerationRequest,
    invalid_payload: dict[str, Any],
    validation_errors: list[str],
    validation_error_summary: dict[str, Any] | None = None,
) -> WorldConfigRepairPromptPackage:
    scenario_requirements = apply_environment_parameters_to_scenario_requirements(
        build_environment_sampling_context(request),
        build_scenario_requirements(extract_scenario_intent(request.prompt)),
    )
    error_lines = "\n".join(f"- {error}" for error in validation_errors)
    summary = validation_error_summary or {}
    missing_lines = "\n".join(f"- {field}" for field in summary.get("missingRequiredFields", [])) or "- None listed"
    extra_lines = "\n".join(f"- {field}" for field in summary.get("extraFields", [])) or "- None listed"
    enum_lines = "\n".join(f"- {field}" for field in summary.get("enumErrors", [])) or "- None listed"
    type_lines = "\n".join(f"- {field}" for field in summary.get("typeErrors", [])) or "- None listed"
    required_checklist = build_world_config_required_field_checklist()
    allowed_summary = build_world_config_allowed_field_summary()
    generation_rules = build_world_config_generation_rules()
    output_contract = build_world_config_output_contract()
    repair_prompt = f"""The previous World Config JSON failed validation.

Validation errors:
{error_lines}

Missing required fields:
{missing_lines}

Remove schema-extra fields:
{extra_lines}

Enum errors:
{enum_lines}

Type errors:
{type_lines}

Invalid payload:
{json.dumps(invalid_payload, ensure_ascii=False, indent=2)}

{required_checklist}

{allowed_summary}

{generation_rules}

{output_contract}

Scenario Requirements:
{_format_scenario_requirements(scenario_requirements)}

Repair rules:
- Return the corrected JSON object only.
- Preserve valid parts from the previous JSON.
- Add all missing required nested fields.
- Remove all extra fields.
- Satisfy the scenario requirements.
- Fix missing fields, invalid enum values, and type errors.
- Follow world_config.schema.json exactly.
- Repair the existing JSON instead of inventing a new schema.
- Fill all missing required fields.
- Remove extra fields that are not in the schema.
- Restore any missing scenario requirements, including Kickboard obstacles, path blocking, and pedestrian crossing behavior when requested.
- For policy comparison scenarios, keep the World Config as one fixed center-blocked narrow-sidewalk scene; backend RunQueue generation keeps EpisodeSetup fixed and varies only DeliveryBotSetup policy parameters.
- Return JSON object only.
- Do not include explanations outside JSON."""

    return WorldConfigRepairPromptPackage(
        requestId=request.requestId,
        systemPrompt=SYSTEM_PROMPT,
        repairPrompt=repair_prompt,
        invalidPayload=invalid_payload,
        validationErrors=validation_errors,
        scenarioRequirements=scenario_requirements,
        validationPolicy=VALIDATION_POLICY,
        warnings=[],
    )
