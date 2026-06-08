from __future__ import annotations

from dataclasses import dataclass, field
from typing import Any

from app.models.episode_setup import EpisodeSetup
from app.catalogs.static_obstacle_catalog import find_static_obstacle_by_prop_id
from app.services.setup_pair_queue_generator import SetupPairQueueResult


START_GOAL_MARGIN_M = 2.0
PEDESTRIAN_EDGE_MARGIN_M = 0.2
OBSTACLE_PATH_BUFFER_M = 0.3
MIN_SIDEWALK_WIDTH_M = 1.5
EXPLICIT_EXTREME_SIDEWALK_WIDTH_M = 1.0


@dataclass(frozen=True)
class ScenarioConsistencyIssue:
    code: str
    message: str
    severity: str = "error"
    path: str | None = None


@dataclass(frozen=True)
class ScenarioConsistencyResult:
    passed: bool
    issues: list[ScenarioConsistencyIssue] = field(default_factory=list)


def _as_float(value: Any) -> float | None:
    if isinstance(value, bool):
        return None
    if isinstance(value, int | float):
        return float(value)
    return None


def _as_int(value: Any) -> int | None:
    if isinstance(value, bool):
        return None
    if isinstance(value, int):
        return value
    if isinstance(value, float) and value.is_integer():
        return int(value)
    return None


def _float_list(value: Any) -> list[float]:
    if not isinstance(value, list):
        return []
    items: list[float] = []
    for item in value:
        number = _as_float(item)
        if number is not None:
            items.append(number)
    return items


def _near(left: float, right: float, tolerance: float = 0.001) -> bool:
    return abs(left - right) <= tolerance


def _obstacle_kind(obstacle: Any) -> str:
    prop_id = str(getattr(obstacle, "prop_id", "")).lower()
    semantic_type = str(obstacle.properties.get("semantic_type", "")).lower()
    if prop_id in {"obstacle.trash_bin", "obstacle.bin"}:
        return "trash_bin"
    if prop_id.startswith("obstacle.box"):
        return "box"
    if prop_id.startswith("obstacle.road_cone"):
        return "traffic_cone"
    if prop_id.startswith("obstacle.road_barrier"):
        return "road_barrier"
    if prop_id.startswith("obstacle.manhole"):
        return "manhole"
    if prop_id == "obstacle.fire_hydrant":
        return "fire_hydrant"
    if prop_id == "obstacle.mailbox":
        return "mailbox"
    if prop_id == "obstacle.street_bank":
        return "street_bank"
    return semantic_type or prop_id or "unknown"


def _sidewalk_bounds(episode: EpisodeSetup) -> tuple[float, float, float, float] | None:
    region = next((item for item in episode.ground_model.regions if item.region_id == "sidewalk_main"), None)
    if region is None:
        return None
    center_x, center_y = region.shape.center_xy_m
    size_x, size_y = region.shape.size_m
    return (
        center_x - size_x / 2.0,
        center_x + size_x / 2.0,
        center_y - size_y / 2.0,
        center_y + size_y / 2.0,
    )


def _point_inside(bounds: tuple[float, float, float, float], point: list[float]) -> bool:
    min_x, max_x, min_y, max_y = bounds
    return min_x <= point[0] <= max_x and min_y <= point[1] <= max_y


def _point_inside_margin(bounds: tuple[float, float, float, float], point: list[float], margin_m: float) -> bool:
    min_x, max_x, min_y, max_y = bounds
    return min_x + margin_m <= point[0] <= max_x - margin_m and min_y + margin_m <= point[1] <= max_y - margin_m


def _point_inside_x_margin(bounds: tuple[float, float, float, float], point: list[float], margin_m: float) -> bool:
    min_x, max_x, min_y, max_y = bounds
    return min_x + margin_m <= point[0] <= max_x - margin_m and min_y <= point[1] <= max_y


def _expected_sidewalk_width_m(width_cm: float) -> float:
    width_m = width_cm / 100.0
    if width_m <= EXPLICIT_EXTREME_SIDEWALK_WIDTH_M:
        return width_m
    return max(width_m, MIN_SIDEWALK_WIDTH_M)


