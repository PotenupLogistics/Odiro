from __future__ import annotations

from copy import deepcopy
from dataclasses import dataclass, field
from typing import Any, Literal

from app.core.settings import Settings
from app.models.environment import EnvironmentParameterSet, EnvironmentSamplingRequest
from app.services.delivery_bot_setup_variation_policy import delivery_bot_tuning_for_episode
from app.services.environment_parameter_catalog import get_allowed_values
from app.services.environment_parameter_sampler import sample_environment_parameters


SCENARIO_TYPES = {
    "narrow_sidewalk_kickboard_crossing",
    "obstacle_ahead",
    "pedestrian_crossing",
    "terrain_risk",
    "generic_sidewalk",
}

ComparisonMode = Literal["scenario_variation", "policy_comparison"]


@dataclass(frozen=True)
class EpisodeVariant:
    world_config: dict[str, Any]
    changed_parameters: list[str] = field(default_factory=list)


def _fixed_parameters(world_config: dict[str, Any]) -> dict[str, Any]:
    environment_sampling = world_config.get("environmentSampling")
    semantic_constraints = world_config.get("semanticFixedConstraints")
    if not isinstance(environment_sampling, dict):
        constraints = world_config.get("constraints")
        environment_sampling = constraints.get("environmentSampling") if isinstance(constraints, dict) else {}
        if not isinstance(semantic_constraints, dict) and isinstance(constraints, dict):
            semantic_constraints = constraints.get("semanticFixedConstraints")
    fixed = environment_sampling.get("fixedParameters") if isinstance(environment_sampling, dict) else {}
    environment_semantic = (
        environment_sampling.get("semanticFixedConstraints") if isinstance(environment_sampling, dict) else None
    )
    merged: dict[str, Any] = {}
    if isinstance(fixed, dict):
        merged.update(fixed)
    if isinstance(environment_semantic, dict):
        merged.update(environment_semantic)
    if isinstance(semantic_constraints, dict):
        merged.update(semantic_constraints)
    return merged


def _sampler_fixed_parameters(fixed: dict[str, Any]) -> dict[str, int | float]:
    sampler_fixed: dict[str, int | float] = {}
    sampler_candidate_keys = {
        "sidewalkWidthCm",
        "slopeDegree",
        "curbHeightCm",
        "timeLimitSec",
        "obstacleBlockingRatio",
        "obstacleLateralOffsetM",
    }
    for key, value in fixed.items():
        if key not in sampler_candidate_keys:
            continue
        if not isinstance(value, int | float) or isinstance(value, bool):
            continue
        try:
            allowed_values = get_allowed_values(key)
        except KeyError:
            continue
        if value in allowed_values:
            sampler_fixed[key] = value
    return sampler_fixed


def _set_runtime_iteration(config: dict[str, Any], index: int) -> None:
    run = config.get("run") if isinstance(config.get("run"), dict) else {}
    run["iteration_index"] = index
    config["run"] = run


def _set_seed(config: dict[str, Any], seed: int) -> None:
    config["seed"] = seed


def _scenario_type(world_config: dict[str, Any]) -> str:
    environment_sampling = world_config.get("environmentSampling")
    if not isinstance(environment_sampling, dict):
        constraints = world_config.get("constraints")
        environment_sampling = constraints.get("environmentSampling") if isinstance(constraints, dict) else {}
    candidate = environment_sampling.get("scenarioType") if isinstance(environment_sampling, dict) else None
    if candidate in SCENARIO_TYPES:
        return str(candidate)
    scenario_id = str(world_config.get("scenarioId", ""))
    return scenario_id if scenario_id in SCENARIO_TYPES else "generic_sidewalk"


def _environment_sampling(config: dict[str, Any]) -> dict[str, Any]:
    environment_sampling = config.get("environmentSampling")
    if not isinstance(environment_sampling, dict):
        environment_sampling = {}
        config["environmentSampling"] = environment_sampling
    return environment_sampling


def _set_if_changed(path: str, before: Any, after: Any, changes: list[str]) -> None:
    if before != after:
        changes.append(path)


