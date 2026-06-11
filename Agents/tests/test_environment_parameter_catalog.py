from __future__ import annotations

from app.services.environment_parameter_catalog import (
    get_allowed_values,
    get_environment_parameter_catalog,
)


def test_catalog_contains_core_parameters() -> None:
    catalog = get_environment_parameter_catalog()

    assert catalog.allowedValues["sidewalkWidthCm"] == [100, 120, 150, 200]
    assert catalog.allowedValues["pedestrianCount"] == [1, 3, 5]
    assert catalog.allowedValues["obstacleBlockingRatio"] == [0.3, 0.6, 0.9]
    assert catalog.allowedValues["pedestrianSpeedMps"] == [0.8, 1.2, 1.6]


def test_get_allowed_values_returns_copy() -> None:
    values = get_allowed_values("sidewalkWidthCm")
    values.append(999)

    assert get_allowed_values("sidewalkWidthCm") == [100, 120, 150, 200]
