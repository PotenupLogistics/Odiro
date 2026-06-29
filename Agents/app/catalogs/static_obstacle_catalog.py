from __future__ import annotations

import json
import re
from dataclasses import dataclass
from functools import lru_cache
from pathlib import Path
from typing import Any


CATALOG_PATH = Path(__file__).with_name("static_obstacle_catalog.json")


@dataclass(frozen=True)
class StaticObstaclePropMention:
    """A static obstacle catalog mention found in normalized prompt text."""

    prop_id: str
    term: str
    start: int
    end: int


def _normalize_key(value: str) -> str:
    lowered = value.strip().lower()
    return re.sub(r"[\s\-.]+", "_", lowered)


def static_obstacle_search_text(value: str) -> str:
    """Return a prompt search key that keeps catalog aliases comparable."""
    normalized = _normalize_key(value)
    searchable = re.sub(r"[^0-9a-z_가-힣]+", "_", normalized)
    return re.sub(r"_+", "_", searchable).strip("_")


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


@lru_cache(maxsize=1)
def _search_terms() -> tuple[tuple[str, str], ...]:
    """Return normalized alias search terms mapped to canonical prop ids."""
    terms: dict[str, str] = {}
    for alias, prop_id in _alias_index().items():
        term = static_obstacle_search_text(alias)
        if len(term.replace("_", "")) < 2:
            continue
        terms[term] = prop_id
    return tuple(sorted(terms.items(), key=lambda item: len(item[0]), reverse=True))


def _term_spans(search_text: str, term: str) -> list[tuple[int, int]]:
    """Return non-empty spans for one normalized alias term."""
    if not term:
        return []
    if re.search(r"[가-힣]", term):
        spans: list[tuple[int, int]] = []
        start = search_text.find(term)
        while start >= 0:
            spans.append((start, start + len(term)))
            start = search_text.find(term, start + 1)
        return spans
    pattern = re.compile(rf"(?<![a-z0-9]){re.escape(term)}(?![a-z0-9])")
    return [match.span() for match in pattern.finditer(search_text)]


def _overlaps_selected(span: tuple[int, int], selected: list[StaticObstaclePropMention]) -> bool:
    """Return whether a candidate span overlaps a previously selected mention."""
    return any(span[0] < mention.end and mention.start < span[1] for mention in selected)


def find_static_obstacle_prop_mentions(text: str) -> list[StaticObstaclePropMention]:
    """Return catalog prop mentions in the order they appear in prompt text."""
    search_text = static_obstacle_search_text(text)
    candidates: list[StaticObstaclePropMention] = []
    for term, prop_id in _search_terms():
        for start, end in _term_spans(search_text, term):
            candidates.append(StaticObstaclePropMention(prop_id=prop_id, term=term, start=start, end=end))
    candidates.sort(key=lambda mention: (mention.start, -(mention.end - mention.start), mention.term))
    selected: list[StaticObstaclePropMention] = []
    for mention in candidates:
        if _overlaps_selected((mention.start, mention.end), selected):
            continue
        selected.append(mention)
    return selected


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
