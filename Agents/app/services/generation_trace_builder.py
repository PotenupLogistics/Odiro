from __future__ import annotations

from typing import Any

from app.models.episode_spec import EpisodeScenarioReflectionResult, EpisodeValidationResult
from app.models.generation import WorldConfigGenerationRequest, WorldConfigGenerationResult
from app.models.generation_trace import GenerationTrace, GenerationTraceItem, GenerationTraceSummary, TraceSourceType
from app.models.scenario import ScenarioPostProcessResult, ScenarioReflectionResult
from app.services.route_geometry_utils import compute_midpoint
from app.services.world_config_scenario_intent_extractor import extract_scenario_intent


def _short(value: Any) -> str | int | float | bool | None:
    if value is None or isinstance(value, str | int | float | bool):
        return value
    if isinstance(value, list):
        return f"list[{len(value)}]"
    if isinstance(value, dict):
        keys = ", ".join(sorted(str(key) for key in value.keys())[:5])
        return f"object[{keys}]"
    return str(value)


def _parameters(environment_sampling: dict[str, Any] | None) -> dict[str, Any]:
    if not isinstance(environment_sampling, dict):
        return {}
    parameters = environment_sampling.get("parameters")
    return parameters if isinstance(parameters, dict) else {}


def _fixed_parameters(request: WorldConfigGenerationRequest) -> dict[str, Any]:
    sampling = request.constraints.environmentSampling or {}
    fixed = sampling.get("fixedParameters") if isinstance(sampling, dict) else None
    return fixed if isinstance(fixed, dict) else {}


def _episode_size_m(episode_spec: dict[str, Any] | None) -> list[Any] | None:
    if not isinstance(episode_spec, dict):
        return None
    regions = ((episode_spec.get("ground_model") or {}).get("regions") or [])
    if not regions or not isinstance(regions[0], dict):
        return None
    size_m = ((regions[0].get("shape") or {}).get("size_m") or None)
    return size_m if isinstance(size_m, list) else None


def _robot_path(payload: dict[str, Any] | None) -> tuple[dict[str, Any], dict[str, Any]]:
    if not isinstance(payload, dict):
        return {}, {}
    robot = payload.get("robot") if isinstance(payload.get("robot"), dict) else {}
    return robot.get("spawn") or {}, robot.get("goal") or {}


def _coordinate_source_from_items(items: list[GenerationTraceItem]) -> str:
    source_types = {item.sourceType.value for item in items}
    active = []
    if "user_prompt" in source_types or "scenario_intent" in source_types:
        active.append("explicit_user_coordinates")
    if "environment_sampling" in source_types:
        active.append("environment_sampling")
    if "placement_rule" in source_types:
        active.append("placement_rule")
    if len(active) > 1:
        return "mixed"
    return active[0] if active else "unknown"


def _policy_rag_used_for(items: list[GenerationTraceItem]) -> str:
    policy_items = [item for item in items if item.sourceType == TraceSourceType.policy_rag]
    if not policy_items:
        return "unknown"
    if any("safety context" in (item.reason or "").lower() for item in policy_items):
        return "safety_context"
    if any(item.valueSummary == "not_used" for item in policy_items):
        return "not_used"
    return "unknown"


def build_user_prompt_trace_items(request: WorldConfigGenerationRequest) -> list[GenerationTraceItem]:
    return [
        GenerationTraceItem(
            sourceType=TraceSourceType.user_prompt,
            fieldPath="generationRequest.prompt",
            valueSummary=f"{len(request.prompt)} chars",
            evidence=request.prompt[:160],
            reason="User natural language is the primary scenario request.",
            priority=10,
        )
    ]


