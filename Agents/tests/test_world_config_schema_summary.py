from __future__ import annotations

from app.services.world_config_schema_summary import (
    build_world_config_allowed_field_summary,
    build_world_config_generation_rules,
    build_world_config_required_field_checklist,
)


def test_required_field_checklist_includes_nested_world_config_fields() -> None:
    checklist = build_world_config_required_field_checklist()

    for field in [
        "schemaVersion",
        "worldId",
        "scenarioId",
        "seed",
        "map.lengthCm",
        "map.sidewalkWidthCm",
        "robot.botId",
        "robot.spawn.x",
        "robot.spawn.y",
        "robot.spawn.z",
        "robot.goal.x",
        "robot.goal.y",
        "robot.goal.z",
        "runtime.maxDurationSec",
    ]:
        assert field in checklist


def test_allowed_field_summary_limits_top_level_fields() -> None:
    summary = build_world_config_allowed_field_summary()

    assert "Allowed top-level fields only" in summary
    assert "schemaVersion" in summary
    assert "environmentObjects" in summary
    assert "policyId" not in summary
    assert "targetContractType" not in summary


def test_generation_rules_forbid_extra_keys_and_null_required_fields() -> None:
    rules = build_world_config_generation_rules()

    assert "Extra keys are not allowed" in rules
    assert "Do not use null for required fields" in rules
    assert "Return one JSON object only" in rules
