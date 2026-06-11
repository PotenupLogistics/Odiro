import heapq

from ..contract import GoalLocation, GridCell, GridMap, RobotState, StartLocation


# A* 결과 정보
class AStarResult:
    def __init__(self, success: bool, path: list[tuple[int, int]], reason: str):
        self.success = success              # 경로 탐색 성공 여부
        self.path = path                    # 찾은 경로 cell 목록
        self.reason = reason                # 성공 또는 실패 이유
        
        
        
# Grid 기반 A* 길찾기
class AStarPathfinder:
    # 시작 위치에서 목표 위치까지 경로 찾기
    def find_path(
        self,
        start: StartLocation | RobotState,
        goal: GoalLocation,
        grid: GridMap,
    ) -> AStarResult:
        if not goal.hasGoal:
            return AStarResult(False, [], "goal_not_available")

        start_cell = self.world_to_cell(start.x, start.y, grid)
        goal_cell = self.world_to_cell(goal.x, goal.y, grid)

        if not self.is_walkable(start_cell, grid):
            return AStarResult(False, [], "start_cell_blocked")

        if not self.is_walkable(goal_cell, grid):
            return AStarResult(False, [], "goal_cell_blocked")

        open_set: list[tuple[float, tuple[int, int]]] = []
        heapq.heappush(open_set, (0.0, start_cell))

        came_from: dict[tuple[int, int], tuple[int, int]] = {}
        g_score: dict[tuple[int, int], float] = {start_cell: 0.0}

        while open_set:
            _, current = heapq.heappop(open_set)

            if current == goal_cell:
                path = self.reconstruct_path(came_from, current)
                return AStarResult(True, path, "path_found")

            for neighbor in self.get_neighbors(current, grid):
                move_cost = self.get_cell_cost(neighbor, grid)
                next_score = g_score[current] + move_cost

                if next_score < g_score.get(neighbor, float("inf")):
                    came_from[neighbor] = current
                    g_score[neighbor] = next_score

                    priority = next_score + self.heuristic(neighbor, goal_cell)
                    heapq.heappush(open_set, (priority, neighbor))

        return AStarResult(False, [], "path_not_found")


    # Unreal world 좌표를 Grid cell 좌표로 변환
    def world_to_cell(self, x: float, y: float, grid: GridMap) -> tuple[int, int]:
        local_x = x - grid.originCm.x
        local_y = y - grid.originCm.y

        cell_x = int(local_x // grid.cellSizeCm)
        cell_y = int(local_y // grid.cellSizeCm)

        return cell_x, cell_y



    # 특정 cell에서 이동 가능한 이웃 cell 목록 가져오기
    def get_neighbors(self, cell: tuple[int, int], grid: GridMap) -> list[tuple[int, int]]:
        x, y = cell

        candidates = [
            (x + 1, y),
            (x - 1, y),
            (x, y + 1),
            (x, y - 1),
        ]
        return [candidate for candidate in candidates if self.is_walkable(candidate, grid)]
    
    
    # 해당 cell이 Grid 안에 있고 막혀 있지 않은지 확인
    def is_walkable(self, cell: tuple[int, int], grid: GridMap) -> bool:
        x, y = cell

        if x < 0 or y < 0:
            return False

        if x >= grid.gridSizeX or y >= grid.gridSizeY:
            return False

        grid_cell = self.get_cell(cell, grid)
        if grid_cell is None:
            return False

        return not grid_cell.blocked
    
    
    
    # cell 좌표로 GridCell 찾기
    def get_cell(self, cell: tuple[int, int], grid: GridMap) -> GridCell | None:
        x, y = cell

        for grid_cell in grid.cells:
            if grid_cell.x == x and grid_cell.y == y:
                return grid_cell

        return None


    # cell 이동 비용 가져오기
    def get_cell_cost(self, cell: tuple[int, int], grid: GridMap) -> float:
        grid_cell = self.get_cell(cell, grid)

        if grid_cell is None:
            return 999999.0

        return max(1.0, grid_cell.cost)


     # A* 휴리스틱. 현재는 맨해튼 거리 사용
    def heuristic(self, a: tuple[int, int], b: tuple[int, int]) -> float:
        return abs(a[0] - b[0]) + abs(a[1] - b[1])


    # came_from 기록을 따라가며 최종 path 복원
    def reconstruct_path(
        self,
        came_from: dict[tuple[int, int], tuple[int, int]],
        current: tuple[int, int],
    ) -> list[tuple[int, int]]:
        path = [current]

        while current in came_from:
            current = came_from[current]
            path.append(current)

        path.reverse()
        return path
