from __future__ import annotations

from dataclasses import dataclass
import heapq
import math
from typing import Any, Iterable

from deliverybot_policy.context import get_float_field, get_int_field


GridIndex = tuple[int, int]


@dataclass(frozen=True)
class AStarOptions:
    allow_diagonal: bool = True
    prevent_corner_cutting: bool = True
    avoid_unknown_cells: bool = True
    heuristic_weight: float = 1.0
    penalty_minimum_cost: float = 5.0
    penalty_cost_multiplier: float = 1.0
    walkable_cost_multiplier: float = 1.0
    diagonal_cost_multiplier: float = 1.0
    blocked_cost_threshold: float = 1.0e30
    max_expanded_nodes: int = 100_000
    blocked_area_types: tuple[str, ...] = ("Blocked",)
    penalty_area_types: tuple[str, ...] = ("Penalty",)


@dataclass(frozen=True)
class AStarResult:
    path: list[GridIndex]
    status: str
    expanded_nodes: int = 0
    path_cost: float = 0.0


def get_option_value(source: dict[str, Any], names: tuple[str, ...], default: Any) -> Any:
    for name in names:
        if name in source:
            return source[name]

    return default


def get_bool_option(source: dict[str, Any], names: tuple[str, ...], default: bool) -> bool:
    value = get_option_value(source, names, default)
    if isinstance(value, bool):
        return value
    if isinstance(value, str):
        return value.strip().lower() in {"1", "true", "yes", "y", "on"}

    return bool(value)


def get_float_option(source: dict[str, Any], names: tuple[str, ...], default: float) -> float:
    value = get_option_value(source, names, default)
    try:
        return float(value)
    except (TypeError, ValueError):
        return default


def get_int_option(source: dict[str, Any], names: tuple[str, ...], default: int) -> int:
    value = get_option_value(source, names, default)
    try:
        return int(value)
    except (TypeError, ValueError):
        return default


def get_string_tuple_option(source: dict[str, Any], names: tuple[str, ...], default: tuple[str, ...]) -> tuple[str, ...]:
    value = get_option_value(source, names, default)
    if isinstance(value, str):
        return (value,)
    if isinstance(value, list):
        return tuple(str(item) for item in value if str(item))
    if isinstance(value, tuple):
        return tuple(str(item) for item in value if str(item))

    return default


def build_astar_options(pathfinding_spec: dict[str, Any] | None = None) -> AStarOptions:
    safe_spec = pathfinding_spec if isinstance(pathfinding_spec, dict) else {}

    return AStarOptions(
        allow_diagonal=get_bool_option(safe_spec, ("allowDiagonal", "allow_diagonal"), True),
        prevent_corner_cutting=get_bool_option(
            safe_spec,
            ("preventCornerCutting", "prevent_corner_cutting"),
            True,
        ),
        avoid_unknown_cells=get_bool_option(safe_spec, ("avoidUnknownCells", "avoid_unknown_cells"), True),
        heuristic_weight=max(get_float_option(safe_spec, ("heuristicWeight", "heuristic_weight"), 1.0), 0.0),
        penalty_minimum_cost=max(
            get_float_option(safe_spec, ("penaltyMinimumCost", "penalty_minimum_cost"), 5.0),
            1.0,
        ),
        penalty_cost_multiplier=max(
            get_float_option(safe_spec, ("penaltyCostMultiplier", "penalty_cost_multiplier"), 1.0),
            0.0,
        ),
        walkable_cost_multiplier=max(
            get_float_option(safe_spec, ("walkableCostMultiplier", "walkable_cost_multiplier"), 1.0),
            0.0,
        ),
        diagonal_cost_multiplier=max(
            get_float_option(safe_spec, ("diagonalCostMultiplier", "diagonal_cost_multiplier"), 1.0),
            0.0,
        ),
        blocked_cost_threshold=max(
            get_float_option(safe_spec, ("blockedCostThreshold", "blocked_cost_threshold"), 1.0e30),
            1.0,
        ),
        max_expanded_nodes=max(get_int_option(safe_spec, ("maxExpandedNodes", "max_expanded_nodes"), 100_000), 1),
        blocked_area_types=get_string_tuple_option(
            safe_spec,
            ("blockedAreaTypes", "blocked_area_types"),
            ("Blocked",),
        ),
        penalty_area_types=get_string_tuple_option(
            safe_spec,
            ("penaltyAreaTypes", "penalty_area_types"),
            ("Penalty",),
        ),
    )


def get_policy_pathfinding_spec(policy_entry: dict[str, Any]) -> dict[str, Any]:
    if not isinstance(policy_entry, dict):
        return {}

    pathfinding_spec = policy_entry.get("pathfinding", {})
    if isinstance(pathfinding_spec, dict):
        return pathfinding_spec

    parameters = policy_entry.get("parameters", {})
    if isinstance(parameters, dict) and isinstance(parameters.get("pathfinding"), dict):
        return parameters["pathfinding"]

    return {}


