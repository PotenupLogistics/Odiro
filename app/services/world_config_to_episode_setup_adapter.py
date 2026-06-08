from __future__ import annotations

from copy import deepcopy
from typing import Any

from app.catalogs.static_obstacle_catalog import resolve_static_obstacle_prop_id
from app.models.episode_setup import (
    EpisodeActors,
    EpisodePath,
    EpisodePedestrian,
    EpisodeRobot,
    EpisodeRobotRoute,
    EpisodeRunConfig,
    EpisodeSetup,
    EpisodeStaticObstacle,
    GroundModel,
    GroundRegion,
    PedestrianMovement,
    RegionShape,
)


START_GOAL_MARGIN_M = 2.0
MIN_SIDEWALK_WIDTH_M = 1.5
EXPLICIT_EXTREME_SIDEWALK_WIDTH_M = 1.0


def _cm_to_m(value: Any) -> float:
    return float(value or 0.0) / 100.0


def _xy_m(location: dict[str, Any] | None) -> list[float]:
    source = location if isinstance(location, dict) else {}
    return [_cm_to_m(source.get("x", 0.0)), _cm_to_m(source.get("y", 0.0))]


def _fixed_constraints(world_config: dict[str, Any]) -> dict[str, Any]:
    semantic_constraints = world_config.get("semanticFixedConstraints")
    if isinstance(semantic_constraints, dict):
        return semantic_constraints
    environment_sampling = world_config.get("environmentSampling")
    if isinstance(environment_sampling, dict):
        environment_constraints = environment_sampling.get("semanticFixedConstraints")
        if isinstance(environment_constraints, dict):
            return environment_constraints
        fixed_parameters = environment_sampling.get("fixedParameters")
        if isinstance(fixed_parameters, dict):
            return fixed_parameters
    constraints = world_config.get("constraints")
    if isinstance(constraints, dict):
        semantic_constraints = constraints.get("semanticFixedConstraints")
        if isinstance(semantic_constraints, dict):
            return semantic_constraints
    return {}


def _sidewalk_width_m(world_config: dict[str, Any], raw_width_m: float) -> float:
    fixed_width_cm = _fixed_constraints(world_config).get("sidewalkWidthCm")
    if isinstance(fixed_width_cm, int | float) and not isinstance(fixed_width_cm, bool):
        fixed_width_m = float(fixed_width_cm) / 100.0
        if fixed_width_m <= EXPLICIT_EXTREME_SIDEWALK_WIDTH_M:
            return fixed_width_m
    return max(raw_width_m, MIN_SIDEWALK_WIDTH_M)


def _ground_model(world_config: dict[str, Any]) -> GroundModel:
    map_config = world_config.get("map") if isinstance(world_config.get("map"), dict) else {}
    robot = world_config.get("robot") if isinstance(world_config.get("robot"), dict) else {}
    start_xy_m = _xy_m(robot.get("spawn"))
    goal_xy_m = _xy_m(robot.get("goal"))
    fallback_length_m = _cm_to_m(map_config.get("lengthCm", 0.0))
    if goal_xy_m == [0.0, 0.0] and fallback_length_m > 0:
        goal_xy_m = [fallback_length_m, start_xy_m[1]]
    x_min = min(start_xy_m[0], goal_xy_m[0]) - START_GOAL_MARGIN_M
    x_max = max(start_xy_m[0], goal_xy_m[0]) + START_GOAL_MARGIN_M
    center_y = (start_xy_m[1] + goal_xy_m[1]) / 2.0
    width_m = _sidewalk_width_m(world_config, _cm_to_m(map_config.get("sidewalkWidthCm", 0.0)))
    return GroundModel(
        default_region_type="blocked",
        regions=[
            GroundRegion(
                region_id="sidewalk_main",
                region_type="walkable",
                shape=RegionShape(
                    type="rectangle",
                    center_xy_m=[(x_min + x_max) / 2.0, center_y],
                    size_m=[x_max - x_min, width_m],
                    yaw_deg=0.0,
                ),
                traversability_score=1.0,
            )
        ],
    )


def _robot(world_config: dict[str, Any]) -> EpisodeRobot:
    robot = world_config.get("robot") if isinstance(world_config.get("robot"), dict) else {}
    return EpisodeRobot(
        instance_id=robot.get("botId") or "robot_01",
        asset_id="delivery_bot",
        spawn_only=False,
        xy_m=_xy_m(robot.get("spawn")),
        yaw_deg=float((robot.get("spawn") or {}).get("yawDegree", 0.0)) if isinstance(robot.get("spawn"), dict) else 0.0,
        route=EpisodeRobotRoute(
            goal_xy_m=_xy_m(robot.get("goal")),
            auto_start=True,
        ),
    )


