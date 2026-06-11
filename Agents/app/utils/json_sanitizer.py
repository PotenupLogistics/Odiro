from __future__ import annotations

from dataclasses import asdict, is_dataclass
from typing import Any

from pydantic import BaseModel


DEFAULT_KEEP_EMPTY_OBJECT_KEYS = {
    "run",
    "evaluation",
    "ground_model",
    "actors",
    "robot",
    "drive",
    "path_follow",
    "lidar",
}


def remove_json_nulls(
    value: Any,
    *,
    keep_empty_object_keys: set[str] | None = None,
    drop_empty_object_keys: set[str] | None = None,
    current_key: str | None = None,
) -> Any:
    keep_empty = DEFAULT_KEEP_EMPTY_OBJECT_KEYS | (keep_empty_object_keys or set())
    drop_empty = drop_empty_object_keys or set()
    if isinstance(value, BaseModel):
        value = value.model_dump(mode="json", by_alias=True)
    elif is_dataclass(value) and not isinstance(value, type):
        value = asdict(value)

    if isinstance(value, dict):
        sanitized: dict[str, Any] = {}
        for key, item in value.items():
            if item is None:
                continue
            child = remove_json_nulls(
                item,
                keep_empty_object_keys=keep_empty,
                drop_empty_object_keys=drop_empty,
                current_key=str(key),
            )
            if child == {} and str(key) in drop_empty:
                continue
            sanitized[str(key)] = child
        if sanitized == {} and current_key and current_key not in keep_empty:
            return {}
        return sanitized
    if isinstance(value, list):
        return [
            remove_json_nulls(
                item,
                keep_empty_object_keys=keep_empty,
                drop_empty_object_keys=drop_empty,
                current_key=current_key,
            )
            for item in value
        ]
    return value


def contains_json_null(value: Any) -> bool:
    if value is None:
        return True
    if isinstance(value, dict):
        return any(contains_json_null(item) for item in value.values())
    if isinstance(value, list):
        return any(contains_json_null(item) for item in value)
    return False
