from __future__ import annotations

from app.catalogs.static_obstacle_catalog import (
    find_static_obstacle_by_prop_id,
    get_allowed_static_obstacle_prop_ids,
    load_static_obstacle_catalog,
    resolve_static_obstacle_prop_id,
)


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
