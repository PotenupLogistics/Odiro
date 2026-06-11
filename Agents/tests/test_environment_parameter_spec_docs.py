from __future__ import annotations

from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def test_environment_parameter_spec_exists_and_lists_core_parameters() -> None:
    spec_path = ROOT / "docs" / "environment" / "ENVIRONMENT_PARAMETER_SPEC.md"
    assert spec_path.exists()

    text = spec_path.read_text(encoding="utf-8-sig")
    for term in [
        "sidewalkWidthCm",
        "pedestrianCount",
        "obstacleBlockingRatio",
        "pedestrianSpeedMps",
    ]:
        assert term in text


def test_environment_parameter_spec_rejects_low_middle_high_as_json_values() -> None:
    text = (ROOT / "docs" / "environment" / "ENVIRONMENT_PARAMETER_SPEC.md").read_text(
        encoding="utf-8-sig"
    )

    assert "Do not use low / middle / high as JSON values" in text
    assert "same label must map to the same numeric value" in text
    assert "sidewalkWidthCm=100 or 120" in text
    assert "pedestrianCount=5" in text
    assert 'pedestrianDensity: "high"' in text
    assert "sidewalkWidthCm: 120" in text


def test_episode_spec_adapter_documents_unit_conversion() -> None:
    text = (ROOT / "docs" / "archive" / "previous_episode_spec" / "UE5_EPISODE_SPEC_ADAPTER.md").read_text(
        encoding="utf-8-sig"
    )

    assert "cm to m" in text
    assert "sidewalkWidthCm" in text
    assert "shape.size_m" in text
    assert "pedestrianSpeedMps" in text
    assert "movement.speed_mps" in text
    assert "obstacleBlockingRatio" in text
    assert "properties.blocking_ratio" in text
    assert "timeLimitSec" in text
    assert "run.time_limit_s" in text


def test_world_config_field_mapping_documents_environment_parameters() -> None:
    text = (ROOT / "docs" / "archive" / "previous_episode_spec" / "UE5_WORLD_CONFIG_FIELD_MAPPING.md").read_text(
        encoding="utf-8-sig"
    )

    assert "Environment Parameter Conversion" in text
    assert "map.lengthCm" in text
    assert "12.0" in text
    assert "obstacleBlockingRatio" in text
    assert "properties.blocking_ratio" in text


def test_readmes_link_environment_parameter_spec() -> None:
    root_readme = (ROOT / "README.md").read_text(encoding="utf-8-sig")
    docs_readme = (ROOT / "docs" / "README.md").read_text(encoding="utf-8-sig")

    assert "ENVIRONMENT_PARAMETER_SPEC.md" in root_readme
    assert "ENVIRONMENT_PARAMETER_SPEC.md" in docs_readme


def test_environment_parameter_work_did_not_create_forbidden_artifacts() -> None:
    forbidden_paths = [
        ROOT / "samples",
        ROOT / "fixtures",
        ROOT / "data" / "rag" / "vector_db",
        ROOT / "data" / "rag" / "embeddings",
        ROOT / "ue",
        ROOT / "UE",
    ]
    for path in forbidden_paths:
        assert not path.exists(), f"Forbidden artifact exists: {path}"