def _obstacle_rect_with_buffer(obstacle: Any) -> tuple[float, float, float, float]:
    catalog_item = find_static_obstacle_by_prop_id(str(getattr(obstacle, "prop_id", "")))
    bbox_m = catalog_item["bbox_m"] if catalog_item else [0.0, 0.0, 0.0]
    half_x = float(bbox_m[0]) / 2.0 + OBSTACLE_PATH_BUFFER_M
    half_y = float(bbox_m[1]) / 2.0 + OBSTACLE_PATH_BUFFER_M
    x, y = obstacle.xy_m
    return x - half_x, x + half_x, y - half_y, y + half_y


def _segment_intersects_rect(
    start: list[float],
    end: list[float],
    rect: tuple[float, float, float, float],
) -> bool:
    min_x, max_x, min_y, max_y = rect
    if _point_inside(rect, start) or _point_inside(rect, end):
        return True
    x1, y1 = start
    x2, y2 = end
    dx = x2 - x1
    dy = y2 - y1
    t_min = 0.0
    t_max = 1.0
    for edge, distance in ((-dx, x1 - min_x), (dx, max_x - x1), (-dy, y1 - min_y), (dy, max_y - y1)):
        if abs(edge) < 1e-12:
            if distance < 0:
                return False
            continue
        ratio = distance / edge
        if edge < 0:
            t_min = max(t_min, ratio)
        else:
            t_max = min(t_max, ratio)
        if t_min > t_max:
            return False
    return True