def _obstacles(world_config: dict[str, Any]) -> list[EpisodeStaticObstacle]:
    obstacles = world_config.get("obstacles") if isinstance(world_config.get("obstacles"), list) else []
    actors: list[EpisodeStaticObstacle] = []
    instance_counts: dict[str, int] = {}
    for index, obstacle in enumerate(obstacles, start=1):
        if not isinstance(obstacle, dict):
            continue
        obstacle_type = obstacle.get("type") or "Obstacle"
        prop_id = (
            resolve_static_obstacle_prop_id(obstacle.get("prop_id"))
            or resolve_static_obstacle_prop_id(obstacle.get("asset_name"))
            or resolve_static_obstacle_prop_id(obstacle.get("asset_id"))
            or resolve_static_obstacle_prop_id(obstacle_type)
            or ("obstacle.road_barrier_01" if obstacle_type == "Kickboard" else "obstacle.box_01")
        )
        base_instance_id = prop_id.removeprefix("obstacle.")
        instance_counts[base_instance_id] = instance_counts.get(base_instance_id, 0) + 1
        instance_id = obstacle.get("objectId") or f"{base_instance_id}_{instance_counts[base_instance_id]:02d}"
        actors.append(
            EpisodeStaticObstacle(
                instance_id=instance_id,
                prop_id=prop_id,
                xy_m=_xy_m(obstacle.get("position")),
                yaw_deg=float(obstacle.get("yawDegree", 0.0)),
                properties={
                    "blocking_ratio": float(obstacle.get("blockingRatio", 0.0)),
                    "semantic_type": "Obstacle",
                },
            )
        )
    return actors


def _pedestrians(world_config: dict[str, Any]) -> tuple[list[EpisodePath], list[EpisodePedestrian]]:
    pedestrians = world_config.get("pedestrians") if isinstance(world_config.get("pedestrians"), list) else []
    paths: list[EpisodePath] = []
    actors: list[EpisodePedestrian] = []
    for index, pedestrian in enumerate(pedestrians, start=1):
        if not isinstance(pedestrian, dict):
            continue
        instance_id = pedestrian.get("objectId") or f"ped_{index:02d}"
        path_id = f"{instance_id}_path"
        spawn_xy = _xy_m(pedestrian.get("spawn"))
        goal_xy = _xy_m(pedestrian.get("goal"))
        paths.append(
            EpisodePath(
                path_id=path_id,
                points_xy_m=[spawn_xy, goal_xy],
                closed_loop=False,
            )
        )
        actors.append(
            EpisodePedestrian(
                instance_id=instance_id,
                archetype_id="adult_pedestrian",
                path_id=path_id,
                spawn_time_s=float(pedestrian.get("spawn_time_s", 0.0)),
                xy_m=spawn_xy,
                yaw_deg=float(pedestrian.get("yawDegree", 0.0)),
                movement=PedestrianMovement(
                    model="straight_line",
                    speed_mps=float(pedestrian.get("speedKmh", 3.6)) / 3.6,
                    initial_distance_m=0.0,
                    auto_start=True,
                ),
                properties={"semantic_behavior": pedestrian.get("behavior")} if pedestrian.get("behavior") else {},
            )
        )
    return paths, actors


def convert_world_config_to_episode_setup(world_config: dict[str, Any]) -> EpisodeSetup:
    source = deepcopy(world_config)
    paths, pedestrians = _pedestrians(source)
    runtime = source.get("runtime") if isinstance(source.get("runtime"), dict) else {}
    run = source.get("run") if isinstance(source.get("run"), dict) else {}
    return EpisodeSetup(
        scenario_id=source.get("scenarioId") or source.get("worldId") or "scenario_001",
        map_id="EpisodeSandbox",
        run=EpisodeRunConfig(
            base_seed=int(source.get("seed", 0)),
            iteration_index=int(run.get("iteration_index", 0)),
            time_limit_s=float(runtime.get("maxDurationSec", 60.0)),
        ),
        ground_model=_ground_model(source),
        paths=paths,
        actors=EpisodeActors(
            robot=_robot(source),
            static_obstacles=_obstacles(source),
            pedestrians=pedestrians,
        ),
    )
