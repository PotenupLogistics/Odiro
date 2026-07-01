import math

from ..action import BotAction, stop_action
from ..contract import GridCell, GridMap, LidarRay, ScenarioDecideRequest
from ..lidar_selector import (
    convert_lidar_ray_1d_to_policy_ray,
    convert_lidar_ray_2d_to_policy_ray,
    convert_lidar_ray_3d_to_policy_ray,
    get_policy_lidar_family,
)
from ..pathfinding.astar import AStarPathfinder
from ..state import AgentState


# 전방 장애물을 grid에 반영하고 현재 위치 기준으로 경로를 다시 찾는 정책
class RePathPolicy:
    name = "RePathPolicy"

    # 재탐색 거리, obstacle warning 거리, 동적 장애물 반경을 설정한다.
    def __init__(
        self,
        repath_distance_m: float = 5.0,
        near_object_distance_m: float = 5,
        stop_distance_m: float = 1.4,
        front_angle_degree: float = 25.0,
        block_radius_cells: int = 1,
        repath_cooldown_seconds: float = 0.5,
        repath_debounce_seconds: float = 3.0,
    ):
        self.pathfinder = AStarPathfinder()
        self.repathDistanceM = repath_distance_m
        self.nearObjectDistanceM = near_object_distance_m
        self.obstacleWarningDistanceM = near_object_distance_m
        self.stopDistanceM = stop_distance_m
        self.frontAngleDegree = front_angle_degree
        self.blockRadiusCells = block_radius_cells
        self.repathCooldownSeconds = repath_cooldown_seconds
        self.repathDebounceSeconds = repath_debounce_seconds

    # /scenario/start의 lidarSpec 값으로 재탐색 기준을 갱신한다.
    def configure_from_start(self, request) -> None:
        lidar_spec = request.lidarSpec or {}
        control_spec = request.controlSpec or {}

        self.repathDistanceM = float(
            control_spec.get(
                "repathDistanceM",
                lidar_spec.get("repathDistanceM", self.repathDistanceM),
            )
        )
        self.stopDistanceM = float(lidar_spec.get("stopDistanceM", self.stopDistanceM))
        near_object_distance_m = float(
            control_spec.get(
                "nearObjectDistanceM",
                lidar_spec.get("nearObjectDistanceM", self.nearObjectDistanceM),
            )
        )
        obstacle_warning_distance_m = float(
            lidar_spec.get(
                "obstacleWarningDistanceM",
                lidar_spec.get(
                    "obstacle_warning_distance_m",
                    lidar_spec.get(
                        "nearObstacleWarningDistanceM",
                        lidar_spec.get(
                            "near_obstacle_warning_distance_m",
                            lidar_spec.get("nearMissDistanceM", near_object_distance_m),
                        ),
                    ),
                ),
            )
        )
        self.frontAngleDegree = float(lidar_spec.get("frontHalfAngleDegree", self.frontAngleDegree))
        self.blockRadiusCells = max(0, int(lidar_spec.get("blockRadiusCells", self.blockRadiusCells)))
        self.repathDebounceSeconds = max(
            0.0,
            float(control_spec.get("repathDebounceSeconds", self.repathDebounceSeconds)),
        )
        self.obstacleWarningDistanceM = obstacle_warning_distance_m
        self.nearObjectDistanceM = self.obstacleWarningDistanceM
        self.pathfinder.configure_from_control_spec(control_spec)

    # 전방 장애물 또는 빈 경로를 보고 재탐색을 수행한다.
    def decide(
        self,
        request: ScenarioDecideRequest,
        state: AgentState,
    ) -> tuple[BotAction | None, str]:
        front_ray = self.find_nearest_front_hit_ray(request)
        bColliding = request.robotState.bColliding

        if bColliding:
            state.bRepathRequested = True
            state.recoveryUntilSeconds = 0.0
            state.recoverySteering = 0.0
            state.lastSteering = 0.0

        bForcedRepath = state.bRepathRequested or not state.has_path()

        if state.grid is None:
            self.update_repath_debug(
                state=state,
                front_ray=front_ray,
                decision="missing_grid",
                bForcedRepath=bForcedRepath,
            )
            if bColliding:
                return stop_action(), "collision_missing_grid"

            return None, "missing_grid"

        if state.goal is None:
            self.update_repath_debug(
                state=state,
                front_ray=front_ray,
                decision="missing_goal",
                bForcedRepath=bForcedRepath,
            )
            if bColliding:
                return stop_action(), "collision_missing_goal"

            return None, "missing_goal"

        bFrontObstacleRepath = False
        bFrontRayInNearObject = self.should_repath(front_ray)
        bRepathDebounced = False

        if bFrontRayInNearObject and front_ray is not None:
            bRepathDebounced = self.is_repath_debounced(request, state, front_ray)
            if bRepathDebounced:
                if not bForcedRepath:
                    self.update_repath_debug(
                        state=state,
                        front_ray=front_ray,
                        decision="repath_debounce",
                        bForcedRepath=bForcedRepath,
                        bFrontRayInNearObject=bFrontRayInNearObject,
                        bRepathDebounced=bRepathDebounced,
                    )
                    return None, "repath_debounce"

            bFrontObstacleRepath = True

        bNeedRepath = bForcedRepath or bFrontObstacleRepath

        if not bNeedRepath:
            if bColliding:
                self.update_repath_debug(
                    state=state,
                    front_ray=front_ray,
                    decision="collision_stop",
                    bForcedRepath=bForcedRepath,
                    bFrontObstacleRepath=bFrontObstacleRepath,
                    bNeedRepath=bNeedRepath,
                    bFrontRayInNearObject=bFrontRayInNearObject,
                    bRepathDebounced=bRepathDebounced,
                )
                return stop_action(), "collision_stop"

            self.update_repath_debug(
                state=state,
                front_ray=front_ray,
                decision="no_repath",
                bForcedRepath=bForcedRepath,
                bFrontObstacleRepath=bFrontObstacleRepath,
                bNeedRepath=bNeedRepath,
                bFrontRayInNearObject=bFrontRayInNearObject,
                bRepathDebounced=bRepathDebounced,
            )
            return None, ""

        bCanRepathNow = self.can_repath_now(request, state)

        if not bCanRepathNow:
            if bColliding:
                self.update_repath_debug(
                    state=state,
                    front_ray=front_ray,
                    decision="collision_repath_cooldown",
                    bForcedRepath=bForcedRepath,
                    bFrontObstacleRepath=bFrontObstacleRepath,
                    bNeedRepath=bNeedRepath,
                    bCanRepathNow=bCanRepathNow,
                    bFrontRayInNearObject=bFrontRayInNearObject,
                    bRepathDebounced=bRepathDebounced,
                )
                return stop_action(), "collision_repath_cooldown"

            if bFrontRayInNearObject:
                self.update_repath_debug(
                    state=state,
                    front_ray=front_ray,
                    decision="front_obstacle_repath_cooldown_stop",
                    bForcedRepath=bForcedRepath,
                    bFrontObstacleRepath=bFrontObstacleRepath,
                    bNeedRepath=bNeedRepath,
                    bCanRepathNow=bCanRepathNow,
                    bFrontRayInNearObject=bFrontRayInNearObject,
                    bRepathDebounced=bRepathDebounced,
                )
                state.lastSteering = 0.0
                return stop_action(), "front_obstacle_repath_cooldown_stop"

            self.update_repath_debug(
                state=state,
                front_ray=front_ray,
                decision="repath_cooldown",
                bForcedRepath=bForcedRepath,
                bFrontObstacleRepath=bFrontObstacleRepath,
                bNeedRepath=bNeedRepath,
                bCanRepathNow=bCanRepathNow,
                bFrontRayInNearObject=bFrontRayInNearObject,
                bRepathDebounced=bRepathDebounced,
            )
            return None, "repath_cooldown"

        if front_ray is not None and bFrontObstacleRepath:
            self.mark_dynamic_obstacle_cells(request, state, front_ray)

        result = self.pathfinder.find_path(
            start=request.robotState,
            goal=state.goal,
            grid=state.grid,
        )
        state.lastPathfindMetrics = result.metrics

        state.lastRepathTimeSeconds = request.runTimeSeconds
        if front_ray is not None and bFrontObstacleRepath:
            self.remember_repath_debounce(request, state, front_ray)

        if not result.success:
            state.bRepathRequested = False
            self.update_repath_debug(
                state=state,
                front_ray=front_ray,
                decision=result.reason,
                bForcedRepath=bForcedRepath,
                bFrontObstacleRepath=bFrontObstacleRepath,
                bNeedRepath=bNeedRepath,
                bCanRepathNow=bCanRepathNow,
                bFrontRayInNearObject=bFrontRayInNearObject,
                bRepathDebounced=bRepathDebounced,
            )
            if bColliding:
                return stop_action(), result.reason

            return None, result.reason

        state.path = result.path
        state.pathIndex = 0
        state.followPathWorldPoints = []
        state.bRepathRequested = False
        state.repathCount += 1
        state.recoveryUntilSeconds = 0.0
        state.recoverySteering = 0.0

        if bColliding:
            self.update_repath_debug(
                state=state,
                front_ray=front_ray,
                decision="collision_repath_ready",
                bForcedRepath=bForcedRepath,
                bFrontObstacleRepath=bFrontObstacleRepath,
                bNeedRepath=bNeedRepath,
                bCanRepathNow=bCanRepathNow,
                bFrontRayInNearObject=bFrontRayInNearObject,
                bRepathDebounced=bRepathDebounced,
            )
            return stop_action(), "collision_repath_ready"

        self.update_repath_debug(
            state=state,
            front_ray=front_ray,
            decision="dynamic_repath_ready",
            bForcedRepath=bForcedRepath,
            bFrontObstacleRepath=bFrontObstacleRepath,
            bNeedRepath=bNeedRepath,
            bCanRepathNow=bCanRepathNow,
            bFrontRayInNearObject=bFrontRayInNearObject,
            bRepathDebounced=bRepathDebounced,
        )
        return None, "dynamic_repath_ready"

    # 이번 decide에서 RePath가 본 ray와 조건 플래그를 로그용 state에 저장한다.
    def update_repath_debug(
        self,
        state: AgentState,
        front_ray: LidarRay | None,
        decision: str,
        bForcedRepath: bool = False,
        bFrontObstacleRepath: bool = False,
        bNeedRepath: bool = False,
        bCanRepathNow: bool = False,
        bFrontRayInNearObject: bool = False,
        bRepathDebounced: bool = False,
    ) -> None:
        state.repathDecision = decision
        state.bRepathForced = bForcedRepath
        state.bRepathFrontObstacle = bFrontObstacleRepath
        state.bRepathNeeded = bNeedRepath
        state.bRepathCanRunNow = bCanRepathNow
        state.bRepathFrontRayExists = front_ray is not None
        state.bRepathFrontRayInNearObject = bFrontRayInNearObject
        state.bRepathDebounced = bRepathDebounced
        state.repathNearObjectDistanceM = self.nearObjectDistanceM
        state.repathFrontAngleDegree = self.frontAngleDegree
        state.repathBlockRadiusCells = self.blockRadiusCells

        if front_ray is None:
            state.repathFrontRayDistanceM = None
            state.repathFrontRayYawDegree = None
            state.repathFrontRayActorName = ""
            state.repathDistanceGapM = None
            return

        state.repathFrontRayDistanceM = front_ray.distanceM
        state.repathFrontRayYawDegree = self.normalize_angle_degree(front_ray.rayYawDegree)
        state.repathFrontRayActorName = front_ray.actorName or ""
        state.repathDistanceGapM = front_ray.distanceM - self.nearObjectDistanceM

    # 전방 ray가 obstacle warning 거리 안에 들어왔는지 확인한다.
    def should_repath(self, front_ray: LidarRay | None) -> bool:
        if front_ray is None:
            return False

        return front_ray.distanceM <= self.obstacleWarningDistanceM

    # 재탐색 쿨다운 시간이 지났는지 확인한다.
    def can_repath_now(self, request: ScenarioDecideRequest, state: AgentState) -> bool:
        elapsed_seconds = request.runTimeSeconds - state.lastRepathTimeSeconds
        return elapsed_seconds >= self.repathCooldownSeconds

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
        lidar_rays = self.get_repath_candidate_lidar_rays(request)
        front_rays = [
            ray
            for ray in lidar_rays
            if (
                ray.hit
                and not self.is_ignored_lidar_policy_ray(ray)
                and abs(self.normalize_angle_degree(ray.rayYawDegree)) <= self.frontAngleDegree
            )
        ]

        if not front_rays:
            return None

        return min(front_rays, key=lambda ray: ray.distanceM)

    # RePath는 선택된 LiDAR 차원의 raw hit ray를 직접 검사한다.
    def get_repath_candidate_lidar_rays(self, request: ScenarioDecideRequest) -> list[LidarRay]:
        family = get_policy_lidar_family(request)

        if family == "1d":
            return [
                convert_lidar_ray_1d_to_policy_ray(ray)
                for ray in request.lidar.rays1d
            ]

        if family == "2d":
            return [
                convert_lidar_ray_2d_to_policy_ray(ray)
                for ray in request.lidar.rays2d
            ]

        if family == "3d":
            return [
                convert_lidar_ray_3d_to_policy_ray(ray)
                for ray in request.lidar.rays3d
            ]

        if family == "legacy2d":
            return list(request.lidarRays)

        return []

    # blocksPolicy가 false인 hit과 정적 맵 geometry는 RePath 장애물 후보에서 제외한다.
    def is_ignored_lidar_policy_ray(self, ray: LidarRay) -> bool:
        if not ray.blocksPolicy:
            return True

        actor_name = ray.actorName or ""
        actor_tags = ray.actorTags or []
        return (
            self.is_static_map_actor_name(actor_name)
            or self.has_static_map_tags(actor_tags)
        )

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
            closest_distance_m = float(observed_object.get("closestDistanceM", self.nearObjectDistanceM + 1.0))

            if not b_matches_ray_actor and (not b_in_front or closest_distance_m > self.nearObjectDistanceM):
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

    # blocksPolicy가 false인 object와 정적 맵 geometry는 RePath bounds marking에서 제외한다.
    def is_ignored_observed_object(self, observed_object: dict) -> bool:
        if not bool(observed_object.get("blocksPolicy", observed_object.get("blocks_policy", True))):
            return True

        actor_name = str(observed_object.get("actorName") or "")
        actor_tags = list(observed_object.get("actorTags") or [])
        target_tags = list(observed_object.get("targetTags") or [])
        return (
            self.is_static_map_actor_name(actor_name)
            or self.has_static_map_tags(actor_tags + target_tags)
        )

    # 정적 맵 actor 이름이면 동적 RePath 장애물에서 제외한다.
    def is_static_map_actor_name(self, actor_name: str) -> bool:
        normalized_name = str(actor_name or "").strip().lower()
        return (
            normalized_name.startswith("scenariocorridorruntimeactor")
            or normalized_name.startswith("generated_city_")
        )

    # 정적 맵 태그이면 동적 RePath 장애물에서 제외한다.
    def has_static_map_tags(self, actor_tags: list[str]) -> bool:
        normalized_tags = {str(tag).strip().lower() for tag in actor_tags}
        return any(self.is_static_map_tag(tag) for tag in normalized_tags)

    # 정적 맵 geometry 태그인지 확인한다.
    def is_static_map_tag(self, tag: str) -> bool:
        return (
            tag == "city_block"
            or tag == "building"
            or tag == "wall"
            or tag.startswith("city_block_role_")
            or tag.endswith(".building")
            or tag.endswith(".wall")
        )

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

    def normalize_angle_degree(self, angle_degree: float) -> float:
        return (angle_degree + 180.0) % 360.0 - 180.0
