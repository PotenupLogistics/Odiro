import math

from ..action import BotAction, reverse_action
from ..contract import GridCell, GridMap, LidarRay, ScenarioDecideRequest
from ..pathfinding.astar import AStarPathfinder
from ..state import AgentState


# 전방 장애물을 grid에 반영하고 현재 위치 기준으로 경로를 다시 찾는 정책
class RePathPolicy:
    name = "RePathPolicy"

    # 재탐색 거리, 동적 장애물 반경, 후진 회복 동작 시간을 설정한다.
    def __init__(
        self,
        repath_distance_m: float = 3.5,
        stop_distance_m: float = 1.6,
        front_angle_degree: float = 45.0,
        block_radius_cells: int = 0,
        repath_cooldown_seconds: float = 0.5,
        recovery_seconds: float = 0.8,
        recovery_speed_kmh: float = 1.2,
    ):
        self.pathfinder = AStarPathfinder()
        self.repathDistanceM = repath_distance_m
        self.stopDistanceM = stop_distance_m
        self.frontAngleDegree = front_angle_degree
        self.blockRadiusCells = block_radius_cells
        self.repathCooldownSeconds = repath_cooldown_seconds
        self.recoverySeconds = recovery_seconds
        self.recoverySpeedKmh = recovery_speed_kmh

    # /scenario/start의 lidarSpec 값으로 재탐색 기준을 갱신한다.
    def configure_from_start(self, request) -> None:
        lidar_spec = request.lidarSpec or {}
        control_spec = request.controlSpec or {}

        self.repathDistanceM = float(lidar_spec.get("slowDownDistanceM", self.repathDistanceM))
        self.stopDistanceM = float(lidar_spec.get("stopDistanceM", self.stopDistanceM))
        self.frontAngleDegree = float(lidar_spec.get("frontHalfAngleDegree", self.frontAngleDegree))
        self.blockRadiusCells = max(0, int(lidar_spec.get("blockRadiusCells", self.blockRadiusCells)))
        self.recoverySpeedKmh = max(0.0, float(control_spec.get("recoverySpeedKmh", self.recoverySpeedKmh)))

    # 전방 장애물 또는 빈 경로를 보고 재탐색과 회복 동작을 수행한다.
    def decide(
        self,
        request: ScenarioDecideRequest,
        state: AgentState,
    ) -> tuple[BotAction | None, str]:
        if self.is_recovery_active(request, state):
            return reverse_action(
                steering=0.0,
                speed_kmh=self.recoverySpeedKmh,
            ), "recovery_reverse"

        if state.grid is None:
            return None, "missing_grid"

        if state.goal is None:
            return None, "missing_goal"

        front_ray = self.find_nearest_front_hit_ray(request)
        bNeedRepath = state.bRepathRequested or not state.has_path() or self.should_repath(front_ray)

        if not bNeedRepath:
            return None, ""

        if not self.can_repath_now(request, state):
            return None, "repath_cooldown"

        if front_ray is not None:
            self.mark_dynamic_obstacle_cells(request, state, front_ray)

        result = self.pathfinder.find_path(
            start=request.robotState,
            goal=state.goal,
            grid=state.grid,
        )

        state.lastRepathTimeSeconds = request.runTimeSeconds

        if not result.success:
            state.bRepathRequested = False
            return None, result.reason

        state.path = result.path
        state.pathIndex = 0
        state.bRepathRequested = False
        state.repathCount += 1

        if front_ray is not None and front_ray.distanceM <= self.stopDistanceM + 0.3:
            state.recoverySteering = 0.0
            return None, "dynamic_repath_near_obstacle"

        return None, "dynamic_repath_ready"

    # 회복 후진 동작이 아직 유지되어야 하는지 확인한다.
    def is_recovery_active(self, request: ScenarioDecideRequest, state: AgentState) -> bool:
        return request.runTimeSeconds < state.recoveryUntilSeconds

    # 전방 장애물이 재탐색 거리 안에 들어왔는지 확인한다.
    def should_repath(self, front_ray: LidarRay | None) -> bool:
        if front_ray is None:
            return False

        return front_ray.distanceM <= self.repathDistanceM

    # 재탐색 쿨다운 시간이 지났는지 확인한다.
    def can_repath_now(self, request: ScenarioDecideRequest, state: AgentState) -> bool:
        elapsed_seconds = request.runTimeSeconds - state.lastRepathTimeSeconds
        return elapsed_seconds >= self.repathCooldownSeconds

    # 전방 각도 안에서 가장 가까운 hit ray를 찾는다.
    def find_nearest_front_hit_ray(self, request: ScenarioDecideRequest) -> LidarRay | None:
        front_rays = [
            ray
            for ray in request.lidarRays
            if ray.hit and abs(self.normalize_angle_degree(ray.rayYawDegree)) <= self.frontAngleDegree
        ]

        if not front_rays:
            return None

        return min(front_rays, key=lambda ray: ray.distanceM)

    # LiDAR hit 위치 주변 cell을 동적 장애물로 막는다.
    def mark_dynamic_obstacle_cells(
        self,
        request: ScenarioDecideRequest,
        state: AgentState,
        ray: LidarRay,
    ) -> None:
        if state.grid is None or state.goal is None:
            return

        hit_x, hit_y = self.get_ray_hit_world_location(request, ray)
        obstacle_cell = self.pathfinder.world_to_cell(hit_x, hit_y, state.grid)
        robot_cell = self.pathfinder.world_to_cell(request.robotState.x, request.robotState.y, state.grid)
        goal_cell = self.pathfinder.world_to_cell(state.goal.x, state.goal.y, state.grid)

        for cell in self.get_cells_around(obstacle_cell):
            if cell == robot_cell or cell == goal_cell:
                continue

            grid_cell = self.pathfinder.get_cell(cell, state.grid)
            if grid_cell is None:
                continue

            self.mark_cell_blocked(grid_cell)
            state.dynamicBlockedCells.add(cell)

    # LiDAR ray 거리와 각도로 world hit 위치를 계산한다.
    def get_ray_hit_world_location(
        self,
        request: ScenarioDecideRequest,
        ray: LidarRay,
    ) -> tuple[float, float]:
        world_yaw_degree = request.robotState.yawDegree + self.normalize_angle_degree(ray.rayYawDegree)
        world_yaw_radian = math.radians(world_yaw_degree)

        distance_cm = ray.distanceM * 100.0
        hit_x = request.robotState.x + math.cos(world_yaw_radian) * distance_cm
        hit_y = request.robotState.y + math.sin(world_yaw_radian) * distance_cm

        return hit_x, hit_y

    # 기준 cell 주변의 block radius 범위 cell 목록을 만든다.
    def get_cells_around(self, center_cell: tuple[int, int]) -> list[tuple[int, int]]:
        center_x, center_y = center_cell
        cells: list[tuple[int, int]] = []

        for offset_x in range(-self.blockRadiusCells, self.blockRadiusCells + 1):
            for offset_y in range(-self.blockRadiusCells, self.blockRadiusCells + 1):
                cells.append((center_x + offset_x, center_y + offset_y))

        return cells

    # GridCell을 막힌 동적 장애물 cell로 변경한다.
    def mark_cell_blocked(self, grid_cell: GridCell) -> None:
        grid_cell.blocked = True
        grid_cell.areaType = "Blocked"
        grid_cell.cost = 999999.0
        grid_cell.sourceCollisionProfile = "DynamicLidarObstacle"

    # 후진 회복 동작에서 장애물 반대 방향으로 조향값을 정한다.
    def get_recovery_steering(self, ray: LidarRay) -> float:
        return 0.0

    def normalize_angle_degree(self, angle_degree: float) -> float:
        return (angle_degree + 180.0) % 360.0 - 180.0
