from __future__ import annotations

import json
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[2]
WORLD_SCHEMA_PATH = ROOT / "schemas" / "world_config.schema.json"


def _load_schema() -> dict[str, Any]:
    return json.loads(WORLD_SCHEMA_PATH.read_text(encoding="utf-8-sig"))


def _resolve_ref(schema: dict[str, Any], ref: str) -> dict[str, Any]:
    if not ref.startswith("#/$defs/"):
        return {}
    name = ref.removeprefix("#/$defs/")
    resolved = schema.get("$defs", {}).get(name)
    return resolved if isinstance(resolved, dict) else {}


def _schema_for_property(schema: dict[str, Any], property_schema: dict[str, Any]) -> dict[str, Any]:
    if "$ref" in property_schema:
        return _resolve_ref(schema, str(property_schema["$ref"]))
    return property_schema


def _collect_required_paths(
    schema: dict[str, Any],
    node: dict[str, Any],
    prefix: str = "",
) -> list[str]:
    paths: list[str] = []
    required = node.get("required", [])
    properties = node.get("properties", {})
    for field in required:
        path = f"{prefix}.{field}" if prefix else str(field)
        paths.append(path)
        child = properties.get(field)
        if isinstance(child, dict):
            child_schema = _schema_for_property(schema, child)
            if child_schema.get("type") == "object" or child_schema.get("required"):
                paths.extend(_collect_required_paths(schema, child_schema, path))
    return paths


def build_world_config_required_field_checklist() -> str:
    schema = _load_schema()
    required_paths = _collect_required_paths(schema, schema)
    lines = ["Required field checklist:"]
    lines.extend(f"- {path}" for path in required_paths)
    return "\n".join(lines)


def build_world_config_allowed_field_summary() -> str:
    schema = _load_schema()
    fields = sorted(schema.get("properties", {}).keys())
    return (
        "Allowed top-level fields only:\n"
        + "\n".join(f"- {field}" for field in fields)
        + "\nDo not add top-level fields outside this list."
    )


def build_world_config_enum_summary() -> str:
    return (
        "Enum and type constraints:\n"
        "- Coordinates use objects with numeric x, y, z fields.\n"
        "- Distances use cm, speed uses kmh, time uses sec, angles use degree.\n"
        "- Boolean fields must use true or false, not strings."
    )


def build_world_config_generation_rules() -> str:
    return (
        "Generation rules:\n"
        "- Return one JSON object only.\n"
        "- Do not output markdown code blocks or explanation text.\n"
        "- Extra keys are not allowed at the top level or inside nested objects.\n"
        "- Do not use null for required fields; choose a safe default within the scenario context.\n"
        "- Include all required nested fields.\n"
        "- Use cm/kmh/sec/degree units."
    )
