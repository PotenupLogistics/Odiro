from __future__ import annotations

import hashlib
import random
from typing import Any

from app.models.environment import (
    EnvironmentParameterSet,
    EnvironmentSamplingRequest,
    EnvironmentSamplingResult,
    EnvironmentSamplingWarning,
    ScenarioType,
)
from app.services.environment_parameter_catalog import (
    ENVIRONMENT_PARAMETER_ALLOWED_VALUES,
    get_environment_parameter_catalog,
)


FORBIDDEN_LABEL_VALUES = {"low", "middle", "high"}

SCENARIO_ALLOWED_VALUE_OVERRIDES: dict[ScenarioType, dict[str, list[int | float]]] = {
    "narrow_sidewalk_kickboard_crossing": {
        "sidewalkWidthCm": [100, 120],
        "pedestrianCount": [1, 3],
        "obstacleBlockingRatio": [0.6, 0.9],
        "crossingAngleDeg": [90],
    },
    "obstacle_ahead": {
        "obstacleBlockingRatio": [0.3, 0.6, 0.9],
    },
    "pedestrian_crossing": {
        "pedestrianCount": [1, 3],
        "crossingAngleDeg": [90],
    },
    "terrain_risk": {
        "slopeDegree": [3, 5],
        "curbHeightCm": [3, 5],
    },
    "generic_sidewalk": {},
}


def _stable_offset(text: str) -> int:
    digest = hashlib.sha256(text.encode("utf-8")).hexdigest()
    return int(digest[:12], 16)


def sample_parameter_from_seed(
    seed: int, parameter_name: str, allowed_values: list[int | float]
) -> int | float:
    if not allowed_values:
        raise ValueError(f"No allowed values configured for {parameter_name}.")
    rng = random.Random(seed + _stable_offset(parameter_name))
    return rng.choice(allowed_values)


def _coerce_fixed_value(value: Any) -> int | float:
    if isinstance(value, bool):
        raise ValueError("Boolean fixed parameter values are not allowed.")
    if isinstance(value, str):
        normalized = value.strip().lower()
        if normalized in FORBIDDEN_LABEL_VALUES:
            raise ValueError(
                "low/middle/high labels are not allowed as fixed parameter values."
            )
        try:
            numeric_value = float(value) if "." in value else int(value)
        except ValueError as exc:
            raise ValueError("Fixed parameter values must be numeric.") from exc
        return numeric_value
    if isinstance(value, int | float):
        return value
    raise ValueError("Fixed parameter values must be numeric.")


def _validate_fixed_parameters(
    fixed_parameters: dict[str, Any],
) -> tuple[dict[str, int | float], list[EnvironmentSamplingWarning]]:
    warnings: list[EnvironmentSamplingWarning] = []
    validated: dict[str, int | float] = {}
    catalog = get_environment_parameter_catalog().allowedValues

    for parameter_name, raw_value in fixed_parameters.items():
        if parameter_name not in catalog:
            warnings.append(
                EnvironmentSamplingWarning(
                    code="unknown_fixed_parameter",
                    message=f"Unknown fixed parameter ignored: {parameter_name}",
                    parameterName=parameter_name,
                )
            )
            continue
        value = _coerce_fixed_value(raw_value)
        if value not in catalog[parameter_name]:
            raise ValueError(
                f"Fixed parameter {parameter_name}={value} is not in allowedValues."
            )
        validated[parameter_name] = value
    return validated, warnings


def _label_hints(parameters: EnvironmentParameterSet) -> dict[str, str]:
    hints: dict[str, str] = {}
    if parameters.sidewalkWidthCm in {100, 120}:
        hints["sidewalkWidthCm"] = "narrow sidewalk"
    if parameters.pedestrianCount == 5:
        hints["pedestrianCount"] = "crowded path"
    if parameters.obstacleBlockingRatio == 0.9:
        hints["obstacleBlockingRatio"] = "strong path blockage"
    if parameters.pedestrianSpeedMps == 0.8:
        hints["pedestrianSpeedMps"] = "slow pedestrian"
    if parameters.slopeDegree in {3, 5} or parameters.curbHeightCm in {3, 5}:
        hints["terrain"] = "terrain risk"
    return hints


def sample_environment_parameters(
    request: EnvironmentSamplingRequest,
) -> EnvironmentSamplingResult:
    catalog = get_environment_parameter_catalog().allowedValues
    scenario_overrides = SCENARIO_ALLOWED_VALUE_OVERRIDES[request.scenarioType]
    fixed_parameters, warnings = _validate_fixed_parameters(request.fixedParameters)
    sampled: dict[str, int | float] = {}

    for parameter_name in ENVIRONMENT_PARAMETER_ALLOWED_VALUES:
        if parameter_name in fixed_parameters:
            sampled[parameter_name] = fixed_parameters[parameter_name]
            continue
        allowed_values = scenario_overrides.get(parameter_name, catalog[parameter_name])
        sampled[parameter_name] = sample_parameter_from_seed(
            request.seed + _stable_offset(request.scenarioType),
            parameter_name,
            allowed_values,
        )

    parameters = EnvironmentParameterSet(**sampled)
    return EnvironmentSamplingResult(
        requestId=request.requestId,
        seed=request.seed,
        scenarioType=request.scenarioType,
        parameters=parameters,
        labelHints=_label_hints(parameters) if request.includeLabels else {},
        warnings=warnings,
    )
