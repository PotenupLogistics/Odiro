from __future__ import annotations

import heapq
import math
from typing import Any, Iterable

from deliverybot_policy.context import get_float_field, get_int_field


GridIndex = tuple[int, int]


def is_blocked_cell(cell: dict[str, Any]) -> bool:
    return bool(cell.get("blocked", False)) or str(cell.get("areaType", "")) == "Blocked"


def get_cell_travel_cost(cell: dict[str, Any]) -> float:
    area_type = str(cell.get("areaType", "Walkable"))
    raw_cost = float(cell.get("cost", 1.0) or 1.0)

    if area_type == "Penalty":
        return max(raw_cost, 5.0)

    return max(raw_cost, 1.0)


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


def iter_neighbor_indexes(grid_size_x: int, grid_size_y: int, node: GridIndex) -> Iterable[GridIndex]:
    x, y = node
    for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1), (1, 1), (1, -1), (-1, 1), (-1, -1)):
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


def find_astar_path(
    grid_info: dict[str, Any],
    cell_lookup: dict[GridIndex, dict[str, Any]],
    start: GridIndex,
    goal: GridIndex,
) -> list[GridIndex]:
    grid_size_x = get_int_field(grid_info, "gridSizeX")
    grid_size_y = get_int_field(grid_info, "gridSizeY")

    if grid_size_x <= 0 or grid_size_y <= 0:
        return []

    start_cell = cell_lookup.get(start)
    goal_cell = cell_lookup.get(goal)
    if not isinstance(start_cell, dict) or not isinstance(goal_cell, dict):
        return []
    if is_blocked_cell(start_cell) or is_blocked_cell(goal_cell):
        return []

    open_heap: list[tuple[float, GridIndex]] = [(0.0, start)]
    came_from: dict[GridIndex, GridIndex] = {}
    g_score: dict[GridIndex, float] = {start: 0.0}
    closed_nodes: set[GridIndex] = set()

    while open_heap:
        _, current = heapq.heappop(open_heap)
        if current in closed_nodes:
            continue

        if current == goal:
            return reconstruct_path(came_from, current)

        closed_nodes.add(current)

        for neighbor in iter_neighbor_indexes(grid_size_x, grid_size_y, current):
            if neighbor in closed_nodes:
                continue

            cell = cell_lookup.get(neighbor)
            if not isinstance(cell, dict) or is_blocked_cell(cell):
                continue

            step_distance = math.sqrt(2.0) if neighbor[0] != current[0] and neighbor[1] != current[1] else 1.0
            tentative_g_score = g_score[current] + get_cell_travel_cost(cell) * step_distance

            if tentative_g_score >= g_score.get(neighbor, math.inf):
                continue

            came_from[neighbor] = current
            g_score[neighbor] = tentative_g_score
            priority = tentative_g_score + math.hypot(goal[0] - neighbor[0], goal[1] - neighbor[1])
            heapq.heappush(open_heap, (priority, neighbor))

    return []