def coerce_astar_options(options: AStarOptions | dict[str, Any] | None) -> AStarOptions:
    if isinstance(options, AStarOptions):
        return options

    return build_astar_options(options if isinstance(options, dict) else {})


def is_blocked_cell(cell: dict[str, Any] | None, options: AStarOptions | dict[str, Any] | None = None) -> bool:
    safe_options = coerce_astar_options(options)
    if not isinstance(cell, dict):
        return safe_options.avoid_unknown_cells

    area_type = str(cell.get("areaType", ""))
    raw_cost = get_float_field(cell, "cost", 1.0)

    return (
        bool(cell.get("blocked", False))
        or area_type in safe_options.blocked_area_types
        or raw_cost >= safe_options.blocked_cost_threshold
    )


def get_cell_travel_cost(cell: dict[str, Any], options: AStarOptions | dict[str, Any] | None = None) -> float:
    safe_options = coerce_astar_options(options)
    area_type = str(cell.get("areaType", "Walkable"))
    raw_cost = float(cell.get("cost", 1.0) or 1.0)
    base_cost = max(raw_cost, 1.0)

    if area_type in safe_options.penalty_area_types:
        return max(base_cost, safe_options.penalty_minimum_cost) * safe_options.penalty_cost_multiplier

    return base_cost * safe_options.walkable_cost_multiplier


def world_to_grid_index(grid_info: dict[str, Any], world_x_cm: float, world_y_cm: float) -> GridIndex | None:
    cell_size_cm = get_float_field(grid_info, "cellSizeCm")
    grid_size_x = get_int_field(grid_info, "gridSizeX")
    grid_size_y = get_int_field(grid_info, "gridSizeY")
    origin_cm = grid_info.get("originCm", {})

    if cell_size_cm <= 0.0 or grid_size_x <= 0 or grid_size_y <= 0 or not isinstance(origin_cm, dict):
        return None

    origin_x_cm = get_float_field(origin_cm, "x")
    origin_y_cm = get_float_field(origin_cm, "y")

    grid_x = math.floor((world_x_cm - origin_x_cm) / cell_size_cm)
    grid_y = math.floor((world_y_cm - origin_y_cm) / cell_size_cm)

    if grid_x < 0 or grid_y < 0 or grid_x >= grid_size_x or grid_y >= grid_size_y:
        return None

    return int(grid_x), int(grid_y)


def grid_index_to_world_location(grid_info: dict[str, Any], grid_index: GridIndex) -> dict[str, float]:
    cell_size_cm = get_float_field(grid_info, "cellSizeCm")
    origin_cm = grid_info.get("originCm", {})
    safe_origin_cm = origin_cm if isinstance(origin_cm, dict) else {}

    return {
        "x": get_float_field(safe_origin_cm, "x") + (grid_index[0] + 0.5) * cell_size_cm,
        "y": get_float_field(safe_origin_cm, "y") + (grid_index[1] + 0.5) * cell_size_cm,
        "z": get_float_field(safe_origin_cm, "z"),
    }


def iter_neighbor_indexes(
    grid_size_x: int,
    grid_size_y: int,
    node: GridIndex,
    allow_diagonal: bool = True,
) -> Iterable[GridIndex]:
    x, y = node
    neighbor_offsets = [(1, 0), (-1, 0), (0, 1), (0, -1)]
    if allow_diagonal:
        neighbor_offsets.extend([(1, 1), (1, -1), (-1, 1), (-1, -1)])

    for dx, dy in neighbor_offsets:
        next_x = x + dx
        next_y = y + dy
        if 0 <= next_x < grid_size_x and 0 <= next_y < grid_size_y:
            yield next_x, next_y


def reconstruct_path(came_from: dict[GridIndex, GridIndex], current: GridIndex) -> list[GridIndex]:
    path = [current]
    while current in came_from:
        current = came_from[current]
        path.append(current)

    path.reverse()
    return path


def is_diagonal_move(first: GridIndex, second: GridIndex) -> bool:
    return first[0] != second[0] and first[1] != second[1]


def can_move_to_neighbor(
    cell_lookup: dict[GridIndex, dict[str, Any]],
    current: GridIndex,
    neighbor: GridIndex,
    options: AStarOptions,
) -> bool:
    cell = cell_lookup.get(neighbor)
    if is_blocked_cell(cell, options):
        return False

    if not is_diagonal_move(current, neighbor) or not options.prevent_corner_cutting:
        return True

    side_cell_a = cell_lookup.get((neighbor[0], current[1]))
    side_cell_b = cell_lookup.get((current[0], neighbor[1]))
    return not is_blocked_cell(side_cell_a, options) and not is_blocked_cell(side_cell_b, options)


def get_step_distance(current: GridIndex, neighbor: GridIndex, options: AStarOptions) -> float:
    if is_diagonal_move(current, neighbor):
        return math.sqrt(2.0) * options.diagonal_cost_multiplier

    return 1.0


