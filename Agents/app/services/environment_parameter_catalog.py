from __future__ import annotations

from app.models.environment import EnvironmentParameterCatalog


ENVIRONMENT_PARAMETER_ALLOWED_VALUES: dict[str, list[int | float]] = {
    "sidewalkWidthCm": [100, 120, 150, 200],
    "pedestrianCount": [1, 3, 5],
    "pedestrianSpeedMps": [0.8, 1.2, 1.6],
    "obstacleBlockingRatio": [0.3, 0.6, 0.9],
    "obstacleLateralOffsetM": [-0.4, 0.0, 0.4],
    "crossingAngleDeg": [45, 90],
    "robotSpeedKmh": [3, 5, 8],
    "slopeDegree": [0, 3, 5],
    "curbHeightCm": [0, 3, 5],
    "timeLimitSec": [30, 60, 90],
}


def get_environment_parameter_catalog() -> EnvironmentParameterCatalog:
    return EnvironmentParameterCatalog(
        allowedValues={
            name: list(values)
            for name, values in ENVIRONMENT_PARAMETER_ALLOWED_VALUES.items()
        }
    )


def get_allowed_values(parameter_name: str) -> list[int | float]:
    values = ENVIRONMENT_PARAMETER_ALLOWED_VALUES.get(parameter_name)
    if values is None:
        raise KeyError(f"Unknown environment parameter: {parameter_name}")
    return list(values)
