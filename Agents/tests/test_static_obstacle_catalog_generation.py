from __future__ import annotations

import pytest

from app.services.episode_setup_validator import validate_episode_setup
from app.services.world_config_scenario_intent_extractor import extract_scenario_intent
from app.services.world_config_scenario_post_processor import apply_scenario_intent_to_world_config
from app.services.world_config_prompt_builder import build_world_config_prompt_package
from app.services.setup_pair_queue_generator import generate_setup_pair_queue
from app.services.world_config_to_episode_setup_adapter import convert_world_config_to_episode_setup

from tests.test_world_config_prompt_builder import _request
from tests.test_world_config_to_episode_setup_adapter import _world_config


def _episode_prop_id_for_obstacle_type(obstacle_type: str) -> str:
    world_config = _world_config()
    world_config["obstacles"] = [
        {
            "objectId": "legacy_obstacle_01",
            "type": obstacle_type,
            "position": {"x": 400, "y": 0, "z": 0},
            "blockingRatio": 0.6,
        }
    ]
    episode = convert_world_config_to_episode_setup(world_config)
    assert validate_episode_setup(episode).valid is True
    return episode.actors.static_obstacles[0].prop_id


def test_prompt_builder_includes_static_obstacle_catalog_rules() -> None:
    package = build_world_config_prompt_package(_request(), compact_prompt=True)

    prompt_text = package.systemPrompt + package.userPrompt
    assert "Static Obstacle Catalog" in prompt_text
    assert "actors.static_obstacles[].prop_id" in prompt_text
    assert "Do not invent static obstacle prop_id values outside this catalog." in prompt_text
    assert "asset_name must not be used as prop_id" in prompt_text
    assert "obstacle.road_cone_01" in prompt_text
    assert "obstacle.trash_bin" in prompt_text


def test_episode_setup_adapter_normalizes_common_obstacle_aliases_to_catalog_prop_ids() -> None:
    assert _episode_prop_id_for_obstacle_type("traffic_cone") == "obstacle.road_cone_01"
    assert _episode_prop_id_for_obstacle_type("trash_can") == "obstacle.trash_bin"
    assert _episode_prop_id_for_obstacle_type("barrier") == "obstacle.road_barrier_01"
    assert _episode_prop_id_for_obstacle_type("box") == "obstacle.box_01"
    assert _episode_prop_id_for_obstacle_type("manhole") == "obstacle.manhole_01"


def test_episode_setup_adapter_prefers_catalog_prop_id_and_generates_stable_instance_id() -> None:
    world_config = _world_config()
    world_config["obstacles"] = [
        {
            "type": "SM_Trash_Bin",
            "asset_name": "SM_Trash_Bin",
            "position": {"x": 300, "y": 0, "z": 0},
            "blockingRatio": 0.3,
        },
        {
            "type": "SM_Trash_Bin",
            "asset_name": "SM_Trash_Bin",
            "position": {"x": 500, "y": 0, "z": 0},
            "blockingRatio": 0.3,
        },
    ]

    episode = convert_world_config_to_episode_setup(world_config)

    static_obstacles = episode.actors.static_obstacles
    assert [item.prop_id for item in static_obstacles] == ["obstacle.trash_bin", "obstacle.trash_bin"]
    assert [item.instance_id for item in static_obstacles] == ["trash_bin_01", "trash_bin_02"]
    assert validate_episode_setup(episode).valid is True


def test_korean_catalog_terms_are_extracted_and_post_processed_into_world_config_obstacle_type() -> None:
    intent = extract_scenario_intent("보도 위에 안전콘이 놓여 있는 상황")

    assert intent.obstacleType == "traffic_cone"
    assert "Obstacle" in intent.obstacleHints

    world_config = _world_config()
    world_config["obstacles"] = []
    result = apply_scenario_intent_to_world_config("보도 위에 안전콘이 놓여 있는 상황", world_config)

    assert result.patchedPayload["obstacles"][0]["type"] == "traffic_cone"
    episode = convert_world_config_to_episode_setup(result.patchedPayload)
    assert episode.actors.static_obstacles[0].prop_id == "obstacle.road_cone_01"


