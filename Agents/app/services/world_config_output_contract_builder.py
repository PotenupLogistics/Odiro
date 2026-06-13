from __future__ import annotations

import json
from pathlib import Path
from typing import Any


AGENTS_ROOT = Path(__file__).resolve().parents[2]
REPO_ROOT = AGENTS_ROOT.parent
WORLD_SCHEMA_PATH = REPO_ROOT / "contracts" / "schemas" / "world_config.schema.json"


def _load_schema() -> dict[str, Any]:
    return json.loads(WORLD_SCHEMA_PATH.read_text(encoding="utf-8-sig"))


def _resolve_ref(schema: dict[str, Any], ref: str) -> dict[str, Any]:
    if not ref.startswith("#/$defs/"):
        return {}
    return schema.get("$defs", {}).get(ref.removeprefix("#/$defs/"), {})


def _property_schema(schema: dict[str, Any], node: dict[str, Any]) -> dict[str, Any]:
    if "$ref" in node:
        return _resolve_ref(schema, str(node["$ref"]))
    return node


def _collect_required(schema: dict[str, Any], node: dict[str, Any], prefix: str = "") -> list[str]:
    paths: list[str] = []
    properties = node.get("properties", {})
    for field in node.get("required", []):
        path = f"{prefix}.{field}" if prefix else str(field)
        paths.append(path)
        child = properties.get(field)
        if isinstance(child, dict):
            child_schema = _property_schema(schema, child)
            if child_schema.get("properties"):
                paths.extend(_collect_required(schema, child_schema, path))
    return paths


def build_world_config_nested_required_paths() -> list[str]:
    schema = _load_schema()
    return _collect_required(schema, schema)


def build_world_config_extra_field_prohibition() -> str:
    return "\n".join(
        [
            "Extra Field Prohibition:",
            "- Do not include markdown, comments, explanations, or extra keys.",
            "- Do not invent keys outside the schema.",
            "- Remove all extra fields reported by validation.",
            "- Use only allowed top-level fields and allowed nested schema fields.",
        ]
    )


def build_world_config_scenario_binding_rules() -> str:
    return "\n".join(
        [
            "Scenario Requirement Binding:",
            "- narrow_sidewalk -> map.sidewalkWidthCm",
            "- Kickboard -> obstacles[].type must include \"Kickboard\"",
            "- pathBlockingHints -> obstacles[].blockingRatio must be greater than 0",
            "- Pedestrian -> pedestrians[] must contain at least one item",
            "- pedestrian_crossing -> pedestrians[].behavior should be \"Crossing\" or equivalent crossing behavior",
        ]
    )


def build_world_config_output_contract() -> str:
    schema = _load_schema()
    required_paths = build_world_config_nested_required_paths()
    allowed_top_level = sorted(schema.get("properties", {}).keys())
    return "\n".join(
        [
            "Output Contract:",
            "You must return exactly one JSON object.",
            "All required nested fields must be present.",
            "",
            "Required nested paths:",
            *[f"- {path}" for path in required_paths],
            "",
            "Allowed top-level fields:",
            *[f"- {field}" for field in allowed_top_level],
            "",
            build_world_config_extra_field_prohibition(),
            "",
            build_world_config_scenario_binding_rules(),
        ]
    )
