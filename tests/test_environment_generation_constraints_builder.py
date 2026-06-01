from __future__ import annotations

import pytest

from app.models.generation import WorldConfigGenerationConstraints, WorldConfigGenerationRequest
from app.services.environment_generation_constraints_builder import (
    apply_environment_parameters_to_scenario_requirements,
    build_environment_constraints_prompt_section,
    build_environment_sampling_context,
)
from app.services.world_config_scenario_intent_extractor import (
    build_scenario_requirements,
    extract_scenario_intent,
)


def _request(seed: int = 1001, fixed_parameters: dict | None = None) -> WorldConfigGenerationRequest:
    return WorldConfigGenerationRequest(
        schemaVersion="1.0",
        requestId="REQ-ENV-GEN-001",
        generationType="world_config",
        prompt="좁은 보도에서 정적 장애물이 경로를 막는 상황",
        targetContractType="world_config",
        policyId="policy_v1_basic_safety",
        constraints=WorldConfigGenerationConstraints(
            unitSystem="cm_kmh_sec_degree",
            allowedMapTypes=["Sidewalk"],
            allowedObjectTypes=["Obstacle"],
            fixedPolicyId="policy_v1_basic_safety",
            defaultSeed=7,
            requireValidation=True,
            environmentSampling={
                "enabled": True,
                "seed": seed,
                "scenarioType": "obstacle_ahead",
                "fixedParameters": fixed_parameters or {},
            },
        ),
    )


def test_build_environment_sampling_context_is_deterministic_for_seed_and_scenario_type() -> None:
    first = build_environment_sampling_context(_request(seed=1001))
    second = build_environment_sampling_context(_request(seed=1001))

    assert first is not None
    assert second is not None
    assert first.parameters == second.parameters
    assert first.seed == 1001
    assert first.scenarioType == "obstacle_ahead"


def test_fixed_parameters_override_sampled_values() -> None:
    context = build_environment_sampling_context(
        _request(
            fixed_parameters={
                "sidewalkWidthCm": 120,
                "obstacleBlockingRatio": 0.6,
                "timeLimitSec": 60,
            }
        )
    )

    assert context is not None
    assert context.parameters is not None
    assert context.parameters.sidewalkWidthCm == 120
    assert context.parameters.obstacleBlockingRatio == 0.6
    assert context.parameters.timeLimitSec == 60


def test_fixed_parameters_reject_low_middle_high_labels() -> None:
    with pytest.raises(ValueError, match="low/middle/high"):
        build_environment_sampling_context(
            _request(fixed_parameters={"sidewalkWidthCm": "high"})
        )


def test_prompt_section_contains_numeric_environment_constraints() -> None:
    context = build_environment_sampling_context(
        _request(fixed_parameters={"sidewalkWidthCm": 120, "obstacleBlockingRatio": 0.6})
    )
    section = build_environment_constraints_prompt_section(context)

    assert "Numeric Environment Constraints" in section
    assert "map.sidewalkWidthCm must be 120" in section
    assert "obstacleBlockingRatio must be 0.6" in section
    assert "Do not replace numeric values with low/middle/high labels." in section


def test_apply_environment_parameters_adds_scenario_requirements() -> None:
    context = build_environment_sampling_context(_request())
    requirements = apply_environment_parameters_to_scenario_requirements(
        context,
        build_scenario_requirements(extract_scenario_intent("정적 장애물이 경로를 막는 상황")),
    )
    requirement_ids = {requirement.requirementId for requirement in requirements}

    assert "environment_sidewalk_width" in requirement_ids
    assert "environment_obstacle_blocking_ratio" in requirement_ids
    assert "environment_time_limit" in requirement_ids