def build_scenario_intent_trace_items(request: WorldConfigGenerationRequest) -> list[GenerationTraceItem]:
    intent = extract_scenario_intent(request.prompt)
    items: list[GenerationTraceItem] = []
    if intent.obstacleHints or intent.pathBlockingHints:
        items.append(
            GenerationTraceItem(
                sourceType=TraceSourceType.scenario_intent,
                fieldPath="scenarioIntent.obstacle",
                valueSummary=", ".join(intent.obstacleHints) or "path_blocking",
                evidence="obstacle/path-blocking expressions detected in prompt",
                reason="Scenario intent extraction identifies obstacle requirements.",
                priority=9,
            )
        )
    if intent.obstaclePlacementHint:
        items.append(
            GenerationTraceItem(
                sourceType=TraceSourceType.scenario_intent,
                fieldPath="scenarioIntent.obstaclePlacementHint",
                valueSummary=intent.obstaclePlacementHint,
                evidence="route center/path midpoint expression detected",
                rule="route_midpoint_intent",
                reason="Path-relative language is converted into deterministic placement intent.",
                priority=9,
            )
        )
    if intent.explicitNoPedestrian:
        items.append(
            GenerationTraceItem(
                sourceType=TraceSourceType.scenario_intent,
                fieldPath="scenarioIntent.explicitNoPedestrian",
                valueSummary=True,
                evidence="no pedestrian expression detected",
                reason="Pedestrian actors and paths should remain empty.",
                priority=8,
            )
        )
    return items


def build_environment_sampling_trace_items(
    request: WorldConfigGenerationRequest,
    environment_sampling: dict[str, Any] | None,
) -> list[GenerationTraceItem]:
    parameters = _parameters(environment_sampling)
    fixed = _fixed_parameters(request)
    fields = [
        ("sidewalkWidthCm", "map.sidewalkWidthCm"),
        ("obstacleBlockingRatio", "obstacles[].blockingRatio"),
        ("timeLimitSec", "runtime.maxDurationSec"),
    ]
    items: list[GenerationTraceItem] = []
    for key, field_path in fields:
        if key not in parameters:
            continue
        items.append(
            GenerationTraceItem(
                sourceType=TraceSourceType.environment_sampling,
                fieldPath=field_path,
                valueSummary=parameters.get(key),
                evidence=f"environmentSampling.parameters.{key}",
                rule="fixed_parameter_override" if key in fixed else "seed_deterministic_sampling",
                reason=(
                    f"fixedParameters.{key} was provided"
                    if key in fixed
                    else f"{key} was sampled from seed/scenarioType"
                ),
                priority=10 if key in fixed else 8,
                inputs={
                    "seed": (environment_sampling or {}).get("seed") if isinstance(environment_sampling, dict) else None,
                    "scenarioType": (environment_sampling or {}).get("scenarioType") if isinstance(environment_sampling, dict) else None,
                },
            )
        )
    return items


def build_placement_trace_items(
    request: WorldConfigGenerationRequest,
    world_config: dict[str, Any] | None,
    environment_sampling: dict[str, Any] | None,
) -> list[GenerationTraceItem]:
    intent = extract_scenario_intent(request.prompt)
    if intent.obstaclePlacementHint != "route_midpoint" or intent.obstaclePositionHint is not None:
        return []
    spawn, goal = _robot_path(world_config)
    midpoint = compute_midpoint(spawn, goal)
    parameters = _parameters(environment_sampling)
    lateral_offset = parameters.get("obstacleLateralOffsetM")
    return [
        GenerationTraceItem(
            sourceType=TraceSourceType.placement_rule,
            fieldPath="obstacles[0].position",
            valueSummary=f"x={midpoint['x']:g}, y={midpoint['y']:g}, z={midpoint['z']:g}",
            evidence="Prompt contains route midpoint language.",
            rule="route_midpoint_with_lateral_offset",
            reason="Obstacle position is derived from robot.spawn and robot.goal, not from policy RAG.",
            priority=10,
            inputs={"spawn": spawn, "goal": goal, "obstacleLateralOffsetM": lateral_offset},
            calculation="midpoint(robot.spawn, robot.goal) plus lateral offset when configured",
        )
    ]


