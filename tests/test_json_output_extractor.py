from __future__ import annotations

import pytest

from app.services.json_output_extractor import JsonExtractionError, extract_json_object


def test_extracts_plain_json_object() -> None:
    payload = extract_json_object('{"schemaVersion": "1.0", "worldId": "world-1"}')

    assert payload == {"schemaVersion": "1.0", "worldId": "world-1"}


def test_extracts_json_object_from_markdown_code_block() -> None:
    text = """Here is the JSON:

```json
{"schemaVersion": "1.0", "worldId": "world-1"}
```
"""

    payload = extract_json_object(text)

    assert payload["worldId"] == "world-1"


def test_invalid_json_raises_clear_error() -> None:
    with pytest.raises(JsonExtractionError) as exc_info:
        extract_json_object("not json")

    assert "JSON object" in str(exc_info.value)


def test_invalid_json_exposes_error_code() -> None:
    with pytest.raises(JsonExtractionError) as exc_info:
        extract_json_object('{"schemaVersion": ')

    assert exc_info.value.code == "json_parse_error"


def test_empty_json_content_exposes_error_code() -> None:
    with pytest.raises(JsonExtractionError) as exc_info:
        extract_json_object("")

    assert exc_info.value.code == "empty_content"
