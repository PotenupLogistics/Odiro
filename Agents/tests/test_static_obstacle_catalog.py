from __future__ import annotations

import re
from pathlib import Path

from app.catalogs.static_obstacle_catalog import (
    find_static_obstacle_by_prop_id,
    get_allowed_static_obstacle_prop_ids,
    load_static_obstacle_catalog,
    resolve_static_obstacle_prop_id,
)


REPO_ROOT = Path(__file__).resolve().parents[2]
CLIENT_ENVIRONMENT_CATALOG = REPO_ROOT / "Client" / "Json" / "environment-catalog.md"
BBOX_ROW_PATTERN = re.compile(
    r"^\|\s*`(?P<prop_id>obstacle\.[^`]+)`\s*\|[^|]*\|\s*`(?P<x>[0-9]+(?:\.[0-9]+)?) x "
    r"(?P<y>[0-9]+(?:\.[0-9]+)?) x (?P<z>[0-9]+(?:\.[0-9]+)?)`",
    re.MULTILINE,
)


def _client_environment_bbox_map() -> dict[str, list[float]]:
    text = CLIENT_ENVIRONMENT_CATALOG.read_text(encoding="utf-8-sig")
    return {
        match.group("prop_id"): [float(match.group("x")), float(match.group("y")), float(match.group("z"))]
        for match in BBOX_ROW_PATTERN.finditer(text)
    }


def test_static_obstacle_catalog_loads_notion_prop_ids_and_bbox_metadata() -> None:
    catalog = load_static_obstacle_catalog()

    assert len(catalog) == 16
    assert "obstacle.trash_bin" in get_allowed_static_obstacle_prop_ids()
    road_barrier = find_static_obstacle_by_prop_id("obstacle.road_barrier_01")
    assert road_barrier is not None
    assert road_barrier["asset_name"] == "SM_Road_Barrier_01"
    assert road_barrier["bbox_cm"] == [240, 70, 120]
    assert road_barrier["bbox_m"] == [2.4, 0.7, 1.2]


def test_static_obstacle_catalog_resolves_korean_and_english_aliases() -> None:
    assert resolve_static_obstacle_prop_id("안전콘") == "obstacle.road_cone_01"
    assert resolve_static_obstacle_prop_id("traffic_cone") == "obstacle.road_cone_01"
    assert resolve_static_obstacle_prop_id("쓰레기통") == "obstacle.trash_bin"
    assert resolve_static_obstacle_prop_id("barrier") == "obstacle.road_barrier_01"
    assert resolve_static_obstacle_prop_id("맨홀") == "obstacle.manhole_01"
    assert resolve_static_obstacle_prop_id("SM_FireHydrant") == "obstacle.fire_hydrant"


def test_static_obstacle_catalog_matches_client_environment_bbox_table() -> None:
    client_bbox_map = _client_environment_bbox_map()
    json_bbox_map = {
        str(item["prop_id"]): [float(value) for value in item["bbox_m"]]
        for item in load_static_obstacle_catalog()
    }

    assert client_bbox_map
    assert set(json_bbox_map) == set(client_bbox_map)
    for prop_id, bbox_m in sorted(client_bbox_map.items()):
        assert json_bbox_map[prop_id] == bbox_m