def build_episode_spec_adapter_trace_items(
    world_config: dict[str, Any] | None,
    episode_spec: dict[str, Any] | None,
) -> list[GenerationTraceItem]:
    if not isinstance(world_config, dict) or not isinstance(episode_spec, dict):
        return []
    map_config = world_config.get("map") if isinstance(world_config.get("map"), dict) else {}
    size_m = _episode_size_m(episode_spec)
    items = [
        GenerationTraceItem(
            sourceType=TraceSourceType.episode_spec_adapter,
            fieldPath="episodeSpec.ground_model.regions[0].shape.size_m",
            valueSummary=str(size_m),
            evidence="WorldConfig map length/sidewalk width",
            rule="cm_to_m",
            reason="WorldConfig cm values converted to EpisodeSpec meters.",
            priority=8,
            inputs={
                "lengthCm": map_config.get("lengthCm"),
                "sidewalkWidthCm": map_config.get("sidewalkWidthCm"),
            },
            calculation="size_m=[lengthCm/100, sidewalkWidthCm/100]",
        )
    ]
    obstacles = ((episode_spec.get("actors") or {}).get("static_obstacles") or [])
    if obstacles:
        properties = obstacles[0].get("properties") if isinstance(obstacles[0], dict) else {}
        items.append(
            GenerationTraceItem(
                sourceType=TraceSourceType.episode_spec_adapter,
                fieldPath="episodeSpec.actors.static_obstacles[0].properties",
                valueSummary=_short(properties),
                evidence="WorldConfig obstacles[] converted to EpisodeSpec actors.static_obstacles[]",
                reason="Obstacle semantic_type and blocking_ratio are preserved as shallow properties.",
                priority=8,
            )
        )
    return items


def build_policy_rag_trace_items(result: WorldConfigGenerationResult) -> list[GenerationTraceItem]:
    if not result.retrievedContexts:
        return [
            GenerationTraceItem(
                sourceType=TraceSourceType.policy_rag,
                fieldPath="relatedPolicyContext",
                valueSummary="not_used",
                reason="No policy RAG chunks were retrieved for this request.",
                priority=3,
            )
        ]
    categories = sorted({context.category for context in result.retrievedContexts})
    return [
        GenerationTraceItem(
            sourceType=TraceSourceType.policy_rag,
            fieldPath="relatedPolicyContext",
            valueSummary=f"{len(result.retrievedContexts)} chunks",
            evidence=", ".join(categories),
            reason="Policy RAG used for safety context, not coordinate generation.",
            priority=5,
            inputs={"categories": categories},
        )
    ]


def build_validation_trace_items(
    generation_result: WorldConfigGenerationResult,
    episode_validation: EpisodeValidationResult | None = None,
) -> list[GenerationTraceItem]:
    items = [
        GenerationTraceItem(
            sourceType=TraceSourceType.validation,
            fieldPath="worldConfig.validation.status",
            valueSummary=generation_result.validation.status,
            reason="WorldConfig schema/Pydantic validation status.",
            priority=7,
        )
    ]
    if episode_validation is not None:
        items.append(
            GenerationTraceItem(
                sourceType=TraceSourceType.validation,
                fieldPath="episodeValidation.valid",
                valueSummary=episode_validation.valid,
                reason="EpisodeSpec contract validation status.",
                priority=7,
            )
        )
    return items


def build_post_processing_trace_items(
    post_processing: ScenarioPostProcessResult | None,
) -> list[GenerationTraceItem]:
    if post_processing is None:
        return []
    items: list[GenerationTraceItem] = []
    for patch in post_processing.patches:
        items.append(
            GenerationTraceItem(
                sourceType=TraceSourceType.post_processing,
                fieldPath=patch.targetPath,
                valueSummary=_short(patch.afterValue),
                evidence=patch.patchType,
                rule=patch.patchType,
                reason=patch.reason,
                priority=9,
                inputs={"before": _short(patch.beforeValue), "after": _short(patch.afterValue)},
            )
        )
    return items


def build_scenario_reflection_trace_items(
    scenario_reflection: ScenarioReflectionResult | None,
    episode_reflection: EpisodeScenarioReflectionResult | None = None,
) -> list[GenerationTraceItem]:
    items: list[GenerationTraceItem] = []
    if scenario_reflection is not None:
        items.append(
            GenerationTraceItem(
                sourceType=TraceSourceType.scenario_reflection,
                fieldPath="scenarioReflection.checkedRequirements",
                valueSummary=f"{len(scenario_reflection.checkedRequirements)} checked",
                reason=scenario_reflection.summary,
                priority=7,
            )
        )
    if episode_reflection is not None:
        items.append(
            GenerationTraceItem(
                sourceType=TraceSourceType.scenario_reflection,
                fieldPath="episodeScenarioReflection.ueCompilerReadiness",
                valueSummary=episode_reflection.ueCompilerReadiness,
                reason="EpisodeSpec scenario reflection and UE compiler readiness summary.",
                priority=8,
                inputs={
                    "staticObstacleCount": episode_reflection.staticObstacleCount,
                    "hasBlockingRatio": episode_reflection.hasBlockingRatio,
                    "pedestrianCount": episode_reflection.pedestrianCount,
                },
            )
        )
    return items