def estimate_heuristic_cost(node: GridIndex, goal: GridIndex, options: AStarOptions) -> float:
    dx = abs(goal[0] - node[0])
    dy = abs(goal[1] - node[1])

    if options.allow_diagonal:
        diagonal_cost = math.sqrt(2.0) * options.diagonal_cost_multiplier
        straight_cost = 1.0
        heuristic = straight_cost * (dx + dy) + (diagonal_cost - 2.0 * straight_cost) * min(dx, dy)
    else:
        heuristic = dx + dy

    return heuristic * options.heuristic_weight


def build_pathfinding_debug(result: AStarResult) -> dict[str, Any]:
    return {
        "pathStatus": result.status,
        "pathLength": len(result.path),
        "pathCost": result.path_cost,
        "expandedNodeCount": result.expanded_nodes,
    }


def build_path_points_debug(
    grid_info: dict[str, Any],
    path: list[GridIndex],
    max_points: int = 200,
) -> dict[str, Any]:
    safe_max_points = max(int(max_points), 2)
    sampled_path = path

    if len(path) > safe_max_points:
        sample_step = max(math.ceil(len(path) / safe_max_points), 1)
        sampled_path = path[::sample_step]
        if sampled_path[-1] != path[-1]:
            sampled_path.append(path[-1])

    return {
        "pathGridPoints": [{"x": grid_index[0], "y": grid_index[1]} for grid_index in sampled_path],
        "pathWorldPoints": [grid_index_to_world_location(grid_info, grid_index) for grid_index in sampled_path],
    }


def find_astar_path_result(
    grid_info: dict[str, Any],
    cell_lookup: dict[GridIndex, dict[str, Any]],
    start: GridIndex,
    goal: GridIndex,
    options: AStarOptions | dict[str, Any] | None = None,
) -> AStarResult:
    safe_options = coerce_astar_options(options)
    grid_size_x = get_int_field(grid_info, "gridSizeX")
    grid_size_y = get_int_field(grid_info, "gridSizeY")

    if grid_size_x <= 0 or grid_size_y <= 0:
        return AStarResult([], "invalid_grid")

    start_cell = cell_lookup.get(start)
    goal_cell = cell_lookup.get(goal)
    if not isinstance(start_cell, dict) or not isinstance(goal_cell, dict):
        return AStarResult([], "missing_start_or_goal_cell")
    if is_blocked_cell(start_cell, safe_options):
        return AStarResult([], "start_blocked")
    if is_blocked_cell(goal_cell, safe_options):
        return AStarResult([], "goal_blocked")

    open_heap: list[tuple[float, GridIndex]] = [(0.0, start)]
    came_from: dict[GridIndex, GridIndex] = {}
    g_score: dict[GridIndex, float] = {start: 0.0}
    closed_nodes: set[GridIndex] = set()
    expanded_nodes = 0

    while open_heap:
        _, current = heapq.heappop(open_heap)
        if current in closed_nodes:
            continue

        if current == goal:
            return AStarResult(reconstruct_path(came_from, current), "ok", expanded_nodes, g_score[current])

        closed_nodes.add(current)
        expanded_nodes += 1

        if expanded_nodes >= safe_options.max_expanded_nodes:
            return AStarResult([], "max_expanded_nodes_reached", expanded_nodes)

        for neighbor in iter_neighbor_indexes(grid_size_x, grid_size_y, current, safe_options.allow_diagonal):
            if neighbor in closed_nodes:
                continue

            if not can_move_to_neighbor(cell_lookup, current, neighbor, safe_options):
                continue

            cell = cell_lookup[neighbor]
            step_distance = get_step_distance(current, neighbor, safe_options)
            tentative_g_score = g_score[current] + get_cell_travel_cost(cell, safe_options) * step_distance

            if tentative_g_score >= g_score.get(neighbor, math.inf):
                continue

            came_from[neighbor] = current
            g_score[neighbor] = tentative_g_score
            priority = tentative_g_score + estimate_heuristic_cost(neighbor, goal, safe_options)
            heapq.heappush(open_heap, (priority, neighbor))

    return AStarResult([], "not_found", expanded_nodes)


def find_astar_path(
    grid_info: dict[str, Any],
    cell_lookup: dict[GridIndex, dict[str, Any]],
    start: GridIndex,
    goal: GridIndex,
    options: AStarOptions | dict[str, Any] | None = None,
) -> list[GridIndex]:
    return find_astar_path_result(grid_info, cell_lookup, start, goal, options).path


def find_policy_astar_path(
    grid_info: dict[str, Any],
    cell_lookup: dict[GridIndex, dict[str, Any]],
    start: GridIndex,
    goal: GridIndex,
    policy_entry: dict[str, Any],
) -> AStarResult:
    return find_astar_path_result(
        grid_info,
        cell_lookup,
        start,
        goal,
        build_astar_options(get_policy_pathfinding_spec(policy_entry)),
    )
