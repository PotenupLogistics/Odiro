from __future__ import annotations

from typing import Any

from app.services.json_output_extractor import extract_json_object


def parse_json_response(value: Any) -> dict[str, Any]:
    if isinstance(value, dict):
        return value
    if isinstance(value, str):
        return extract_json_object(value)
    raise ValueError("LLM response must be a JSON object or JSON object string.")