def build_failure_trace_items(
    failure_stage: str | None,
    error_summary: str | None,
) -> list[GenerationTraceItem]:
    if not failure_stage and not error_summary:
        return []
    return [
        GenerationTraceItem(
            sourceType=TraceSourceType.validation,
            fieldPath="failureStage",
            valueSummary=failure_stage,
            evidence=error_summary,
            reason="Generation or handoff stopped before a complete EpisodeSpec response.",
            priority=10,
        )
    ]


def infer_failure_stage(generation_result: WorldConfigGenerationResult) -> tuple[str | None, str | None]:
    if generation_result.success:
        return None, None
    if generation_result.validation.status == "failed":
        summary = "; ".join(generation_result.validation.errors[:3]) if generation_result.validation.errors else None
        return "world_config_validation", summary or "WorldConfig validation failed."
    if generation_result.scenarioReflection is not None and not generation_result.scenarioReflection.passed:
        return "scenario_reflection", generation_result.scenarioReflection.summary
    if generation_result.generatedPayload is None:
        summary = generation_result.error.message if generation_result.error else "WorldConfig generation failed."
        return "world_config_generation", summary
    if generation_result.error is not None:
        return "world_config_generation", generation_result.error.message
    return "unknown", "Generation failed before a precise failure stage could be determined."


def build_generation_trace(
    request: WorldConfigGenerationRequest,
    generation_result: WorldConfigGenerationResult,
    episode_spec: dict[str, Any] | None = None,
    episode_validation: EpisodeValidationResult | None = None,
    episode_scenario_reflection: EpisodeScenarioReflectionResult | None = None,
    failure_stage: str | None = None,
    error_summary: str | None = None,
) -> GenerationTrace:
    if failure_stage is None and not generation_result.success:
        failure_stage, inferred_error_summary = infer_failure_stage(generation_result)
        error_summary = error_summary or inferred_error_summary
    items: list[GenerationTraceItem] = []
    warnings: list[str] = []
    for builder in [
        lambda: build_user_prompt_trace_items(request),
        lambda: build_scenario_intent_trace_items(request),
        lambda: build_environment_sampling_trace_items(request, generation_result.environmentSampling),
        lambda: build_policy_rag_trace_items(generation_result),
        lambda: build_placement_trace_items(request, generation_result.generatedPayload, generation_result.environmentSampling),
        lambda: build_post_processing_trace_items(generation_result.scenarioPostProcessing),
        lambda: build_episode_spec_adapter_trace_items(generation_result.generatedPayload, episode_spec),
        lambda: build_validation_trace_items(generation_result, episode_validation),
        lambda: build_scenario_reflection_trace_items(generation_result.scenarioReflection, episode_scenario_reflection),
        lambda: build_failure_trace_items(failure_stage, error_summary),
    ]:
        try:
            items.extend(builder())
        except Exception as exc:  # pragma: no cover - defensive trace hardening
            warnings.append(f"trace_item_builder_failed: {exc}")
    status = "success" if generation_result.success and episode_spec is not None and failure_stage is None else "failed" if failure_stage else "partial"
    summary = GenerationTraceSummary(
        message=(
            "Map generation trace contains summary evidence only; full WorldConfig, "
            "full EpisodeSpec, raw model output, and secrets are not stored."
        ),
        status=status,
        failureStage=failure_stage,
        errorSummary=error_summary,
        coordinateSource=_coordinate_source_from_items(items),
        policyRagUsedFor=_policy_rag_used_for(items),
    ).model_dump(mode="json")
    return GenerationTrace(
        requestId=request.requestId,
        summary=summary,
        evidenceItems=items,
        warnings=warnings,
    )