def _sample_environment(
    request_id: str,
    seed: int,
    scenario_type: str,
    fixed: dict[str, Any],
) -> EnvironmentParameterSet:
    sampler_fixed = _sampler_fixed_parameters(fixed)
    result = sample_environment_parameters(
        EnvironmentSamplingRequest(
            requestId=request_id,
            seed=seed,
            scenarioType=scenario_type,  # type: ignore[arg-type]
            fixedParameters=sampler_fixed,
            includeLabels=False,
        )
    )
    return result.parameters


def _apply_environment_parameters(
    config: dict[str, Any],
    parameters: EnvironmentParameterSet,
    fixed: dict[str, Any],
    changes: list[str],
) -> None:
    map_config = config.get("map") if isinstance(config.get("map"), dict) else {}
    config["map"] = map_config
    if "sidewalkWidthCm" not in fixed:
        before = map_config.get("sidewalkWidthCm")
        map_config["sidewalkWidthCm"] = parameters.sidewalkWidthCm
        _set_if_changed("map.sidewalkWidthCm", before, parameters.sidewalkWidthCm, changes)
    if "slopeDegree" not in fixed:
        before = map_config.get("slopeDegree")
        map_config["slopeDegree"] = parameters.slopeDegree
        _set_if_changed("map.slopeDegree", before, parameters.slopeDegree, changes)
    if "curbHeightCm" not in fixed:
        before = map_config.get("curbHeightCm")
        map_config["curbHeightCm"] = parameters.curbHeightCm
        _set_if_changed("map.curbHeightCm", before, parameters.curbHeightCm, changes)

    runtime = config.get("runtime") if isinstance(config.get("runtime"), dict) else {}
    config["runtime"] = runtime
    if "timeLimitSec" not in fixed:
        before = runtime.get("maxDurationSec")
        runtime["maxDurationSec"] = parameters.timeLimitSec
        _set_if_changed("runtime.maxDurationSec", before, parameters.timeLimitSec, changes)

    obstacles = config.get("obstacles") if isinstance(config.get("obstacles"), list) else []
    if obstacles and isinstance(obstacles[0], dict):
        obstacle = obstacles[0]
        if "obstacleBlockingRatio" not in fixed:
            before = obstacle.get("blockingRatio")
            obstacle["blockingRatio"] = parameters.obstacleBlockingRatio
            _set_if_changed("obstacles[0].blockingRatio", before, parameters.obstacleBlockingRatio, changes)
        if "obstacleLateralOffsetM" not in fixed:
            position = obstacle.get("position") if isinstance(obstacle.get("position"), dict) else {}
            obstacle["position"] = position
            before = position.get("y")
            offset_cm = round(parameters.obstacleLateralOffsetM * 100.0, 6)
            position["y"] = offset_cm
            _set_if_changed("obstacles[0].position.y", before, offset_cm, changes)

    pedestrians = config.get("pedestrians") if isinstance(config.get("pedestrians"), list) else []
    if pedestrians:
        target_speed_kmh = round(parameters.pedestrianSpeedMps * 3.6, 3)
        for index, pedestrian in enumerate(pedestrians[: parameters.pedestrianCount]):
            if not isinstance(pedestrian, dict):
                continue
            before = pedestrian.get("speedKmh")
            pedestrian["speedKmh"] = target_speed_kmh
            _set_if_changed(f"pedestrians[{index}].speedKmh", before, target_speed_kmh, changes)


def _map_length_cm(config: dict[str, Any]) -> float:
    map_config = config.get("map") if isinstance(config.get("map"), dict) else {}
    return float(map_config.get("lengthCm", 800) or 800)


def _sidewalk_width_cm(config: dict[str, Any], fixed: dict[str, Any]) -> float:
    if fixed.get("sidewalkWidthCm") is not None:
        return float(fixed["sidewalkWidthCm"])
    map_config = config.get("map") if isinstance(config.get("map"), dict) else {}
    return float(map_config.get("sidewalkWidthCm", 120) or 120)


def _location(config: dict[str, Any], key: str) -> dict[str, Any]:
    robot = config.get("robot") if isinstance(config.get("robot"), dict) else {}
    location = robot.get(key) if isinstance(robot.get(key), dict) else {}
    return location


def _clamp(value: float, lower: float, upper: float) -> float:
    return max(lower, min(upper, value))


