from __future__ import annotations

import json
from dataclasses import asdict, is_dataclass
from datetime import date, datetime
from enum import Enum
from pathlib import Path
from typing import Any

from pydantic import BaseModel


SECRET_KEY_PARTS = {
    "openai_api_key",
    "apikey",
    "api_key",
    "authorization",
    "token",
    "secret",
}
MASKED_VALUE = "***MASKED***"


def _is_secret_key(key: object) -> bool:
    normalized = str(key).replace("-", "_").lower()
    return any(part in normalized for part in SECRET_KEY_PARTS)


def _append_warning(warnings: list[str], message: str) -> None:
    if message not in warnings:
        warnings.append(message)


def _to_jsonable(value: Any, warnings: list[str]) -> Any:
    if value is None or isinstance(value, str | int | float | bool):
        return value
    if isinstance(value, BaseModel):
        return _to_jsonable(value.model_dump(mode="json"), warnings)
    if is_dataclass(value) and not isinstance(value, type):
        return _to_jsonable(asdict(value), warnings)
    if isinstance(value, Enum):
        return _to_jsonable(value.value, warnings)
    if isinstance(value, datetime | date):
        return value.isoformat()
    if isinstance(value, Path):
        return str(value)
    if isinstance(value, list | tuple | set):
        return [_to_jsonable(item, warnings) for item in value]
    if isinstance(value, dict):
        converted: dict[str, Any] = {}
        for key, item in value.items():
            key_text = str(key)
            converted[key_text] = MASKED_VALUE if _is_secret_key(key) else _to_jsonable(item, warnings)
        return converted
    _append_warning(warnings, f"Converted unsupported report value to string: {type(value).__name__}")
    return str(value)


def to_jsonable(value: Any) -> Any:
    warnings: list[str] = []
    converted = _to_jsonable(value, warnings)
    if warnings and isinstance(converted, dict):
        existing = converted.get("serializationWarnings")
        if isinstance(existing, list):
            converted["serializationWarnings"] = existing + warnings
        else:
            converted["serializationWarnings"] = warnings
    return converted


def write_json_report(path: str | Path, data: dict[str, Any]) -> None:
    report_path = Path(path)
    report_path.parent.mkdir(parents=True, exist_ok=True)
    report_path.write_text(
        json.dumps(to_jsonable(data), ensure_ascii=False, indent=2) + "\n",
        encoding="utf-8",
    )
