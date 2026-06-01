from __future__ import annotations

from typing import Any

from app.models.environment import (
    EnvironmentParameterSet,
    EnvironmentSamplingContext,
    EnvironmentSamplingRequest,
    ScenarioType,
)
from app.models.generation import WorldConfigGenerationRequest
from app.models.scenario import ScenarioRequirement
from app.services.environment_parameter_sampler import sample_environment_parameters


DEFAULT_SCENARIO_TYPE: ScenarioType = "generic_sidewalk"


def _environment_sampling_config(request: WorldConfigGenerationRequest) -> dict[str, Any]:
    value = request.constraints.environmentSampling
    return value if isinstance(value, dict) else {}


def build_environment_sampling_context(
    generation_request: WorldConfigGenerationRequest,
) -> EnvironmentSamplingContext | None:
    config = _environment_sampling_config(generation_request)
    if not config or not bool(config.get("enabled", False)):
        return None

    seed = config.get("seed")
    if seed is None:
        seed = generation_request.constraints.defaultSeed
    if seed is None:
        seed = 0

    scenario_type = config.get("scenarioType") or DEFAULT_SCENARIO_TYPE
    fixed_parameters = config.get("fixedParameters") or {}
    sampling_request = EnvironmentSamplingRequest(
        requestId=f"{generation_request.requestId}-ENV",
        seed=int(seed),
        scenarioType=scenario_type,
        fixedParameters=fixed_parameters,
        includeLabels=False,
    )
    result = sample_environment_parameters(sampling_request)
    return EnvironmentSamplingContext(
        enabled=True,
        seed=result.seed,
        scenarioType=result.scenarioType,
        parameters=result.parameters,
        fixedParameters=dict(fixed_parameters),
        warnings=result.warnings,
    )


def environment_sampling_summary(context: EnvironmentSamplingContext | None) -> dict[str, Any] | None:
    if context is None:
        return None
    parameters = context.parameters.model_dump(mode="json") if context.parameters else None
    return {
        "enabled": context.enabled,
        "seed": context.seed,
        "scenarioType": context.scenarioType,
        "parameters": parameters,
        "fixedParameters": dict(context.fixedParameters),
        "warnings": [warning.model_dump(mode="json") for warning in context.warnings],
    }


def _parameter_lines(parameters: EnvironmentParameterSet) -> list[str]:
    return [
        f"- map.sidewalkWidthCm must be {parameters.sidewalkWidthCm}",
        f"- pedestrianCount must be {parameters.pedestrianCount}",
        f"- pedestrianSpeedMps must be {parameters.pedestrianSpeedMps}",
        f"- obstacleBlockingRatio must be {parameters.obstacleBlockingRatio}",
        f"- obstacleLateralOffsetM must be {parameters.obstacleLateralOffsetM}",
        f"- crossingAngleDeg must be {parameters.crossingAngleDeg}",
        f"- robotSpeedKmh must be {parameters.robotSpeedKmh}",
        f"- map.slopeDegree must be {parameters.slopeDegree}",
        f"- curbHeightCm must be {parameters.curbHeightCm}",
        f"- runtime.maxDurationSec must be {parameters.timeLimitSec}",
    ]


def build_environment_constraints_prompt_section(
    context: EnvironmentSamplingContext | None,
) -> str:
    if context is None or context.parameters is None:
        return "- Environment sampling disabled."
    lines = [
        "Numeric Environment Constraints:",
        f"- environmentSampling.enabled: {context.enabled}",
        f"- seed: {context.seed}",
        f"- scenarioType: {context.scenarioType}",
        *_parameter_lines(context.parameters),
        "- Use these numeric values exactly.",
        "- Do not replace numeric values with low/middle/high labels.",
        "- If environment constraints conflict with vague natural language, numeric values win.",
    ]
    return "\n".join(lines)


def apply_environment_parameters_to_scenario_requirements(
    context: EnvironmentSamplingContext | None,
    scenario_requirements: list[ScenarioRequirement],
) -> list[ScenarioRequirement]:
    if context is None or context.parameters is None:
        return list(scenario_requirements)

    parameters = context.parameters
    requirements = list(scenario_requirements)
    requirements.append(
        ScenarioRequirement(
            requirementId="environment_sidewalk_width",
            type="environment_sampling",
            description="Apply sampled sidewalk width from numeric environment constraints.",
            requiredInWorldConfig=True,
            expectedPath="map.sidewalkWidthCm",
            expectedValueHint=f"Must be exactly {parameters.sidewalkWidthCm}.",
        )
    )
    requirements.append(
        ScenarioRequirement(
            requirementId="environment_obstacle_blocking_ratio",
            type="environment_sampling",
            description="Apply sampled obstacle blocking ratio from numeric environment constraints.",
            requiredInWorldConfig=True,
            expectedPath="obstacles[].blockingRatio",
            expectedValueHint=f"Must be exactly {parameters.obstacleBlockingRatio}.",
        )
    )
    requirements.append(
        ScenarioRequirement(
            requirementId="environment_time_limit",
            type="environment_sampling",
            description="Apply sampled time limit from numeric environment constraints.",
            requiredInWorldConfig=True,
            expectedPath="runtime.maxDurationSec",
            expectedValueHint=f"Must be exactly {parameters.timeLimitSec}.",
        )
    )
    return requirements
