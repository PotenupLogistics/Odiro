import math

from ..action import BotAction, reverse_action, stop_action
from ..contract import GridCell, GridMap, LidarRay, ScenarioDecideRequest
from ..pathfinding.astar import AStarPathfinder
from ..state import AgentState


# 전방 장애물을 grid에 반영하고 현재 위치 기준으로 경로를 다시 찾는 정책
class RePathPolicy:
    name = "RePathPolicy"

    # 재탐색 거리, 동적 장애물 반경, 후진 회복 동작 시간을 설정한다.
    def __init__(
        self,
        repath_distance_m: float = 5.0,
        stop_distance_m: float = 1.4,
        front_angle_degree: float = 25.0,
        block_radius_cells: int = 0,
        repath_cooldown_seconds: float = 0.5,
        repath_debounce_seconds: float = 3.0,
        path_corridor_half_width_m: float = 0.65,
        recovery_seconds: float = 0.8,
        recovery_speed_kmh: float = 1.2,
    ):
        self.pathfinder = AStarPathfinder()
        self.repathDistanceM = repath_distance_m
        self.stopDistanceM = stop_distance_m
        self.frontAngleDegree = front_angle_degree
        self.blockRadiusCells = block_radius_cells
        self.repathCooldownSeconds = repath_cooldown_seconds
        self.repathDebounceSeconds = repath_debounce_seconds
        self.pathCorridorHalfWidthM = path_corridor_half_width_m
        self.recoverySeconds = recovery_seconds
        self.recoverySpeedKmh = recovery_speed_kmh

    # /scenario/start의 lidarSpec 값으로 재탐색 기준을 갱신한다.
    def configure_from_start(self, request) -> None:
        lidar_spec = request.lidarSpec or {}
        control_spec = request.controlSpec or {}
        robot_spec = request.robotSpec or request.vehicleSpec or {}

        self.repathDistanceM = float(
            control_spec.get(
                "repathDistanceM",
                lidar_spec.get("repathDistanceM", lidar_spec.get("slowDownDistanceM", self.repathDistanceM)),
            )
        )
        self.stopDistanceM = float(lidar_spec.get("stopDistanceM", self.stopDistanceM))
        self.frontAngleDegree = float(lidar_spec.get("frontHalfAngleDegree", self.frontAngleDegree))
        self.blockRadiusCells = max(0, int(lidar_spec.get("blockRadiusCells", self.blockRadiusCells)))
        self.repathDebounceSeconds = max(
            0.0,
            float(control_spec.get("repathDebounceSeconds", self.repathDebounceSeconds)),
        )
        self.pathCorridorHalfWidthM = self.get_path_corridor_half_width_m(control_spec, robot_spec)
        max_reverse_speed_kmh = float(robot_spec.get("maxReverseSpeedKmh", 0.0))
        default_recovery_speed_kmh = (
            max_reverse_speed_kmh * 0.4
            if max_reverse_speed_kmh > 0.0
            else self.recoverySpeedKmh
        )
        self.recoverySpeedKmh = max(0.0, float(control_spec.get("recoverySpeedKmh", default_recovery_speed_kmh)))
        self.pathfinder.configure_from_control_spec(control_spec)

    # 전방 장애물 또는 빈 경로를 보고 재탐색과 회복 동작을 수행한다.
    def decide(
        self,
        request: ScenarioDecideRequest,
        state: AgentState,
    ) -> tuple[BotAction | None, str]:
        if request.robotState.bColliding:
            state.lastSteering = 0.0
            return stop_action(), "collision_stop"

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
        bForcedRepath = state.bRepathRequested or not state.has_path()
        bFrontObstacleRepath = False

        if self.should_repath(front_ray):
            if self.is_front_ray_on_path_corridor(request, state, front_ray):
                if self.is_repath_debounced(request, state, front_ray):
                    if not bForcedRepath:
                        return None, "repath_debounce"
                else:
                    bFrontObstacleRepath = True
            elif not bForcedRepath:
                return None, "front_obstacle_off_path_corridor"

        bNeedRepath = bForcedRepath or bFrontObstacleRepath

        if not bNeedRepath:
            return None, ""

        if not self.can_repath_now(request, state):
            return None, "repath_cooldown"

        if front_ray is not None and bFrontObstacleRepath:
            self.mark_dynamic_obstacle_cells(request, state, front_ray)

        result = self.pathfinder.find_path(
            start=request.robotState,
            goal=state.goal,
            grid=state.grid,
        )

        state.lastRepathTimeSeconds = request.runTimeSeconds
        if front_ray is not None and bFrontObstacleRepath:
            self.remember_repath_debounce(request, state, front_ray)

        if not result.success:
            state.bRepathRequested = False
            return None, result.reason

        state.path = result.path
        state.pathIndex = 0
        state.followPathWorldPoints = []
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

    def get_path_corridor_half_width_m(self, control_spec: dict, robot_spec: dict) -> float:
        if "pathCorridorHalfWidthM" in control_spec:
            return max(0.1, float(control_spec["pathCorridorHalfWidthM"]))

        body_width_cm = float(robot_spec.get("bodyWidthCm", 0.0))
        if body_width_cm > 0.0:
            return max(0.25, (body_width_cm * 0.5 / 100.0) + 0.25)

        return max(0.1, self.pathCorridorHalfWidthM)

    def is_front_ray_on_path_corridor(
        self,
        request: ScenarioDecideRequest,
        state: AgentState,
        ray: LidarRay,
    ) -> bool:
        if state.grid is None or not state.has_path():
            return True

        hit_x, hit_y = self.get_ray_hit_world_location(request, ray)
        path_points = self.get_path_world_points(state)
        if not path_points:
            return True

        start_index = min(max(state.pathIndex, 0), max(len(path_points) - 1, 0))
        active_points = path_points[start_index:]
        if not active_points:
            return True

        distance_cm = self.get_distance_to_path_cm(hit_x, hit_y, active_points)
        return distance_cm <= self.pathCorridorHalfWidthM * 100.0

    def get_path_world_points(self, state: AgentState) -> list[tuple[float, float]]:
        if state.followPathWorldPoints:
            return [
                (float(point.get("x", 0.0)), float(point.get("y", 0.0)))
                for point in state.followPathWorldPoints
            ]

        if state.grid is None:
            return []

        return [
            (
                state.grid.originCm.x + (cell_x + 0.5) * state.grid.cellSizeCm,
                state.grid.originCm.y + (cell_y + 0.5) * state.grid.cellSizeCm,
            )
            for cell_x, cell_y in state.path
        ]

    def get_distance_to_path_cm(
        self,
        x: float,
        y: float,
        path_points: list[tuple[float, float]],
    ) -> float:
        if len(path_points) == 1:
            return math.hypot(x - path_points[0][0], y - path_points[0][1])

        min_distance = float("inf")
        for index in range(len(path_points) - 1):
            min_distance = min(
                min_distance,
                self.get_point_segment_distance_cm(x, y, path_points[index], path_points[index + 1]),
            )

        return min_distance

    def get_point_segment_distance_cm(
        self,
        x: float,
        y: float,
        segment_start: tuple[float, float],
        segment_end: tuple[float, float],
    ) -> float:
        start_x, start_y = segment_start
        end_x, end_y = segment_end
        delta_x = end_x - start_x
        delta_y = end_y - start_y
        segment_length_sq = delta_x * delta_x + delta_y * delta_y

        if segment_length_sq <= 0.0001:
            return math.hypot(x - start_x, y - start_y)

        projection = ((x - start_x) * delta_x + (y - start_y) * delta_y) / segment_length_sq
        projection = max(0.0, min(1.0, projection))
        closest_x = start_x + projection * delta_x
        closest_y = start_y + projection * delta_y

        return math.hypot(x - closest_x, y - closest_y)

    def is_repath_debounced(
        self,
        request: ScenarioDecideRequest,
        state: AgentState,
        ray: LidarRay,
    ) -> bool:
        self.prune_repath_debounce(request, state)
        key = self.get_repath_debounce_key(request, state, ray)
        return state.repathDebounceUntilSeconds.get(key, -999.0) > request.runTimeSeconds

    def remember_repath_debounce(
        self,
        request: ScenarioDecideRequest,
        state: AgentState,
        ray: LidarRay,
    ) -> None:
        if self.repathDebounceSeconds <= 0.0:
            return

        key = self.get_repath_debounce_key(request, state, ray)
        state.repathDebounceUntilSeconds[key] = request.runTimeSeconds + self.repathDebounceSeconds
        state.lastRepathDebounceKey = key

    def prune_repath_debounce(self, request: ScenarioDecideRequest, state: AgentState) -> None:
        expired_keys = [
            key
            for key, until_seconds in state.repathDebounceUntilSeconds.items()
            if until_seconds <= request.runTimeSeconds
        ]

        for key in expired_keys:
            del state.repathDebounceUntilSeconds[key]

    def get_repath_debounce_key(
        self,
        request: ScenarioDecideRequest,
        state: AgentState,
        ray: LidarRay,
    ) -> str:
        actor_name = ray.actorName or ""
        if actor_name:
            return f"actor:{actor_name}"

        if state.grid is not None:
            hit_x, hit_y = self.get_ray_hit_world_location(request, ray)
            cell_x, cell_y = self.pathfinder.world_to_cell(hit_x, hit_y, state.grid)
            return f"cell:{cell_x}:{cell_y}"

        ray_index = ray.rayIndex if ray.rayIndex is not None else -1
        return f"ray:{ray_index}:{round(self.normalize_angle_degree(ray.rayYawDegree), 1)}"

    # 전방 각도 안에서 가장 가까운 hit ray를 찾는다.
    def find_nearest_front_hit_ray(self, request: ScenarioDecideRequest) -> LidarRay | None:
        front_rays = [
            ray
            for ray in request.lidarRays
            if (
                ray.hit
                and not self.is_ignored_lidar_policy_ray(ray)
                and abs(self.normalize_angle_degree(ray.rayYawDegree)) <= self.frontAngleDegree
            )
        ]

        if not front_rays:
            return None

        return min(front_rays, key=lambda ray: ray.distanceM)

    def is_ignored_lidar_policy_ray(self, ray: LidarRay) -> bool:
        actor_name = ray.actorName or ""
        actor_tags = ray.actorTags or []
        return actor_name.startswith("ScenarioGroundRegion") and len(actor_tags) == 0

    # LiDAR hit 위치 주변 cell을 동적 장애물로 막는다.
    def mark_dynamic_obstacle_cells(
        self,
        request: ScenarioDecideRequest,
        state: AgentState,
        ray: LidarRay,
    ) -> None:
        if state.grid is None or state.goal is None:
            return

        if self.mark_observed_obstacle_bounds_cells(request, state, ray):
            return

        self.mark_ray_hit_obstacle_cell(request, state, ray)

    def mark_observed_obstacle_bounds_cells(
        self,
        request: ScenarioDecideRequest,
        state: AgentState,
        ray: LidarRay,
    ) -> bool:
        b_marked_any = False

        for observed_object in self.get_repath_observed_objects(request, ray):
            b_marked_any = self.mark_observed_object_bounds_cells(
                request,
                state,
                observed_object,
            ) or b_marked_any

        return b_marked_any

    def get_repath_observed_objects(
        self,
        request: ScenarioDecideRequest,
        ray: LidarRay,
    ) -> list[dict]:
        result: list[dict] = []
        ray_actor_name = ray.actorName or ""

        for observed_object in request.observedObjects:
            if self.is_ignored_observed_object(observed_object):
                continue

            actor_name = str(observed_object.get("actorName") or "")
            b_matches_ray_actor = bool(ray_actor_name) and actor_name == ray_actor_name
            b_in_front = bool(observed_object.get("inFront", False))
            closest_distance_m = float(observed_object.get("closestDistanceM", self.repathDistanceM + 1.0))

            if not b_matches_ray_actor and (not b_in_front or closest_distance_m > self.repathDistanceM):
                continue

            result.append(observed_object)

        return result

    def mark_observed_object_bounds_cells(
        self,
        request: ScenarioDecideRequest,
        state: AgentState,
        observed_object: dict,
    ) -> bool:
        if state.grid is None or state.goal is None:
            return False

        if not bool(observed_object.get("hasBounds", False)):
            return False

        origin = observed_object.get("boundsOriginCm")
        extent = observed_object.get("boundsExtentCm")
        if not isinstance(origin, dict) or not isinstance(extent, dict):
            return False

        origin_x = float(origin.get("x", 0.0))
        origin_y = float(origin.get("y", 0.0))
        extent_x = float(extent.get("x", 0.0))
        extent_y = float(extent.get("y", 0.0))

        if extent_x <= 0.0 or extent_y <= 0.0:
            return False

        margin_cm = self.blockRadiusCells * state.grid.cellSizeCm
        min_cell = self.pathfinder.world_to_cell(origin_x - extent_x - margin_cm, origin_y - extent_y - margin_cm, state.grid)
        max_cell = self.pathfinder.world_to_cell(origin_x + extent_x + margin_cm, origin_y + extent_y + margin_cm, state.grid)

        min_x = max(0, min(min_cell[0], max_cell[0]))
        max_x = min(state.grid.gridSizeX - 1, max(min_cell[0], max_cell[0]))
        min_y = max(0, min(min_cell[1], max_cell[1]))
        max_y = min(state.grid.gridSizeY - 1, max(min_cell[1], max_cell[1]))

        robot_cell = self.pathfinder.world_to_cell(request.robotState.x, request.robotState.y, state.grid)
        goal_cell = self.pathfinder.world_to_cell(state.goal.x, state.goal.y, state.grid)
        source = str(observed_object.get("actorName") or "DynamicLidarObstacle")
        b_marked_any = False

        for cell_x in range(min_x, max_x + 1):
            for cell_y in range(min_y, max_y + 1):
                cell = (cell_x, cell_y)
                if cell == robot_cell or cell == goal_cell:
                    continue

                grid_cell = self.pathfinder.get_cell(cell, state.grid)
                if grid_cell is None:
                    continue

                self.mark_cell_blocked(grid_cell, source)
                state.dynamicBlockedCells.add(cell)
                b_marked_any = True

        return b_marked_any

    def mark_ray_hit_obstacle_cell(
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

            self.mark_cell_blocked(grid_cell, ray.actorName or "DynamicLidarObstacle")
            state.dynamicBlockedCells.add(cell)

    def is_ignored_observed_object(self, observed_object: dict) -> bool:
        actor_name = str(observed_object.get("actorName") or "")
        actor_tags = observed_object.get("actorTags") or []
        return actor_name.startswith("ScenarioGroundRegion") and len(actor_tags) == 0

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
    def mark_cell_blocked(self, grid_cell: GridCell, source: str = "DynamicLidarObstacle") -> None:
        grid_cell.blocked = True
        grid_cell.areaType = "Blocked"
        grid_cell.cost = 999999.0
        grid_cell.sourceCollisionProfile = source

    # 후진 회복 동작에서 장애물 반대 방향으로 조향값을 정한다.
    def get_recovery_steering(self, ray: LidarRay) -> float:
        return 0.0

    def normalize_angle_degree(self, angle_degree: float) -> float:
        return (angle_degree + 180.0) % 360.0 - 180.0