def _obstacle(
    index: int,
    *,
    x_cm: float,
    y_cm: float,
    blocking_ratio: float,
    obstacle_type: str = "Obstacle",
) -> dict[str, Any]:
    return {
        "objectId": f"obstacle_{index + 1:02d}",
        "type": obstacle_type,
        "position": {"x": round(x_cm, 3), "y": round(y_cm, 3), "z": 0},
        "blockingRatio": blocking_ratio,
    }


def _fixed_float_list(value: Any) -> list[float]:
    if not isinstance(value, list):
        return []
    floats: list[float] = []
    for item in value:
        if isinstance(item, int | float) and not isinstance(item, bool):
            floats.append(float(item))
    return floats


def _has_explicit_actor_constraints(fixed: dict[str, Any]) -> bool:
    return any(
        key in fixed
        for key in (
            "goalDistanceM",
            "obstacleCount",
            "obstacleType",
            "obstacleTypes",
            "obstaclePositionsFromStartM",
            "obstacleLateralPosition",
            "pedestrianCount",
            "pedestrianDirection",
        )
    )


def _pedestrian_y_offsets(count: int, half_width_cm: float, index: int) -> list[float]:
    usable_half_width = max(0.0, half_width_cm - 5.0)
    if count <= 1:
        return [0.0]
    if usable_half_width <= 0:
        return [0.0 for _ in range(count)]
    step = (usable_half_width * 2.0) / max(1, count - 1)
    base = [-usable_half_width + step * item for item in range(count)]
    shift = ((index % 3) - 1) * min(5.0, usable_half_width / 4.0)
    return [round(_clamp(value + shift, -usable_half_width, usable_half_width), 3) for value in base]


