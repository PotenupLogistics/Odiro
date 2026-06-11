from __future__ import annotations

from app.utils.json_sanitizer import remove_json_nulls


def test_remove_json_nulls_recursively_keeps_false_zero_empty_list_and_required_empty_objects() -> None:
    payload = {
        "schema": "delivery_bot_setup",
        "version": 1,
        "robot": {
            "drive": {
                "max_speed_kmh": 0,
                "draw_debug": False,
                "missing": None,
                "tags": [],
            },
            "path_follow": {},
            "lidar": {"ignore_tags": []},
        },
    }

    sanitized = remove_json_nulls(payload, keep_empty_object_keys={"drive", "path_follow", "lidar"})

    assert sanitized["robot"]["drive"]["max_speed_kmh"] == 0
    assert sanitized["robot"]["drive"]["draw_debug"] is False
    assert sanitized["robot"]["drive"]["tags"] == []
    assert "missing" not in sanitized["robot"]["drive"]
    assert sanitized["robot"]["path_follow"] == {}
    assert sanitized["robot"]["lidar"]["ignore_tags"] == []


def test_remove_json_nulls_can_drop_empty_properties_objects() -> None:
    payload = {
        "actors": {
            "robot": {"properties": {}},
            "static_obstacles": [{"properties": {"blocking_ratio": 0.6}}],
        }
    }

    sanitized = remove_json_nulls(payload, drop_empty_object_keys={"properties"})

    assert "properties" not in sanitized["actors"]["robot"]
    assert sanitized["actors"]["static_obstacles"][0]["properties"] == {"blocking_ratio": 0.6}