def _check_episode(
    episode: EpisodeSetup,
    fixed_constraints: dict[str, Any],
    issues: list[ScenarioConsistencyIssue],
    episode_index: int,
) -> None:
    region = next((item for item in episode.ground_model.regions if item.region_id == "sidewalk_main"), None)
    if episode.ground_model.default_region_type != "blocked":
        issues.append(
            ScenarioConsistencyIssue(
                code="default_region_type_mismatch",
                message="ground_model.default_region_type must be blocked.",
                path=f"items[{episode_index}].episode_setup.ground_model.default_region_type",
            )
        )
    if region is None or region.region_type != "walkable":
        issues.append(
            ScenarioConsistencyIssue(
                code="sidewalk_region_mismatch",
                message="sidewalk_main must exist and be walkable.",
                path=f"items[{episode_index}].episode_setup.ground_model.regions",
            )
        )
    sidewalk_width_cm = _as_float(fixed_constraints.get("sidewalkWidthCm"))
    if sidewalk_width_cm is not None and region is not None and not _near(region.shape.size_m[1], _expected_sidewalk_width_m(sidewalk_width_cm)):
        issues.append(
            ScenarioConsistencyIssue(
                code="sidewalk_width_mismatch",
                message="Episode sidewalk width does not match fixed or minimum safe sidewalk width.",
                path=f"items[{episode_index}].episode_setup.ground_model.regions[sidewalk_main].shape.size_m",
            )
        )

    robot = episode.actors.robot
    if robot.route is not None:
        goal_distance_m = _as_float(fixed_constraints.get("goalDistanceM"))
        if goal_distance_m is not None and not _near(robot.route.goal_xy_m[0] - robot.xy_m[0], goal_distance_m):
            issues.append(
                ScenarioConsistencyIssue(
                    code="goal_distance_mismatch",
                    message="Robot goal distance does not match fixed goalDistanceM.",
                    path=f"items[{episode_index}].episode_setup.actors.robot.route.goal_xy_m",
                )
            )

    obstacle_count = _as_int(fixed_constraints.get("obstacleCount"))
    if obstacle_count is not None and len(episode.actors.static_obstacles) != obstacle_count:
        issues.append(
            ScenarioConsistencyIssue(
                code="obstacle_count_mismatch",
                message="Static obstacle count does not match fixed obstacleCount.",
                path=f"items[{episode_index}].episode_setup.actors.static_obstacles",
            )
        )
    obstacle_types = fixed_constraints.get("obstacleTypes")
    if isinstance(obstacle_types, list) and obstacle_types:
        actual_types = [_obstacle_kind(obstacle) for obstacle in episode.actors.static_obstacles]
        expected_types = [
            "road_barrier" if str(item) == "kickboard" else str(item)
            for item in obstacle_types
        ]
        if actual_types[: len(expected_types)] != expected_types:
            issues.append(
                ScenarioConsistencyIssue(
                    code="obstacle_type_mismatch",
                    message="Static obstacle types do not match fixed obstacleTypes.",
                    path=f"items[{episode_index}].episode_setup.actors.static_obstacles",
                )
            )
    elif fixed_constraints.get("obstacleType") in {"box", "static_obstacle"}:
        for obstacle_index, obstacle in enumerate(episode.actors.static_obstacles):
            if not obstacle.prop_id.startswith("obstacle.box"):
                issues.append(
                    ScenarioConsistencyIssue(
                        code="obstacle_type_mismatch",
                        message="Static obstacle prop_id does not match fixed static obstacle type.",
                        path=f"items[{episode_index}].episode_setup.actors.static_obstacles[{obstacle_index}].prop_id",
                    )
                )
    requested_positions = _float_list(fixed_constraints.get("obstaclePositionsFromStartM"))
    if requested_positions:
        actual_positions = [obstacle.xy_m[0] - robot.xy_m[0] for obstacle in episode.actors.static_obstacles]
        for expected, actual in zip(requested_positions, actual_positions, strict=False):
            if not _near(actual, expected):
                issues.append(
                    ScenarioConsistencyIssue(
                        code="obstacle_position_mismatch",
                        message="Static obstacle x position does not match fixed obstacle position.",
                        path=f"items[{episode_index}].episode_setup.actors.static_obstacles",
                    )
                )
                break

    pedestrian_count = _as_int(fixed_constraints.get("pedestrianCount"))
    if pedestrian_count is not None and len(episode.actors.pedestrians) != pedestrian_count:
        issues.append(
            ScenarioConsistencyIssue(
                code="pedestrian_count_mismatch",
                message="Pedestrian count does not match fixed pedestrianCount.",
                path=f"items[{episode_index}].episode_setup.actors.pedestrians",
            )
        )
    pedestrian_direction = fixed_constraints.get("pedestrianDirection")
    if pedestrian_direction is not None:
        for pedestrian_index, pedestrian in enumerate(episode.actors.pedestrians):
            if pedestrian.properties.get("semantic_behavior") != pedestrian_direction:
                issues.append(
                    ScenarioConsistencyIssue(
                        code="pedestrian_direction_mismatch",
                        message="Pedestrian behavior does not match fixed pedestrianDirection.",
                        path=f"items[{episode_index}].episode_setup.actors.pedestrians[{pedestrian_index}].properties.semantic_behavior",
                    )
                )

    bounds = _sidewalk_bounds(episode)
    if bounds is None:
        return
    if not _point_inside_x_margin(bounds, robot.xy_m, START_GOAL_MARGIN_M):
        issues.append(
            ScenarioConsistencyIssue(
                code="robot_start_margin_violation",
                message="Robot start must be inside the walkable region with x margin.",
                path=f"items[{episode_index}].episode_setup.actors.robot.xy_m",
            )
        )
    if robot.route is not None and not _point_inside_x_margin(bounds, robot.route.goal_xy_m, START_GOAL_MARGIN_M):
        issues.append(
            ScenarioConsistencyIssue(
                code="robot_goal_margin_violation",
                message="Robot goal must be inside the walkable region with x margin.",
                path=f"items[{episode_index}].episode_setup.actors.robot.route.goal_xy_m",
            )
        )

    path_by_id = {path.path_id: path for path in episode.paths}
    obstacle_rects = [_obstacle_rect_with_buffer(obstacle) for obstacle in episode.actors.static_obstacles]
    check_crossing_path_geometry = str(fixed_constraints.get("pedestrianDirection", "")).lower() == "crossing"
    for pedestrian_index, pedestrian in enumerate(episode.actors.pedestrians):
        path = path_by_id.get(pedestrian.path_id)
        if path is None:
            continue
        is_crossing = str(pedestrian.properties.get("semantic_behavior", "")).lower() == "crossing"
        for point_index, point in enumerate(path.points_xy_m):
            if check_crossing_path_geometry and is_crossing and not _point_inside_margin(bounds, point, PEDESTRIAN_EDGE_MARGIN_M):
                issues.append(
                    ScenarioConsistencyIssue(
                        code="pedestrian_path_margin_violation",
                        message="Pedestrian path endpoint must stay inside the walkable region margin.",
                        path=f"items[{episode_index}].episode_setup.paths[{path.path_id}].points_xy_m[{point_index}]",
                    )
                )
                break
        if check_crossing_path_geometry and is_crossing and len(path.points_xy_m) >= 2:
            start = path.points_xy_m[0]
            end = path.points_xy_m[-1]
            for rect in obstacle_rects:
                if _segment_intersects_rect(start, end, rect):
                    issues.append(
                        ScenarioConsistencyIssue(
                            code="pedestrian_path_obstacle_overlap",
                            message="Pedestrian path segment intersects a static obstacle bbox plus buffer.",
                            path=f"items[{episode_index}].episode_setup.actors.pedestrians[{pedestrian_index}].path_id",
                        )
                    )
                    break

    points = [robot.xy_m]
    if robot.route is not None:
        points.append(robot.route.goal_xy_m)
    points.extend(obstacle.xy_m for obstacle in episode.actors.static_obstacles)
    points.extend(pedestrian.xy_m for pedestrian in episode.actors.pedestrians)
    for path in episode.paths:
        points.extend(path.points_xy_m)
    for point_index, point in enumerate(points):
        if not _point_inside(bounds, point):
            issues.append(
                ScenarioConsistencyIssue(
                    code="actor_or_path_outside_sidewalk",
                    message="Actor or path point is outside sidewalk_main.",
                    path=f"items[{episode_index}].episode_setup.points[{point_index}]",
                )
            )
            break