def _apply_fixed_scenario_constraints(
    config: dict[str, Any],
    index: int,
    fixed: dict[str, Any],
    changes: list[str],
) -> None:
    if not _has_explicit_actor_constraints(fixed):
        return

    map_config = config.get("map") if isinstance(config.get("map"), dict) else {}
    config["map"] = map_config
    sidewalk_width_cm = _sidewalk_width_cm(config, fixed)
    map_config["sidewalkWidthCm"] = sidewalk_width_cm
    half_width_cm = sidewalk_width_cm / 2.0

    robot = config.get("robot") if isinstance(config.get("robot"), dict) else {}
    config["robot"] = robot
    spawn = robot.get("spawn") if isinstance(robot.get("spawn"), dict) else {"x": 0, "y": 0, "z": 0}
    robot["spawn"] = spawn
    start_x_cm = float(spawn.get("x", 0.0) or 0.0)
    start_y_cm = float(spawn.get("y", 0.0) or 0.0)

    max_required_x_cm = _map_length_cm(config)
    if fixed.get("goalDistanceM") is not None:
        goal_x_cm = start_x_cm + float(fixed["goalDistanceM"]) * 100.0
        goal = robot.get("goal") if isinstance(robot.get("goal"), dict) else {}
        before_goal = dict(goal)
        goal.update({"x": round(goal_x_cm, 3), "y": round(start_y_cm, 3), "z": 0})
        robot["goal"] = goal
        max_required_x_cm = max(max_required_x_cm, goal_x_cm)
        _set_if_changed("robot.goal", before_goal, dict(goal), changes)

    positions_m = _fixed_float_list(fixed.get("obstaclePositionsFromStartM"))
    for position_m in positions_m:
        max_required_x_cm = max(max_required_x_cm, start_x_cm + position_m * 100.0)
    before_length = map_config.get("lengthCm")
    map_config["lengthCm"] = max(float(map_config.get("lengthCm", 0.0) or 0.0), max_required_x_cm)
    _set_if_changed("map.lengthCm", before_length, map_config["lengthCm"], changes)

    if any(key in fixed for key in ("obstacleCount", "obstacleType", "obstacleTypes", "obstaclePositionsFromStartM", "obstacleLateralPosition")):
        existing_obstacles = config.get("obstacles") if isinstance(config.get("obstacles"), list) else []
        obstacle_types = [str(item).lower() for item in fixed.get("obstacleTypes", [])] if isinstance(fixed.get("obstacleTypes"), list) else []
        fallback_count = len(existing_obstacles) if existing_obstacles else max(1, len(positions_m))
        obstacle_count = len(obstacle_types) if obstacle_types else (
            int(fixed["obstacleCount"]) if fixed.get("obstacleCount") is not None else fallback_count
        )
        single_obstacle_type = str(fixed.get("obstacleType", "") or "").lower()
        y_cm = 0.0 if fixed.get("obstacleLateralPosition") == "center" else start_y_cm
        blocking_ratio = float(fixed.get("obstacleBlockingRatio", 0.6) or 0.6)
        obstacles: list[dict[str, Any]] = []
        for obstacle_index in range(obstacle_count):
            obstacle_type = obstacle_types[obstacle_index] if obstacle_index < len(obstacle_types) else single_obstacle_type
            world_obstacle_type = "Kickboard" if obstacle_type == "kickboard" else "Obstacle"
            if obstacle_index < len(positions_m):
                x_cm = start_x_cm + positions_m[obstacle_index] * 100.0
            elif obstacle_count == 1:
                x_cm = start_x_cm + max_required_x_cm / 2.0
            else:
                spacing = max_required_x_cm / (obstacle_count + 1)
                x_cm = start_x_cm + spacing * (obstacle_index + 1)
            obstacle = _obstacle(
                obstacle_index,
                x_cm=x_cm,
                y_cm=y_cm,
                blocking_ratio=blocking_ratio,
                obstacle_type=world_obstacle_type,
            )
            obstacle["yawDegree"] = round(((index % 3) - 1) * 2.0, 3)
            obstacles.append(obstacle)
        before_obstacles = config.get("obstacles")
        config["obstacles"] = obstacles
        _set_if_changed("obstacles", before_obstacles, obstacles, changes)

    if any(key in fixed for key in ("pedestrianCount", "pedestrianDirection")):
        existing_pedestrians = config.get("pedestrians") if isinstance(config.get("pedestrians"), list) else []
        fallback_count = len(existing_pedestrians)
        pedestrian_count = int(fixed["pedestrianCount"]) if fixed.get("pedestrianCount") is not None else fallback_count
        direction = str(fixed.get("pedestrianDirection", "opposite_direction") or "opposite_direction")
        y_offsets = _pedestrian_y_offsets(pedestrian_count, half_width_cm, index)
        goal_x_cm = float(_location(config, "goal").get("x", max_required_x_cm) or max_required_x_cm)
        pedestrians: list[dict[str, Any]] = []
        for pedestrian_index in range(pedestrian_count):
            y_cm = y_offsets[pedestrian_index]
            speed_kmh = round(3.6 + 0.2 * ((index + pedestrian_index) % 3), 3)
            spawn_time_s = round(0.5 * pedestrian_index + 0.25 * index, 3)
            if direction == "same_direction":
                spawn_x_cm = start_x_cm
                goal_x = goal_x_cm
                spawn_y_cm = goal_y_cm = y_cm
            elif direction == "crossing":
                spawn_x_cm = goal_x = _clamp(goal_x_cm * 0.5, 0.0, max_required_x_cm)
                spawn_y_cm = -half_width_cm + 5.0
                goal_y_cm = half_width_cm - 5.0
            else:
                spawn_x_cm = goal_x_cm
                goal_x = start_x_cm
                spawn_y_cm = goal_y_cm = y_cm
            pedestrians.append(
                _pedestrian(
                    pedestrian_index,
                    spawn_x_cm=spawn_x_cm,
                    spawn_y_cm=spawn_y_cm,
                    goal_x_cm=goal_x,
                    goal_y_cm=goal_y_cm,
                    behavior=direction,
                    speed_kmh=speed_kmh,
                    spawn_time_s=spawn_time_s,
                )
            )
        before_pedestrians = config.get("pedestrians")
        config["pedestrians"] = pedestrians
        _set_if_changed("pedestrians", before_pedestrians, pedestrians, changes)


def _pedestrian(
    index: int,
    *,
    spawn_x_cm: float,
    spawn_y_cm: float,
    goal_x_cm: float,
    goal_y_cm: float,
    behavior: str,
    speed_kmh: float,
    spawn_time_s: float = 0.0,
) -> dict[str, Any]:
    return {
        "objectId": f"pedestrian_{index + 1:02d}",
        "spawn": {"x": round(spawn_x_cm, 3), "y": round(spawn_y_cm, 3), "z": 0},
        "goal": {"x": round(goal_x_cm, 3), "y": round(goal_y_cm, 3), "z": 0},
        "behavior": behavior,
        "speedKmh": speed_kmh,
        "spawn_time_s": spawn_time_s,
    }


