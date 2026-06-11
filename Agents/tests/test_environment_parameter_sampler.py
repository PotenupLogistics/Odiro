from __future__ import annotations

import pytest

from app.models.environment import EnvironmentSamplingRequest
from app.services.environment_parameter_sampler import sample_environment_parameters


def _request(seed: int = 1001, **overrides: object) -> EnvironmentSamplingRequest:
    payload = {
        "requestId": "TEST-ENV-SAMPLER-001",
        "seed": seed,
        "scenarioType": "narrow_sidewalk_kickboard_crossing",
    }
    payload.update(overrides)
    return EnvironmentSamplingRequest(**payload)


def test_same_seed_and_scenario_are_deterministic() -> None:
    first = sample_environment_parameters(_request())
    second = sample_environment_parameters(_request())

    assert first.parameters == second.parameters


def test_different_seed_can_change_some_values() -> None:
    first = sample_environment_parameters(_request(seed=1001))
    second = sample_environment_parameters(_request(seed=1002))

    assert first.parameters != second.parameters


def test_narrow_sidewalk_kickboard_crossing_uses_scenario_tendencies() -> None:
    result = sample_environment_parameters(_request())

    assert result.parameters.sidewalkWidthCm in {100, 120}
    assert result.parameters.obstacleBlockingRatio in {0.6, 0.9}
    assert result.parameters.crossingAngleDeg == 90


def test_fixed_parameters_take_precedence() -> None:
    result = sample_environment_parameters(
        _request(fixedParameters={"sidewalkWidthCm": 120, "pedestrianCount": 3})
    )

    assert result.parameters.sidewalkWidthCm == 120
    assert result.parameters.pedestrianCount == 3


def test_fixed_parameter_outside_allowed_values_fails() -> None:
    with pytest.raises(ValueError, match="allowedValues"):
        sample_environment_parameters(_request(fixedParameters={"sidewalkWidthCm": 130}))


@pytest.mark.parametrize("label", ["high", "middle", "low"])
def test_low_middle_high_fixed_parameter_values_fail(label: str) -> None:
    with pytest.raises(ValueError, match="low/middle/high"):
        sample_environment_parameters(_request(fixedParameters={"sidewalkWidthCm": label}))
