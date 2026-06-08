from __future__ import annotations

import json
import re
from functools import lru_cache
from pathlib import Path
from typing import Any


CATALOG_PATH = Path(__file__).with_name("static_obstacle_catalog.json")


def _normalize_key(value: str) -> str:
    lowered = value.strip().lower()
    return re.sub(r"[\s\-.]+", "_", lowered)


@lru_cache(maxsize=1)
def load_static_obstacle_catalog() -> list[dict[str, Any]]:
    return json.loads(CATALOG_PATH.read_text(encoding="utf-8"))


@lru_cache(maxsize=1)
def _catalog_by_prop_id() -> dict[str, dict[str, Any]]:
    return {str(item["prop_id"]): item for item in load_static_obstacle_catalog()}


@lru_cache(maxsize=1)
def _alias_index() -> dict[str, str]:
    aliases: dict[str, str] = {}
    for item in load_static_obstacle_catalog():
        prop_id = str(item["prop_id"])
        aliases[_normalize_key(prop_id)] = prop_id
        aliases[_normalize_key(prop_id.removeprefix("obstacle."))] = prop_id
        aliases[_normalize_key(str(item["asset_name"]))] = prop_id
        for alias in item.get("aliases", []):
            aliases[_normalize_key(str(alias))] = prop_id
    return aliases


def get_allowed_static_obstacle_prop_ids() -> set[str]:
    return set(_catalog_by_prop_id())


def find_static_obstacle_by_prop_id(prop_id: str) -> dict[str, Any] | None:
    return _catalog_by_prop_id().get(prop_id)


def resolve_static_obstacle_prop_id(value: Any, *, default: str | None = None) -> str | None:
    if value is None:
        return default
    raw = str(value).strip()
    if raw in _catalog_by_prop_id():
        return raw
    normalized = _normalize_key(raw)
    if normalized in _alias_index():
        return _alias_index()[normalized]
    compact = normalized.replace("_", "")
    for alias, prop_id in _alias_index().items():
        if compact == alias.replace("_", ""):
            return prop_id
    return default


def static_obstacle_catalog_prompt_section() -> str:
    lines = [
        "Static Obstacle Catalog:",
        "- actors.static_obstacles[].prop_id must use only one of the catalog prop_id values below.",
        "- Do not invent static obstacle prop_id values outside this catalog.",
        "- asset_name must not be used as prop_id.",
        "- Prefer prop_id over asset_id for static obstacles.",
        "- If the user names an obstacle that is not a prop_id, choose the closest catalog prop_id by alias/meaning.",
        "- Use bbox metadata only to improve generation quality; do not treat it as simulation result validation.",
        "- Avoid placing large obstacles at the exact center of a narrow sidewalk unless the user intends path blocking.",
        "- road_barrier entries are preferred for path-blocking intent.",
        "- manhole entries are low ground obstacles.",
        "- road_cone entries are small avoidance obstacles.",
        "- trash_bin, bin, mailbox, fire_hydrant, and street_bank are fixed/lifestyle sidewalk obstacles.",
    ]
    for item in load_static_obstacle_catalog():
        aliases = ", ".join(str(alias) for alias in item.get("aliases", [])[:5])
        lines.append(
            "- "
            f"{item['prop_id']} | asset_name={item['asset_name']} | "
            f"bbox_cm={item['bbox_cm']} | bbox_m={item['bbox_m']} | "
            f"category={item.get('category', '')} | aliases={aliases}"
        )
    return "\n".join(lines)