def _apply_scenario_variation_pattern(
    config: dict[str, Any],
    index: int,
    fixed: dict[str, Any],
    changes: list[str],
) -> None:
    length_cm = _map_length_cm(config)
    mid_x = length_cm * 0.5
    near_goal_x = length_cm * 0.68
    near_start_x = length_cm * 0.32
    pattern_index = index % 5
    width_by_pattern = [120, 120, 150, 180, 150][pattern_index]
    time_by_pattern = [30, 60, 90, 90, 60][pattern_index]
    first_blocking = float(fixed.get("obstacleBlockingRatio", [0.7, 0.5, 0.3, 0.9, 0.5][pattern_index]))
    obstacle_patterns = [
        [_obstacle(0, x_cm=mid_x, y_cm=0, blocking_ratio=first_blocking)],
        [
            _obstacle(0, x_cm=mid_x * 0.9, y_cm=-25, blocking_ratio=first_blocking),
            _obstacle(1, x_cm=near_goal_x, y_cm=30, blocking_ratio=0.7),
        ],
        [
            _obstacle(0, x_cm=near_goal_x, y_cm=-20, blocking_ratio=first_blocking),
            _obstacle(1, x_cm=mid_x, y_cm=28, blocking_ratio=0.5, obstacle_type="Kickboard"),
        ],
        [
            _obstacle(0, x_cm=near_start_x, y_cm=-35, blocking_ratio=first_blocking),
            _obstacle(1, x_cm=mid_x, y_cm=0, blocking_ratio=0.7),
            _obstacle(2, x_cm=near_goal_x, y_cm=35, blocking_ratio=0.9),
        ],
        [
            _obstacle(0, x_cm=mid_x * 1.08, y_cm=35, blocking_ratio=first_blocking),
            _obstacle(1, x_cm=near_start_x, y_cm=-30, blocking_ratio=0.3, obstacle_type="Kickboard"),
        ],
    ]
    pedestrian_patterns = [
        [],
        [
            _pedestrian(0, spawn_x_cm=near_start_x, spawn_y_cm=55, goal_x_cm=near_goal_x, goal_y_cm=55, behavior="same_direction", speed_kmh=3.6),
            _pedestrian(1, spawn_x_cm=near_goal_x, spawn_y_cm=-55, goal_x_cm=near_start_x, goal_y_cm=-55, behavior="opposite_direction", speed_kmh=4.2),
        ],
        [
            _pedestrian(0, spawn_x_cm=mid_x, spawn_y_cm=-55, goal_x_cm=mid_x, goal_y_cm=55, behavior="crossing", speed_kmh=3.2),
            _pedestrian(1, spawn_x_cm=near_goal_x, spawn_y_cm=55, goal_x_cm=length_cm * 0.9, goal_y_cm=55, behavior="same_direction", speed_kmh=3.8),
            _pedestrian(2, spawn_x_cm=length_cm * 0.85, spawn_y_cm=-55, goal_x_cm=mid_x, goal_y_cm=-55, behavior="opposite_direction", speed_kmh=4.0),
        ],
        [
            _pedestrian(0, spawn_x_cm=near_start_x, spawn_y_cm=-70, goal_x_cm=near_goal_x, goal_y_cm=-70, behavior="same_direction", speed_kmh=3.6),
            _pedestrian(1, spawn_x_cm=mid_x, spawn_y_cm=55, goal_x_cm=mid_x, goal_y_cm=-55, behavior="crossing", speed_kmh=3.0),
            _pedestrian(2, spawn_x_cm=near_goal_x, spawn_y_cm=60, goal_x_cm=near_start_x, goal_y_cm=60, behavior="opposite_direction", speed_kmh=4.3),
            _pedestrian(3, spawn_x_cm=length_cm * 0.78, spawn_y_cm=-55, goal_x_cm=length_cm * 0.78, goal_y_cm=55, behavior="crossing", speed_kmh=3.4),
        ],
        [
            _pedestrian(0, spawn_x_cm=near_goal_x, spawn_y_cm=60, goal_x_cm=near_start_x, goal_y_cm=60, behavior="opposite_direction", speed_kmh=4.0),
        ],
    ]

    map_config = config.get("map") if isinstance(config.get("map"), dict) else {}
    config["map"] = map_config
    if "sidewalkWidthCm" not in fixed:
        before_width = map_config.get("sidewalkWidthCm")
        map_config["sidewalkWidthCm"] = width_by_pattern
        _set_if_changed("map.sidewalkWidthCm", before_width, width_by_pattern, changes)

    runtime = config.get("runtime") if isinstance(config.get("runtime"), dict) else {}
    config["runtime"] = runtime
    if "timeLimitSec" not in fixed:
        before_time = runtime.get("maxDurationSec")
        runtime["maxDurationSec"] = time_by_pattern
        _set_if_changed("runtime.maxDurationSec", before_time, time_by_pattern, changes)

    before_obstacles = config.get("obstacles")
    config["obstacles"] = obstacle_patterns[pattern_index]
    _set_if_changed("obstacles", before_obstacles, config["obstacles"], changes)
    if isinstance(before_obstacles, list) and before_obstacles:
        before_y = ((before_obstacles[0] if isinstance(before_obstacles[0], dict) else {}).get("position") or {}).get("y")
        after_y = config["obstacles"][0]["position"]["y"]
        _set_if_changed("obstacles[0].position.y", before_y, after_y, changes)

    before_pedestrians = config.get("pedestrians")
    config["pedestrians"] = pedestrian_patterns[pattern_index]
    _set_if_changed("pedestrians", before_pedestrians, config["pedestrians"], changes)


