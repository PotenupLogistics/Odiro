from __future__ import annotations

from app.services.world_config_output_contract_builder import (
    build_world_config_extra_field_prohibition,
    build_world_config_nested_required_paths,
    build_world_config_output_contract,
    build_world_config_scenario_binding_rules,
)


def test_output_contract_includes_required_nested_paths() -> None:
    paths = build_world_config_nested_required_paths()

    assert "schemaVersion" in paths
    assert "map.lengthCm" in paths
    assert "map.sidewalkWidthCm" in paths
    assert "robot.botId" in paths
    assert "robot.spawn.x" in paths
    assert "robot.goal.x" in paths
    assert "runtime.maxDurationSec" in paths
    assert "robot.spawn.yawDegree" not in paths


def test_output_contract_includes_allowed_top_level_fields_and_extra_key_rule() -> None:
    contract = build_world_config_output_contract()

    assert "Output Contract" in contract
    assert "Allowed top-level fields" in contract
    assert "schemaVersion" in contract
    assert "environmentObjects" in contract
    assert "Do not invent keys outside the schema" in contract


def test_extra_field_prohibition_is_explicit() -> None:
    text = build_world_config_extra_field_prohibition()

    assert "Do not include markdown, comments, explanations, or extra keys" in text
    assert "Remove all extra fields" in text


def test_scenario_binding_rules_include_schema_paths() -> None:
    rules = build_world_config_scenario_binding_rules()

    assert "Kickboard" in rules
    assert "obstacles[].type" in rules
    assert "pedestrians[].behavior" in rules
    assert "obstacles[].blockingRatio" in rules
