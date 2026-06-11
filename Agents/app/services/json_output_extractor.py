from __future__ import annotations

import json
import re
from typing import Any


class JsonExtractionError(ValueError):
    def __init__(self, message: str, code: str = "json_parse_error") -> None:
        super().__init__(message)
        self.code = code
        self.message = message


def _candidate_texts(text: str) -> list[str]:
    stripped = text.strip()
    candidates = [stripped]
    code_blocks = re.findall(r"```(?:json)?\s*(.*?)```", text, flags=re.DOTALL | re.IGNORECASE)
    candidates.extend(block.strip() for block in code_blocks)

    first = stripped.find("{")
    last = stripped.rfind("}")
    if first != -1 and last != -1 and last > first:
        candidates.append(stripped[first : last + 1])
    return [candidate for candidate in candidates if candidate]


def extract_json_object(text: str) -> dict[str, Any]:
    if not text.strip():
        raise JsonExtractionError("Could not extract a JSON object: empty content.", "empty_content")

    errors: list[str] = []
    candidates = _candidate_texts(text)
    if not candidates:
        raise JsonExtractionError("Could not extract a JSON object: no JSON object candidate found.", "no_json_object_found")

    parsed_objects: list[dict[str, Any]] = []
    parse_errors: list[str] = []
    for candidate in candidates:
        try:
            parsed = json.loads(candidate)
        except json.JSONDecodeError as exc:
            errors.append(str(exc))
            continue
        if not isinstance(parsed, dict):
            raise JsonExtractionError("Extracted JSON value is not a JSON object.", "json_parse_error")
        normalized = json.dumps(parsed, sort_keys=True, ensure_ascii=False)
        if normalized not in parse_errors:
            parse_errors.append(normalized)
            parsed_objects.append(parsed)

    if len(parsed_objects) == 1:
        return parsed_objects[0]
    if len(parsed_objects) > 1:
        raise JsonExtractionError(
            "Could not extract a JSON object: multiple JSON objects were found.",
            "multiple_json_objects_ambiguous",
        )

    first = text.find("{")
    last = text.rfind("}")
    if first == -1:
        raise JsonExtractionError(
            "Could not extract a JSON object: no JSON object candidate found.",
            "no_json_object_found",
        )
    if last == -1 or last <= first:
        detail = errors[0] if errors else "unbalanced JSON object braces"
        raise JsonExtractionError(f"Could not extract a JSON object: {detail}", "json_parse_error")

    detail = errors[0] if errors else "JSON parse error"
    raise JsonExtractionError(f"Could not extract a JSON object: {detail}", "json_parse_error")