def _apply_delivery_bot_tuning(
    config: dict[str, Any],
    index: int,
    fixed: dict[str, Any],
    changes: list[str],
    comparison_mode: ComparisonMode,
) -> None:
    tuning_index = index if comparison_mode == "policy_comparison" else 0
    tuning = delivery_bot_tuning_for_episode(
        tuning_index,
        fixed_parameters=fixed,
        scenario_intent=str(config.get("scenarioId", "")),
    )
    environment_sampling = _environment_sampling(config)
    environment_sampling["deliveryBotPolicyProfile"] = tuning.profile
    for key, value in tuning.values.items():
        environment_sampling[key] = value
    changes.extend(tuning.changed_parameters)


def generate_episode_variants(
    base_world_config: dict[str, Any],
    episode_count: int | None = None,
    base_seed: int | None = None,
    comparison_mode: ComparisonMode = "scenario_variation",
) -> list[EpisodeVariant]:
    settings = Settings()
    count = episode_count if episode_count is not None else settings.scenarioEpisodeDefaultCount
    seed_start = int(base_seed if base_seed is not None else base_world_config.get("seed", 0))
    fixed = _fixed_parameters(base_world_config)
    scenario_type = _scenario_type(base_world_config)
    variants: list[EpisodeVariant] = []

    for index in range(count):
        config = deepcopy(base_world_config)
        changes: list[str] = []
        seed = seed_start + index
        _set_seed(config, seed)
        _set_runtime_iteration(config, index)
        parameters = _sample_environment(
            f"{config.get('scenarioId', 'scenario')}-{index:03d}-ENV",
            seed,
            scenario_type,
            fixed,
        )
        environment_sampling = _environment_sampling(config)
        environment_sampling["enabled"] = True
        environment_sampling["seed"] = seed
        environment_sampling["scenarioType"] = scenario_type
        environment_sampling["fixedParameters"] = _sampler_fixed_parameters(fixed)
        environment_sampling["semanticFixedConstraints"] = {
            key: value for key, value in fixed.items() if key not in environment_sampling["fixedParameters"]
        }
        environment_sampling["parameters"] = parameters.model_dump(mode="json")
        _apply_environment_parameters(config, parameters, fixed, changes)
        if comparison_mode == "scenario_variation":
            _apply_scenario_variation_pattern(config, index, fixed, changes)
        _apply_fixed_scenario_constraints(config, index, fixed, changes)
        _apply_delivery_bot_tuning(config, index, fixed, changes, comparison_mode)
        variants.append(EpisodeVariant(world_config=config, changed_parameters=changes))
    return variants
