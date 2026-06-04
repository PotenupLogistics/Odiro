from __future__ import annotations

from app.services.setup_pair_trace_builder import build_setup_pair_trace_items
from app.services.world_config_to_delivery_bot_setup_adapter import convert_world_config_to_delivery_bot_setup
from app.services.world_config_to_episode_setup_adapter import convert_world_config_to_episode_setup


def test_setup_pair_trace_builder_summarizes_episode_and_delivery_bot_sources() -> None:
    world_config = {
        "scenarioId": "obstacle_ahead",
        "seed": 1001,
        "map": {"lengthCm": 800, "sidewalkWidthCm": 120},
        "robot": {"spawn": {"x": 0, "y": 0}, "goal": {"x": 800, "y": 0}},
        "obstacles": [{"type": "Obstacle", "position": {"x": 400, "y": -40}, "blockingRatio": 0.6}],
        "pedestrians": [],
        "runtime": {"maxDurationSec": 60},
    }
    episode_setup = convert_world_config_to_episode_setup(world_config)
    delivery_bot_setup = convert_world_config_to_delivery_bot_setup(world_config)

    items = build_setup_pair_trace_items(world_config, episode_setup, delivery_bot_setup)
    text = "\n".join(item["valueSummary"] for item in items)

    assert "map.sidewalkWidthCm=120 -> ground_model.regions[0].shape.size_m[1]=1.2" in text
    assert "robot.goal.x=800cm -> actors.robot.route.goal_xy_m[0]=8.0" in text
    assert "obstacle.position={x:400,y:-40}cm -> static_obstacles[0].xy_m=[4.0,-0.4]" in text
    assert "lidar.stop_distance_m=1.2 default" in text