@pytest.mark.parametrize(
    ("prompt", "expected_prop_id"),
    [
        ("좁은 보도에 쓰레기통이 로봇 앞을 일부 막고 있는 상황", "obstacle.trash_bin"),
        ("보도 위에 안전콘이 놓여 있는 상황", "obstacle.road_cone_01"),
        ("경로를 바리케이드가 막고 있는 상황", "obstacle.road_barrier_01"),
        ("좁은 보도에 상자가 놓여 있는 상황", "obstacle.box_01"),
        ("맨홀이 있는 보도 상황", "obstacle.manhole_01"),
    ],
)
def test_requested_prompt_cases_resolve_to_catalog_prop_ids(prompt: str, expected_prop_id: str) -> None:
    world_config = _world_config()
    world_config["obstacles"] = []

    result = apply_scenario_intent_to_world_config(prompt, world_config)
    episode = convert_world_config_to_episode_setup(result.patchedPayload)

    assert episode.actors.static_obstacles[0].prop_id == expected_prop_id
    assert validate_episode_setup(episode).valid is True


def test_multiple_named_obstacles_are_preserved_through_post_processing() -> None:
    world_config = _world_config()
    world_config["obstacles"] = []

    result = apply_scenario_intent_to_world_config(
        "좁은 보도에서 안전콘과 상자형 장애물이 놓여 있는 상황",
        world_config,
    )
    episode = convert_world_config_to_episode_setup(result.patchedPayload)

    prop_ids = [item.prop_id for item in episode.actors.static_obstacles]
    assert "obstacle.road_cone_01" in prop_ids
    assert "obstacle.box_01" in prop_ids


def test_run_queue_variants_preserve_named_obstacle_type_constraints() -> None:
    world_config = _world_config()
    world_config["semanticFixedConstraints"] = {
        "sidewalkWidthCm": 120,
        "obstacleType": "trash_bin",
        "pedestrianCount": 0,
    }
    world_config["environmentSampling"] = {
        "enabled": True,
        "scenarioType": "obstacle_ahead",
        "fixedParameters": {},
        "semanticFixedConstraints": world_config["semanticFixedConstraints"],
    }

    queue = generate_setup_pair_queue(world_config, episode_count=2)

    for item in queue.items:
        prop_ids = [obstacle.prop_id for obstacle in item.episode_setup.actors.static_obstacles]
        assert "obstacle.trash_bin" in prop_ids or "obstacle.bin" in prop_ids
        assert prop_ids != ["obstacle.box_01"]
        assert item.episode_setup.actors.pedestrians == []


def test_run_queue_variants_preserve_multiple_named_obstacles_and_crossing_pedestrians() -> None:
    world_config = _world_config()
    world_config["semanticFixedConstraints"] = {
        "sidewalkWidthCm": 120,
        "obstacleTypes": ["traffic_cone", "box"],
        "obstacleCount": 2,
        "pedestrianCount": 1,
        "pedestrianDirection": "crossing",
    }
    world_config["environmentSampling"] = {
        "enabled": True,
        "scenarioType": "obstacle_ahead",
        "fixedParameters": {},
        "semanticFixedConstraints": world_config["semanticFixedConstraints"],
    }

    queue = generate_setup_pair_queue(world_config, episode_count=2)

    for item in queue.items:
        prop_ids = [obstacle.prop_id for obstacle in item.episode_setup.actors.static_obstacles]
        assert any(prop_id.startswith("obstacle.road_cone") for prop_id in prop_ids)
        assert any(prop_id.startswith("obstacle.box") for prop_id in prop_ids)
        assert len(item.episode_setup.actors.pedestrians) >= 1
        assert item.episode_setup.paths
