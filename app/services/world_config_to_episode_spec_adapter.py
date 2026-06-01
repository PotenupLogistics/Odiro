from __future__ import annotations

from copy import deepcopy
from typing import Any

from app.models.episode_spec import (
    EpisodeActors,
    EpisodeConversionWarning,
    EpisodePath,
    EpisodeRotation,
    EpisodeRun,
    EpisodeSpec,
    EpisodeTransform,
    GroundModel,
    GroundRegion,
    GroundShape,
    PedestrianActor,
    PedestrianMovement,
    RobotActor,
    RobotRoute,
    StaticObstacleActor,
)


def _cm_to_m(value: float | int) -> float:
    return float(value) / 100.0


def _location_m(location: dict[str, Any]) -> list[float]:
    return [_cm_to_m(location.get("x", 0.0)), _cm_to_m(location.get("y", 0.0)), _cm_to_m(location.get("z", 0.0))]


def _rotation(yaw: float = 0.0) -> EpisodeRotation:
    return EpisodeRotation(yaw=float(yaw), pitch=0.0, roll=0.0)


def _transform(location: dict[str, Any], yaw: float = 0.0) -> EpisodeTransform:
    return EpisodeTransform(
        location_m=_location_m(location),
        rotation_deg=_rotation(yaw),
        scale=[1.0, 1.0, 1.0],
    )


def _ground_model(world_config: dict[str, Any]) -> GroundModel:
    map_config = world_config.get("map", {})
    return GroundModel(
        default_region_type="walkable",
        regions=[
            GroundRegion(
                region_id="sidewalk_main",
                region_type="walkable",
                shape=GroundShape(
                    type="rectangle",
                    center_m=[0.0, 0.0, 0.0],
                    size_m=[
                        _cm_to_m(map_config.get("lengthCm", 0.0)),
                        _cm_to_m(map_config.get("sidewalkWidthCm", 0.0)),
                    ],
                    yaw_deg=0.0,
                ),
                traversability_score=1.0,
            )
        ],
    )


def _robot(world_config: dict[str, Any]) -> RobotActor:
    robot = world_config.get("robot", {})
    spawn = robot.get("spawn", {})
    goal = robot.get("goal", {})
    return RobotActor(
        instance_id=robot.get("botId") or "robot_01",
        asset_id="delivery_bot",
        spawn_only=False,
        transform=_transform(spawn, spawn.get("yawDegree", 0.0)),
        route=RobotRoute(goal_m=_location_m(goal), auto_start=True),
    )


def _pedestrians(world_config: dict[str, Any]) -> tuple[list[EpisodePath], list[PedestrianActor]]:
    paths: list[EpisodePath] = []
    actors: list[PedestrianActor] = []
    for index, pedestrian in enumerate(world_config.get("pedestrians", []), start=1):
        instance_id = pedestrian.get("objectId") or f"ped_{index:02d}"
        behavior = pedestrian.get("behavior", "")
        path_id = f"{instance_id}_path"
        spawn_m = _location_m(pedestrian.get("spawn", {}))
        goal_m = _location_m(pedestrian.get("goal", {}))
        role = "pedestrian_crossing" if behavior == "Crossing" else "pedestrian_baseline"
        paths.append(
            EpisodePath(
                path_id=path_id,
                role=role,
                type="spline",
                points_m=[spawn_m, goal_m],
                closed_loop=False,
            )
        )
        properties = {"semantic_behavior": behavior} if behavior == "Crossing" else {}
        actors.append(
            PedestrianActor(
                instance_id=instance_id,
                archetype_id="adult_pedestrian",
                path_id=path_id,
                spawn_time_s=0.0,
                transform=EpisodeTransform(
                    location_m=spawn_m,
                    rotation_deg=_rotation(90.0 if behavior == "Crossing" else 0.0),
                    scale=[1.0, 1.0, 1.0],
                ),
                movement=PedestrianMovement(
                    model="straight_line",
                    speed_mps=float(pedestrian.get("speedKmh", 0.0)) / 3.6,
                    initial_distance_m=0.0,
                    auto_start=True,
                ),
                properties=properties,
            )
        )
    return paths, actors


def _prop_for_obstacle(obstacle: dict[str, Any], warnings: list[EpisodeConversionWarning], index: int) -> str:
    obstacle_type = obstacle.get("type") or "Obstacle"
    if obstacle_type == "Kickboard":
        warnings.append(
            EpisodeConversionWarning(
                code="kickboard_prop_fallback",
                message=(
                    "Kickboard mapped to obstacle.road_barrier_01 because "
                    "obstacle.kickboard is not available in UE catalog"
                ),
                sourcePath=f"obstacles[{index - 1}].type",
            )
        )
        return "obstacle.road_barrier_01"
    if obstacle_type in {"Cone", "TrafficCone"}:
        return "obstacle.road_cone_01"
    warnings.append(
        EpisodeConversionWarning(
            code="obstacle_prop_fallback",
            message=f"{obstacle_type} mapped to obstacle.box_01 because no exact UE catalog prop is configured",
            sourcePath=f"obstacles[{index - 1}].type",
        )
    )
    return "obstacle.box_01"


def _obstacles(
    world_config: dict[str, Any],
    warnings: list[EpisodeConversionWarning],
) -> list[StaticObstacleActor]:
    actors: list[StaticObstacleActor] = []
    for index, obstacle in enumerate(world_config.get("obstacles", []), start=1):
        instance_id = obstacle.get("objectId") or f"obstacle_{index:02d}"
        obstacle_type = obstacle.get("type") or "Obstacle"
        properties: dict[str, str | float | bool | list[float] | None] = {
            "blocking_ratio": float(obstacle.get("blockingRatio", 0.0)),
            "semantic_type": obstacle_type,
        }
        actors.append(
            StaticObstacleActor(
                instance_id=instance_id,
                prop_id=_prop_for_obstacle(obstacle, warnings, index),
                transform=_transform(obstacle.get("position", {}), obstacle.get("yawDegree", 0.0)),
                properties=properties,
            )
        )
    return actors


def convert_world_config_to_episode_spec_with_warnings(
    world_config: dict[str, Any],
) -> tuple[EpisodeSpec, list[EpisodeConversionWarning]]:
    source = deepcopy(world_config)
    warnings: list[EpisodeConversionWarning] = []
    paths, pedestrians = _pedestrians(source)
    static_obstacles = _obstacles(source, warnings)
    source_obstacles = source.get("obstacles", [])
    if isinstance(source_obstacles, list) and source_obstacles and not static_obstacles:
        warnings.append(
            EpisodeConversionWarning(
                code="static_obstacle_conversion_empty",
                message="WorldConfig contains obstacles but EpisodeSpec static_obstacles is empty",
                sourcePath="obstacles",
            )
        )
    episode = EpisodeSpec(
        scenario_id=source.get("scenarioId") or source.get("worldId") or "scenario_001",
        map_id="EpisodeSandbox",
        run=EpisodeRun(
            base_seed=int(source.get("seed", 0)),
            iteration_index=0,
            time_limit_s=float(source.get("runtime", {}).get("maxDurationSec", 0.0)),
        ),
        ground_model=_ground_model(source),
        paths=paths,
        actors=EpisodeActors(
            robot=_robot(source),
            pedestrians=pedestrians,
            static_obstacles=static_obstacles,
        ),
    )
    return episode, warnings


def convert_world_config_to_episode_spec(world_config: dict[str, Any]) -> EpisodeSpec:
    episode, _warnings = convert_world_config_to_episode_spec_with_warnings(world_config)
    return episode