def check_setup_pair_queue_consistency(
    queue: SetupPairQueueResult,
    *,
    fixed_constraints: dict[str, Any] | None = None,
    expected_episode_count: int | None = None,
) -> ScenarioConsistencyResult:
    constraints = fixed_constraints or {}
    issues: list[ScenarioConsistencyIssue] = []

    if expected_episode_count is not None and len(queue.run_queue.runs) != expected_episode_count:
        issues.append(
            ScenarioConsistencyIssue(
                code="run_count_mismatch",
                message="RunQueue runs count does not match requested episode_count.",
                path="run_queue.runs",
            )
        )
    if len(queue.items) != len(queue.run_queue.runs):
        issues.append(
            ScenarioConsistencyIssue(
                code="delivery_bot_setup_count_mismatch",
                message="Setup pair item count must match RunQueue runs count.",
                path="items",
            )
        )

    for index, item in enumerate(queue.items):
        if index >= len(queue.run_queue.runs):
            continue
        run = queue.run_queue.runs[index]
        if run.episode_setup != item.episode_setup_path:
            issues.append(
                ScenarioConsistencyIssue(
                    code="artifact_path_mismatch",
                    message="RunQueue episode_setup path does not match generated EpisodeSetup filename.",
                    path=f"run_queue.runs[{index}].episode_setup",
                )
            )
        if run.delivery_bot_setup != item.delivery_bot_setup_path:
            issues.append(
                ScenarioConsistencyIssue(
                    code="artifact_path_mismatch",
                    message="RunQueue delivery_bot_setup path does not match generated DeliveryBotSetup filename.",
                    path=f"run_queue.runs[{index}].delivery_bot_setup",
                )
            )
        if not item.pair_id.endswith("_baseline"):
            issues.append(
                ScenarioConsistencyIssue(
                    code="delivery_bot_profile_mismatch",
                    message="Default scenario generation should use baseline DeliveryBotSetup.",
                    path=f"items[{index}].pair_id",
                    severity="warning",
                )
            )
        _check_episode(item.episode_setup, constraints, issues, index)

    return ScenarioConsistencyResult(passed=not issues, issues=issues)
