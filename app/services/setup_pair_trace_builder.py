from __future__ import annotations

from typing import Any

from app.models.delivery_bot_setup import DeliveryBotSetup
from app.models.episode_setup import EpisodeSetup


def _trace(source: str, target: str, value_summary: str) -> dict[str, str]:
    return {
        "source": source,
        "target": target,
        "valueSummary": value_summary,
    }


def build_setup_pair_trace_items(
    world_config: dict[str, Any],
    episode_setup: EpisodeSetup,
    delivery_bot_setup: DeliveryBotSetup,
) -> list[dict[str, str]]:
    items: list[dict[str, str]] = []
    map_config = world_config.get("map") if isinstance(world_config.get("map"), dict) else {}
    robot = world_config.get("robot") if isinstance(world_config.get("robot"), dict) else {}
    spawn = robot.get("spawn") if isinstance(robot.get("spawn"), dict) else {}
    goal = robot.get("goal") if isinstance(robot.get("goal"), dict) else {}
    runtime = world_config.get("runtime") if isinstance(world_config.get("runtime"), dict) else {}
    obstacles = world_config.get("obstacles") if isinstance(world_config.get("obstacles"), list) else []

    if episode_setup.ground_model.regions:
        width_m = episode_setup.ground_model.regions[0].shape.size_m[1]
        items.append(
            _trace(
                "map.sidewalkWidthCm",
                "ground_model.regions[0].shape.size_m[1]",
                f"map.sidewalkWidthCm={map_config.get('sidewalkWidthCm')} -> ground_model.regions[0].shape.size_m[1]={width_m}",
            )
        )
    spawn_xy = episode_setup.actors.robot.xy_m
    items.append(
        _trace(
            "robot.spawn",
            "actors.robot.xy_m",
            f"robot.spawn={{x:{spawn.get('x')},y:{spawn.get('y')}}}cm -> actors.robot.xy_m=[{spawn_xy[0]},{spawn_xy[1]}]",
        )
    )
    if episode_setup.actors.robot.route is not None:
        goal_x = episode_setup.actors.robot.route.goal_xy_m[0]
        items.append(
            _trace(
                "robot.goal.x",
                "actors.robot.route.goal_xy_m[0]",
                f"robot.goal.x={goal.get('x')}cm -> actors.robot.route.goal_xy_m[0]={goal_x}",
            )
        )
    items.append(
        _trace(
            "runtime.maxDurationSec",
            "run.time_limit_s",
            f"runtime.maxDurationSec={runtime.get('maxDurationSec')} -> run.time_limit_s={episode_setup.run.time_limit_s}",
        )
    )
    if obstacles and episode_setup.actors.static_obstacles:
        position = obstacles[0].get("position") if isinstance(obstacles[0], dict) else {}
        xy = episode_setup.actors.static_obstacles[0].xy_m
        items.append(
            _trace(
                "obstacles[0].position",
                "actors.static_obstacles[0].xy_m",
                f"obstacle.position={{x:{position.get('x')},y:{position.get('y')}}}cm -> static_obstacles[0].xy_m=[{xy[0]},{xy[1]}]",
            )
        )
    items.append(
        _trace(
            "delivery_bot_setup.defaults",
            "robot.lidar.stop_distance_m",
            f"lidar.stop_distance_m={delivery_bot_setup.robot.lidar.stop_distance_m} default",
        )
    )
    return items
