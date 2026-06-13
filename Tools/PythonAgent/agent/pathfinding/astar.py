import heapq
import math

from ..contract import GoalLocation, GridCell, GridMap, RobotState, StartLocation


class AStarResult:
    def __init__(self, success: bool, path: list[tuple[int, int]], reason: str):
        self.success = success
        self.path = path
        self.reason = reason


class AStarPathfinder:
    def __init__(
        self,
        obstacle_soft_cost_radius_m: float = 2.0,
        obstacle_soft_cost_max_penalty: float = 8.0,
        obstacle_soft_cost_power: float = 2.0,
        path_turn_cost_penalty: float = 1.5,
        allow_diagonal_pathfinding: bool = True,
        smooth_path_with_line_of_sight: bool = True,
    ):
        self.obstacleSoftCostRadiusM = obstacle_soft_cost_radius_m
        self.obstacleSoftCostMaxPenalty = obstacle_soft_cost_max_penalty
        self.obstacleSoftCostPower = obstacle_soft_cost_power
        self.pathTurnCostPenalty = path_turn_cost_penalty
        self.bAllowDiagonalPathfinding = allow_diagonal_pathfinding
        self.bSmoothPathWithLineOfSight = smooth_path_with_line_of_sight

    def configure_from_control_spec(self, control_spec: dict | None) -> None:
        control_spec = control_spec or {}
        self.obstacleSoftCostRadiusM = max(
            0.0,
            float(control_spec.get("obstacleSoftCostRadiusM", self.obstacleSoftCostRadiusM)),
        )
        self.obstacleSoftCostMaxPenalty = max(
            0.0,
            float(control_spec.get("obstacleSoftCostMaxPenalty", self.obstacleSoftCostMaxPenalty)),
        )
        self.obstacleSoftCostPower = max(
            0.1,
            float(control_spec.get("obstacleSoftCostPower", self.obstacleSoftCostPower)),
        )
        self.pathTurnCostPenalty = max(
            0.0,
            float(control_spec.get("pathTurnCostPenalty", self.pathTurnCostPenalty)),
        )
        self.bAllowDiagonalPathfinding = self.get_bool_config(
            control_spec,
            "allowDiagonalPathfinding",
            self.bAllowDiagonalPathfinding,
        )
        self.bSmoothPathWithLineOfSight = self.get_bool_config(
            control_spec,
            "smoothPathWithLineOfSight",
            self.bSmoothPathWithLineOfSight,
        )

    def find_path(
        self,
        start: StartLocation | RobotState,
        goal: GoalLocation,
        grid: GridMap,
    ) -> AStarResult:
        if not goal.hasGoal:
            return AStarResult(False, [], "goal_not_available")

        cell_lookup = self.build_cell_lookup(grid)
        obstacle_soft_costs = self.build_obstacle_soft_costs(grid, cell_lookup)

        start_cell = self.world_to_cell(start.x, start.y, grid)
        goal_cell = self.world_to_cell(goal.x, goal.y, grid)

        if not self.is_walkable(start_cell, grid, cell_lookup):
            return AStarResult(False, [], "start_cell_blocked")

        if not self.is_walkable(goal_cell, grid, cell_lookup):
            return AStarResult(False, [], "goal_cell_blocked")

        start_state = self.make_path_state(start_cell, (0, 0))
        open_set: list[tuple[float, int, tuple[int, int, int, int]]] = []
        push_order = 0
        heapq.heappush(open_set, (0.0, push_order, start_state))

        came_from: dict[tuple[int, int, int, int], tuple[int, int, int, int]] = {}
        g_score: dict[tuple[int, int, int, int], float] = {start_state: 0.0}

        while open_set:
            _, _, current_state = heapq.heappop(open_set)
            current = self.get_cell_from_path_state(current_state)
            current_direction = self.get_direction_from_path_state(current_state)

            if current == goal_cell:
                path = self.reconstruct_path(came_from, current_state)
                path = self.smooth_path(path, grid, cell_lookup)
                return AStarResult(True, path, "path_found")

            for neighbor in self.get_neighbors(current, grid, cell_lookup):
                next_direction = self.get_move_direction(current, neighbor)
                next_state = self.make_path_state(neighbor, next_direction)
                move_cost = self.get_move_cost(current, neighbor, grid, cell_lookup, obstacle_soft_costs)
                turn_cost = self.get_turn_cost(current_direction, next_direction)
                next_score = g_score[current_state] + move_cost + turn_cost

                if next_score < g_score.get(next_state, float("inf")):
                    came_from[next_state] = current_state
                    g_score[next_state] = next_score

                    priority = next_score + self.heuristic(neighbor, goal_cell)
                    push_order += 1
                    heapq.heappush(open_set, (priority, push_order, next_state))

        return AStarResult(False, [], "path_not_found")

    def world_to_cell(self, x: float, y: float, grid: GridMap) -> tuple[int, int]:
        local_x = x - grid.originCm.x
        local_y = y - grid.originCm.y
        return int(local_x // grid.cellSizeCm), int(local_y // grid.cellSizeCm)

    def get_neighbors(
        self,
        cell: tuple[int, int],
        grid: GridMap,
        cell_lookup: dict[tuple[int, int], GridCell] | None = None,
    ) -> list[tuple[int, int]]:
        x, y = cell
        candidates = [
            (x + direction_x, y + direction_y)
            for direction_x, direction_y in self.get_neighbor_directions()
        ]
        return [
            candidate
            for candidate in candidates
            if self.can_move_between(cell, candidate, grid, cell_lookup)
        ]

    def get_neighbor_directions(self) -> list[tuple[int, int]]:
        directions = [
            (1, 0),
            (-1, 0),
            (0, 1),
            (0, -1),
        ]

        if self.bAllowDiagonalPathfinding:
            directions.extend([
                (1, 1),
                (1, -1),
                (-1, 1),
                (-1, -1),
            ])

        return directions

    def can_move_between(
        self,
        from_cell: tuple[int, int],
        to_cell: tuple[int, int],
        grid: GridMap,
        cell_lookup: dict[tuple[int, int], GridCell] | None = None,
    ) -> bool:
        if not self.is_walkable(to_cell, grid, cell_lookup):
            return False

        direction = self.get_move_direction(from_cell, to_cell)
        if not self.is_diagonal_direction(direction):
            return True

        if not self.bAllowDiagonalPathfinding:
            return False

        # Prevent cutting through a blocked obstacle corner.
        side_a = (from_cell[0] + direction[0], from_cell[1])
        side_b = (from_cell[0], from_cell[1] + direction[1])
        return (
            self.is_walkable(side_a, grid, cell_lookup)
            and self.is_walkable(side_b, grid, cell_lookup)
        )

    def is_walkable(
        self,
        cell: tuple[int, int],
        grid: GridMap,
        cell_lookup: dict[tuple[int, int], GridCell] | None = None,
    ) -> bool:
        x, y = cell
        if x < 0 or y < 0:
            return False
        if x >= grid.gridSizeX or y >= grid.gridSizeY:
            return False

        grid_cell = self.get_cell(cell, grid, cell_lookup)
        return grid_cell is not None and not grid_cell.blocked

    def get_cell(
        self,
        cell: tuple[int, int],
        grid: GridMap,
        cell_lookup: dict[tuple[int, int], GridCell] | None = None,
    ) -> GridCell | None:
        if cell_lookup is not None:
            return cell_lookup.get(cell)

        x, y = cell
        for grid_cell in grid.cells:
            if grid_cell.x == x and grid_cell.y == y:
                return grid_cell
        return None

    def get_cell_cost(
        self,
        cell: tuple[int, int],
        grid: GridMap,
        cell_lookup: dict[tuple[int, int], GridCell] | None = None,
        obstacle_soft_costs: dict[tuple[int, int], float] | None = None,
    ) -> float:
        grid_cell = self.get_cell(cell, grid, cell_lookup)
        if grid_cell is None:
            return 999999.0

        base_cost = max(1.0, grid_cell.cost)
        soft_cost = 0.0 if obstacle_soft_costs is None else obstacle_soft_costs.get(cell, 0.0)
        return base_cost + soft_cost

    def get_move_cost(
        self,
        from_cell: tuple[int, int],
        to_cell: tuple[int, int],
        grid: GridMap,
        cell_lookup: dict[tuple[int, int], GridCell] | None = None,
        obstacle_soft_costs: dict[tuple[int, int], float] | None = None,
    ) -> float:
        cost = self.get_cell_cost(to_cell, grid, cell_lookup, obstacle_soft_costs)
        direction = self.get_move_direction(from_cell, to_cell)
        if self.is_diagonal_direction(direction):
            return cost * math.sqrt(2.0)

        return cost

    def make_path_state(
        self,
        cell: tuple[int, int],
        direction: tuple[int, int],
    ) -> tuple[int, int, int, int]:
        return cell[0], cell[1], direction[0], direction[1]

    def get_cell_from_path_state(self, state: tuple[int, int, int, int]) -> tuple[int, int]:
        return state[0], state[1]

    def get_direction_from_path_state(self, state: tuple[int, int, int, int]) -> tuple[int, int]:
        return state[2], state[3]

    def get_move_direction(
        self,
        from_cell: tuple[int, int],
        to_cell: tuple[int, int],
    ) -> tuple[int, int]:
        return to_cell[0] - from_cell[0], to_cell[1] - from_cell[1]

    def is_diagonal_direction(self, direction: tuple[int, int]) -> bool:
        return direction[0] != 0 and direction[1] != 0

    def get_turn_cost(
        self,
        previous_direction: tuple[int, int],
        next_direction: tuple[int, int],
    ) -> float:
        if self.pathTurnCostPenalty <= 0.0:
            return 0.0

        if previous_direction == (0, 0) or previous_direction == next_direction:
            return 0.0

        previous_x, previous_y = previous_direction
        next_x, next_y = next_direction
        dot = previous_x * next_x + previous_y * next_y
        previous_length = math.hypot(previous_x, previous_y)
        next_length = math.hypot(next_x, next_y)

        if previous_length <= 0.0 or next_length <= 0.0:
            return 0.0

        normalized_dot = max(-1.0, min(1.0, dot / (previous_length * next_length)))
        turn_angle_degree = math.degrees(math.acos(normalized_dot))
        return self.pathTurnCostPenalty * (turn_angle_degree / 90.0)

    def build_cell_lookup(self, grid: GridMap) -> dict[tuple[int, int], GridCell]:
        return {(grid_cell.x, grid_cell.y): grid_cell for grid_cell in grid.cells}

    def build_obstacle_soft_costs(
        self,
        grid: GridMap,
        cell_lookup: dict[tuple[int, int], GridCell],
    ) -> dict[tuple[int, int], float]:
        del cell_lookup

        if self.obstacleSoftCostRadiusM <= 0.0 or self.obstacleSoftCostMaxPenalty <= 0.0:
            return {}

        radius_cm = self.obstacleSoftCostRadiusM * 100.0
        cell_size_cm = max(grid.cellSizeCm, 1.0)
        radius_cells = max(1, math.ceil(radius_cm / cell_size_cm))
        blocked_cells = [
            (grid_cell.x, grid_cell.y)
            for grid_cell in grid.cells
            if grid_cell.blocked
        ]

        if not blocked_cells:
            return {}

        soft_costs: dict[tuple[int, int], float] = {}

        for grid_cell in grid.cells:
            if grid_cell.blocked:
                continue

            min_distance_cells = float("inf")
            for blocked_x, blocked_y in blocked_cells:
                offset_x = abs(grid_cell.x - blocked_x)
                offset_y = abs(grid_cell.y - blocked_y)
                if offset_x > radius_cells or offset_y > radius_cells:
                    continue

                distance_cells = math.hypot(offset_x, offset_y)
                if distance_cells < min_distance_cells:
                    min_distance_cells = distance_cells

            if not math.isfinite(min_distance_cells):
                continue

            distance_cm = min_distance_cells * cell_size_cm
            if distance_cm > radius_cm:
                continue

            distance_ratio = max(0.0, min(1.0, 1.0 - (distance_cm / radius_cm)))
            penalty = self.obstacleSoftCostMaxPenalty * (distance_ratio ** self.obstacleSoftCostPower)
            if penalty > 0.0:
                soft_costs[(grid_cell.x, grid_cell.y)] = penalty

        return soft_costs

    def heuristic(self, a: tuple[int, int], b: tuple[int, int]) -> float:
        delta_x = abs(a[0] - b[0])
        delta_y = abs(a[1] - b[1])

        if not self.bAllowDiagonalPathfinding:
            return delta_x + delta_y

        diagonal = min(delta_x, delta_y)
        straight = max(delta_x, delta_y) - diagonal
        return (math.sqrt(2.0) * diagonal) + straight

    def reconstruct_path(
        self,
        came_from: dict[tuple[int, int, int, int], tuple[int, int, int, int]],
        current: tuple[int, int, int, int],
    ) -> list[tuple[int, int]]:
        path = [self.get_cell_from_path_state(current)]

        while current in came_from:
            current = came_from[current]
            path.append(self.get_cell_from_path_state(current))

        path.reverse()
        return path

    def smooth_path(
        self,
        path: list[tuple[int, int]],
        grid: GridMap,
        cell_lookup: dict[tuple[int, int], GridCell],
    ) -> list[tuple[int, int]]:
        if not self.bSmoothPathWithLineOfSight or len(path) < 3:
            return path

        smoothed_path = [path[0]]
        anchor_index = 0

        while anchor_index < len(path) - 1:
            next_index = len(path) - 1
            while next_index > anchor_index + 1:
                if self.has_line_of_sight(path[anchor_index], path[next_index], grid, cell_lookup):
                    break
                next_index -= 1

            smoothed_path.append(path[next_index])
            anchor_index = next_index

        return smoothed_path

    def has_line_of_sight(
        self,
        from_cell: tuple[int, int],
        to_cell: tuple[int, int],
        grid: GridMap,
        cell_lookup: dict[tuple[int, int], GridCell],
    ) -> bool:
        delta_x = to_cell[0] - from_cell[0]
        delta_y = to_cell[1] - from_cell[1]
        steps = max(abs(delta_x), abs(delta_y)) * 4

        if steps <= 0:
            return self.is_walkable(from_cell, grid, cell_lookup)

        previous_cell = from_cell
        for index in range(steps + 1):
            alpha = index / steps
            sample_x = from_cell[0] + 0.5 + (delta_x * alpha)
            sample_y = from_cell[1] + 0.5 + (delta_y * alpha)
            sample_cell = (int(math.floor(sample_x)), int(math.floor(sample_y)))

            if sample_cell == to_cell:
                sample_cell = to_cell

            if not self.is_walkable(sample_cell, grid, cell_lookup):
                return False

            if sample_cell != previous_cell and not self.can_move_between(
                previous_cell,
                sample_cell,
                grid,
                cell_lookup,
            ):
                return False

            previous_cell = sample_cell

        return True

    def get_bool_config(self, config: dict, key: str, default_value: bool) -> bool:
        value = config.get(key, default_value)

        if isinstance(value, bool):
            return value

        if isinstance(value, (int, float)):
            return value != 0

        if isinstance(value, str):
            normalized = value.strip().lower()
            if normalized in {"true", "1", "yes", "on"}:
                return True
            if normalized in {"false", "0", "no", "off"}:
                return False

        return default_value
