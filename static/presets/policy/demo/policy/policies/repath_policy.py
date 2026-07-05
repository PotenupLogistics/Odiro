import math

from ..action import BotAction, stop_action
from ..contract import GoalLocation, GridCell, GridMap, LidarRay, ScenarioDecideRequest
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
        stuck_rejoin_lookahead_cells: int = 2,
        stuck_rejoin_max_distance_m: float = 5.0,
        repath_local_rejoin_enabled: bool = True,
        repath_local_rejoin_lookahead_cells: int = 2,
        repath_local_rejoin_max_distance_m: float = 12.0,
        repath_local_search_margin_m: float = 3.0,
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
        self.stuckRejoinLookaheadCells = stuck_rejoin_lookahead_cells
        self.stuckRejoinMaxDistanceM = stuck_rejoin_max_distance_m
        self.bRepathLocalRejoinEnabled = repath_local_rejoin_enabled
        self.repathLocalRejoinLookaheadCells = repath_local_rejoin_lookahead_cells
        self.repathLocalRejoinMaxDistanceM = repath_local_rejoin_max_distance_m
        self.repathLocalSearchMarginM = repath_local_search_margin_m

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
        self.repathCooldownSeconds = max(
            0.0,
            float(control_spec.get("repathCooldownSeconds", self.repathCooldownSeconds)),
        )
        self.stuckRejoinLookaheadCells = max(
            1,
            int(control_spec.get("stuckRejoinLookaheadCells", self.stuckRejoinLookaheadCells)),
        )
        self.stuckRejoinMaxDistanceM = max(
            0.5,
            float(control_spec.get("stuckRejoinMaxDistanceM", self.stuckRejoinMaxDistanceM)),
        )
        self.bRepathLocalRejoinEnabled = self.pathfinder.get_bool_config(
            control_spec,
            "repathLocalRejoinEnabled",
            self.bRepathLocalRejoinEnabled,
        )
        self.repathLocalRejoinLookaheadCells = max(
            1,
            int(control_spec.get("repathLocalRejoinLookaheadCells", self.repathLocalRejoinLookaheadCells)),
        )
        self.repathLocalRejoinMaxDistanceM = max(
            1.0,
            float(control_spec.get("repathLocalRejoinMaxDistanceM", self.repathLocalRejoinMaxDistanceM)),
        )
        self.repathLocalSearchMarginM = max(
            0.5,
            float(control_spec.get("repathLocalSearchMarginM", self.repathLocalSearchMarginM)),
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

        self.ensure_grid_context(state)

        bFrontObstacleRepath = False
        bFrontRayInNearObject = self.should_repath(front_ray)
        bRepathDebounced = False
        bStuckRepath = state.bStuckRepathRequested

        if bFrontRayInNearObject and front_ray is not None:
            bRepathDebounced = self.is_repath_debounced(request, state, front_ray)
            if bRepathDebounced:
                if not bForcedRepath and not bStuckRepath:
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

        bNeedRepath = bForcedRepath or bFrontObstacleRepath or bStuckRepath

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
            if bStuckRepath:
                self.update_repath_debug(
                    state=state,
                    front_ray=front_ray,
                    decision="stuck_local_repath_cooldown_stop",
                    bForcedRepath=bForcedRepath,
                    bFrontObstacleRepath=bFrontObstacleRepath,
                    bNeedRepath=bNeedRepath,
                    bCanRepathNow=bCanRepathNow,
                    bFrontRayInNearObject=bFrontRayInNearObject,
                    bRepathDebounced=bRepathDebounced,
                )
                state.lastSteering = 0.0
                return stop_action(), "stuck_local_repath_cooldown_stop"

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
            previous_dynamic_cells = set(state.dynamicBlockedCells)
            self.mark_dynamic_obstacle_cells(request, state, front_ray)
            new_dynamic_cells = state.dynamicBlockedCells - previous_dynamic_cells
            self.pathfinder.add_blocked_cells_to_context(state.pathGridContext, new_dynamic_cells)

        if bStuckRepath:
            local_result = self.try_build_stuck_rejoin_path(request, state)
            if local_result is not None:
                local_path, local_metrics = local_result
                state.lastPathfindMetrics = local_metrics
                state.lastRepathTimeSeconds = request.runTimeSeconds
                state.path = local_path
                state.pathIndex = 0
                state.followPathWorldPoints = []
                state.bRepathRequested = False
                state.bStuckRepathRequested = False
                state.stuckStartSeconds = None
                state.repathCount += 1
                state.recoveryUntilSeconds = 0.0
                state.recoverySteering = 0.0
                state.lastSteering = 0.0
                self.update_repath_debug(
                    state=state,
                    front_ray=front_ray,
                    decision="stuck_local_repath_ready",
                    bForcedRepath=bForcedRepath,
                    bFrontObstacleRepath=bFrontObstacleRepath,
                    bNeedRepath=bNeedRepath,
                    bCanRepathNow=bCanRepathNow,
                    bFrontRayInNearObject=bFrontRayInNearObject,
                    bRepathDebounced=bRepathDebounced,
                )
                return stop_action(), "stuck_local_repath_ready"

        if bFrontObstacleRepath and self.bRepathLocalRejoinEnabled:
            local_result = self.try_build_local_rejoin_path(request, state, front_ray)
            if local_result is not None:
                local_path, local_metrics = local_result
                state.lastPathfindMetrics = local_metrics
                state.lastRepathTimeSeconds = request.runTimeSeconds
                if front_ray is not None:
                    self.remember_repath_debounce(request, state, front_ray)
                state.path = local_path
                state.pathIndex = 0
                state.followPathWorldPoints = []
                state.bRepathRequested = False
                state.bStuckRepathRequested = False
                state.stuckStartSeconds = None
                state.repathCount += 1
                state.recoveryUntilSeconds = 0.0
                state.recoverySteering = 0.0
                state.lastSteering = 0.0
                decision = "collision_local_repath_ready" if bColliding else "local_repath_ready"
                self.update_repath_debug(
                    state=state,
                    front_ray=front_ray,
                    decision=decision,
                    bForcedRepath=bForcedRepath,
                    bFrontObstacleRepath=bFrontObstacleRepath,
                    bNeedRepath=bNeedRepath,
                    bCanRepathNow=bCanRepathNow,
                    bFrontRayInNearObject=bFrontRayInNearObject,
                    bRepathDebounced=bRepathDebounced,
                )
                if bColliding:
                    return stop_action(), decision

                return None, decision

        result = self.pathfinder.find_path(
            start=request.robotState,
            goal=state.goal,
            grid=state.grid,
            grid_context=state.pathGridContext,
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
        state.bStuckRepathRequested = False
        state.stuckStartSeconds = None
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

    # 정체 상태에서는 전체 goal 대신 가까운 기존 path cell로 짧게 재진입한다.
    def ensure_grid_context(self, state: AgentState):
        if state.grid is None:
            return None

        if state.pathGridContext is None or state.pathGridContext.grid is not state.grid:
            state.pathGridContext = self.pathfinder.build_grid_context(state.grid)

        return state.pathGridContext

    def try_build_local_rejoin_path(
        self,
        request: ScenarioDecideRequest,
        state: AgentState,
        front_ray: LidarRay | None,
    ) -> tuple[list[tuple[int, int]], dict] | None:
        reference_path = self.get_rejoin_reference_path(state)
        if state.grid is None or not reference_path:
            return None

        closest_index = self.get_closest_existing_path_index(request, state, reference_path)
        if closest_index is None:
            return None

        anchor_index = self.get_local_rejoin_anchor_index(request, state, reference_path, closest_index, front_ray)
        for candidate_index in self.get_rejoin_candidate_indices(
            anchor_index,
            len(reference_path),
            self.repathLocalRejoinLookaheadCells,
        ):
            result = self.try_path_to_rejoin_candidate(
                request=request,
                state=state,
                reference_path=reference_path,
                candidate_index=candidate_index,
                max_distance_m=self.repathLocalRejoinMaxDistanceM,
                front_ray=front_ray,
            )
            if result is not None:
                return result

        return None

    def try_build_stuck_rejoin_path(
        self,
        request: ScenarioDecideRequest,
        state: AgentState,
    ) -> tuple[list[tuple[int, int]], dict] | None:
        if state.grid is None or not state.path:
            return None

        closest_index = self.get_closest_existing_path_index(request, state, state.path)
        if closest_index is None:
            return None

        for candidate_index in self.get_rejoin_candidate_indices(
            closest_index,
            len(state.path),
            self.stuckRejoinLookaheadCells,
        ):
            result = self.try_path_to_rejoin_candidate(
                request=request,
                state=state,
                reference_path=state.path,
                candidate_index=candidate_index,
                max_distance_m=self.stuckRejoinMaxDistanceM,
                front_ray=None,
            )
            if result is not None:
                return result

        return None

    # 후보 재진입 cell까지 작은 bounds 안에서 A*를 수행한다.
    def try_path_to_rejoin_candidate(
        self,
        request: ScenarioDecideRequest,
        state: AgentState,
        reference_path: list[tuple[int, int]],
        candidate_index: int,
        max_distance_m: float,
        front_ray: LidarRay | None,
    ) -> tuple[list[tuple[int, int]], dict] | None:
        if state.grid is None:
            return None

        candidate_cell = reference_path[candidate_index]
        cell_lookup = state.pathGridContext.cellLookup if state.pathGridContext is not None else None
        if not self.pathfinder.is_walkable(candidate_cell, state.grid, cell_lookup):
            return None

        candidate_x, candidate_y = self.cell_to_world_center(candidate_cell, state.grid)
        distance_m = math.hypot(
            request.robotState.x - candidate_x,
            request.robotState.y - candidate_y,
        ) / 100.0
        if distance_m > max_distance_m:
            return None

        local_goal = GoalLocation(
            hasGoal=True,
            x=candidate_x,
            y=candidate_y,
            z=state.goal.z if state.goal is not None else request.robotState.z,
        )
        search_bounds = self.build_local_search_bounds(request, state, candidate_cell, front_ray)
        result = self.pathfinder.find_path(
            start=request.robotState,
            goal=local_goal,
            grid=state.grid,
            grid_context=state.pathGridContext,
            search_bounds=search_bounds,
        )
        if not result.success or len(result.path) < 2:
            state.lastPathfindMetrics = result.metrics
            return None

        return result.path + reference_path[candidate_index + 1:], result.metrics

    def get_rejoin_reference_path(self, state: AgentState) -> list[tuple[int, int]]:
        if state.initialPath:
            return state.initialPath

        return state.path

    def get_rejoin_candidate_indices(
        self,
        closest_index: int,
        path_length: int,
        lookahead_cells: int,
    ) -> list[int]:
        candidate_indices: list[int] = []
        for offset in range(1, lookahead_cells + 1):
            candidate_index = min(closest_index + offset, path_length - 1)
            if candidate_index <= closest_index or candidate_index in candidate_indices:
                continue
            candidate_indices.append(candidate_index)

        return candidate_indices

    def get_local_rejoin_anchor_index(
        self,
        request: ScenarioDecideRequest,
        state: AgentState,
        reference_path: list[tuple[int, int]],
        closest_robot_index: int,
        front_ray: LidarRay | None,
    ) -> int:
        if state.grid is None or front_ray is None:
            return closest_robot_index

        hit_x, hit_y = self.get_ray_hit_world_location(request, front_ray)
        obstacle_cell = self.pathfinder.world_to_cell(hit_x, hit_y, state.grid)
        obstacle_index = self.get_closest_path_index_to_cell(reference_path, obstacle_cell)
        if obstacle_index is None or obstacle_index < closest_robot_index:
            return closest_robot_index

        return obstacle_index

    def get_closest_path_index_to_cell(
        self,
        path: list[tuple[int, int]],
        target_cell: tuple[int, int],
    ) -> int | None:
        if not path:
            return None

        closest_index = None
        closest_distance = float("inf")
        for index, cell in enumerate(path):
            distance = math.hypot(target_cell[0] - cell[0], target_cell[1] - cell[1])
            if distance >= closest_distance:
                continue

            closest_index = index
            closest_distance = distance

        return closest_index

    def build_local_search_bounds(
        self,
        request: ScenarioDecideRequest,
        state: AgentState,
        target_cell: tuple[int, int],
        front_ray: LidarRay | None,
    ) -> tuple[int, int, int, int] | None:
        if state.grid is None:
            return None

        robot_cell = self.pathfinder.world_to_cell(request.robotState.x, request.robotState.y, state.grid)
        bounds_cells = [robot_cell, target_cell]

        if front_ray is not None:
            hit_x, hit_y = self.get_ray_hit_world_location(request, front_ray)
            bounds_cells.append(self.pathfinder.world_to_cell(hit_x, hit_y, state.grid))

        margin_cells = max(1, math.ceil((self.repathLocalSearchMarginM * 100.0) / max(state.grid.cellSizeCm, 1.0)))
        min_x = max(0, min(cell[0] for cell in bounds_cells) - margin_cells)
        max_x = min(state.grid.gridSizeX - 1, max(cell[0] for cell in bounds_cells) + margin_cells)
        min_y = max(0, min(cell[1] for cell in bounds_cells) - margin_cells)
        max_y = min(state.grid.gridSizeY - 1, max(cell[1] for cell in bounds_cells) + margin_cells)

        return min_x, max_x, min_y, max_y

    # 현재 로봇 위치에서 가장 가까운 기존 raw path index를 찾는다.
    def get_closest_existing_path_index(
        self,
        request: ScenarioDecideRequest,
        state: AgentState,
        path: list[tuple[int, int]] | None = None,
    ) -> int | None:
        reference_path = state.path if path is None else path
        if state.grid is None or not reference_path:
            return None

        closest_index = None
        closest_distance = float("inf")
        for index, cell in enumerate(reference_path):
            cell_x, cell_y = self.cell_to_world_center(cell, state.grid)
            distance = math.hypot(request.robotState.x - cell_x, request.robotState.y - cell_y)
            if distance >= closest_distance:
                continue

            closest_index = index
            closest_distance = distance

        return closest_index

    # grid cell 중심을 정책 world 좌표로 변환한다.
    def cell_to_world_center(self, cell: tuple[int, int], grid: GridMap) -> tuple[float, float]:
        cell_x, cell_y = cell
        world_x = grid.originCm.x + (cell_x + 0.5) * grid.cellSizeCm
        world_y = grid.originCm.y + (cell_y + 0.5) * grid.cellSizeCm
        return world_x, world_y

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

    # Ground-tagged low surfaces are traversable and should not seed dynamic RePath blocks.
    def is_ground_tag(self, tag: str) -> bool:
        return tag == "ground" or tag.endswith(".ground")

    # 정적 맵 geometry 태그인지 확인한다.
    def is_static_map_tag(self, tag: str) -> bool:
        return (
            self.is_ground_tag(tag)
            or tag == "city_block"
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
