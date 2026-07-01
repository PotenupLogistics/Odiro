import math

from ..action import BotAction, clamp, soft_stop_action
from ..contract import GridCell, ScenarioDecideRequest
from ..lidar_selector import select_policy_lidar_rays_2d
from ..state import AgentState


class StopPolicy:
    name = "StopPolicy"

    def __init__(
        self,
        stop_distance_m: float = 1.4,
        front_angle_degree: float = 25.0,
        soft_stop_brake: float = 0.18,
        emergency_stop_distance_m: float = 0.45,
        emergency_brake: float = 0.45,
        obstacle_warning_distance_m: float = 2.5,
        stop_hold_seconds: float = 3.0,
        blocked_corridor_half_width_m: float = 0.75,
        blocked_corridor_extra_distance_m: float = 0.8,
        collision_stop_half_angle_degree: float = 8.0,
        collision_stop_distance_m: float = 0.45,
    ):
        self.stopDistanceM = stop_distance_m
        self.frontAngleDegree = front_angle_degree
        self.softStopBrake = soft_stop_brake
        self.emergencyStopDistanceM = emergency_stop_distance_m
        self.emergencyBrake = emergency_brake
        self.obstacleWarningDistanceM = obstacle_warning_distance_m
        self.stopHoldSeconds = stop_hold_seconds
        self.blockedCorridorHalfWidthM = blocked_corridor_half_width_m
        self.blockedCorridorExtraDistanceM = blocked_corridor_extra_distance_m
        self.collisionStopHalfAngleDegree = collision_stop_half_angle_degree
        self.collisionStopDistanceM = collision_stop_distance_m

    def configure_from_start(self, request) -> None:
        lidar_spec = request.lidarSpec or {}
        control_spec = request.controlSpec or {}
        drive_spec = request.driveSpec or {}

        self.stopDistanceM = float(lidar_spec.get("stopDistanceM", self.stopDistanceM))
        self.obstacleWarningDistanceM = float(
            lidar_spec.get(
                "obstacleWarningDistanceM",
                lidar_spec.get(
                    "obstacle_warning_distance_m",
                    lidar_spec.get(
                        "nearObstacleWarningDistanceM",
                        lidar_spec.get(
                            "near_obstacle_warning_distance_m",
                            lidar_spec.get("nearMissDistanceM", self.obstacleWarningDistanceM),
                        ),
                    ),
                ),
            )
        )
        self.frontAngleDegree = float(lidar_spec.get("frontHalfAngleDegree", self.frontAngleDegree))
        self.collisionStopHalfAngleDegree = float(
            lidar_spec.get("collisionStopHalfAngleDegree", self.collisionStopHalfAngleDegree)
        )
        self.collisionStopDistanceM = float(
            lidar_spec.get("collisionStopDistanceM", self.collisionStopDistanceM)
        )
        near_object_distance_m = float(
            control_spec.get(
                "nearObjectDistanceM",
                lidar_spec.get("nearObjectDistanceM", self.obstacleWarningDistanceM),
            )
        )
        self.obstacleWarningDistanceM = max(self.obstacleWarningDistanceM, near_object_distance_m)
        self.softStopBrake = clamp(
            float(control_spec.get("softStopBrakeInput", drive_spec.get("stopBrakeInput", self.softStopBrake))),
            0.0,
            1.0,
        )
        self.emergencyStopDistanceM = max(
            0.0,
            float(control_spec.get("emergencyStopDistanceM", self.emergencyStopDistanceM)),
        )
        self.emergencyBrake = clamp(
            float(control_spec.get("emergencyBrakeInput", self.emergencyBrake)),
            0.0,
            1.0,
        )
        self.stopHoldSeconds = max(
            0.0,
            float(control_spec.get("frontObstacleStopHoldSeconds", self.stopHoldSeconds)),
        )
        self.blockedCorridorHalfWidthM = max(
            0.1,
            float(control_spec.get("blockedCorridorHalfWidthM", self.blockedCorridorHalfWidthM)),
        )
        self.blockedCorridorExtraDistanceM = max(
            0.0,
            float(control_spec.get("blockedCorridorExtraDistanceM", self.blockedCorridorExtraDistanceM)),
        )
        self.collisionStopHalfAngleDegree = clamp(self.collisionStopHalfAngleDegree, 0.0, self.frontAngleDegree)
        self.collisionStopDistanceM = max(0.0, self.collisionStopDistanceM)
        self.obstacleWarningDistanceM = max(self.stopDistanceM + 0.1, self.obstacleWarningDistanceM)

    def decide(
        self,
        request: ScenarioDecideRequest,
        state: AgentState,
    ) -> tuple[BotAction | None, str]:
        front_ray = self.find_nearest_stop_ray(request)
        if front_ray is None:
            self.record_side_obstacle_warning_once(request, state)
            state.frontObstacleStopStartSeconds = None
            return None, ""

        state.stopCount += 1

        if state.frontObstacleStopStartSeconds is None:
            state.frontObstacleStopStartSeconds = request.runTimeSeconds

        stop_elapsed_seconds = request.runTimeSeconds - state.frontObstacleStopStartSeconds
        if stop_elapsed_seconds >= self.stopHoldSeconds:
            self.mark_blocked_forward_corridor(request, state, front_ray)
            state.frontObstacleStopStartSeconds = None
            state.recoverySteering = 0.0
            state.recoveryUntilSeconds = 0.0
            state.lastSteering = 0.0
            state.bRepathRequested = True

            return soft_stop_action(brake=self.softStopBrake, steering=0.0), "front_obstacle_repath_requested"

        brake = (
            self.emergencyBrake
            if front_ray.distanceM <= self.emergencyStopDistanceM
            else self.softStopBrake
        )
        return soft_stop_action(brake=brake, steering=state.lastSteering * 0.5), "front_obstacle_soft_stop"

    def find_nearest_stop_ray(self, request: ScenarioDecideRequest):
        lidar_rays = select_policy_lidar_rays_2d(request)
        front_rays = [
            ray
            for ray in lidar_rays
            if (
                ray.hit
                and not self.is_ignored_lidar_policy_ray(ray)
                and ray.distanceM <= self.stopDistanceM
                and self.is_collision_stop_ray(ray)
            )
        ]

        if not front_rays:
            return None

        return min(front_rays, key=lambda ray: ray.distanceM)

    def is_collision_stop_ray(self, ray) -> bool:
        yaw_degree = abs(normalize_angle_degree(ray.rayYawDegree))
        return yaw_degree <= self.collisionStopHalfAngleDegree or ray.distanceM <= self.collisionStopDistanceM

    def record_side_obstacle_warning_once(self, request: ScenarioDecideRequest, state: AgentState) -> None:
        lidar_rays = select_policy_lidar_rays_2d(request)

        for ray in lidar_rays:
            if (
                not ray.hit
                or self.is_ignored_lidar_policy_ray(ray)
                or ray.distanceM > self.obstacleWarningDistanceM
            ):
                continue

            if ray.distanceM <= self.stopDistanceM and self.is_collision_stop_ray(ray):
                continue

            source = self.get_lidar_obstacle_warning_source(ray)
            if source in state.obstacleWarningRecordedSources:
                continue

            state.obstacleWarningRecordedSources.add(source)
            state.obstacleWarningCount += 1
            state.bObstacleWarningRecorded = True
            state.lastObstacleWarningCell = None
            state.lastObstacleWarningSource = source

    def get_lidar_obstacle_warning_source(self, ray) -> str:
        if ray.actorName:
            return ray.actorName

        if ray.rayIndex is not None:
            return f"LidarRay:{ray.rayIndex}"

        return "LidarObstacleWarning"

    def is_ignored_lidar_policy_ray(self, ray) -> bool:
        if not ray.blocksPolicy:
            return True

        actor_name = ray.actorName or ""
        actor_tags = ray.actorTags or []
        normalized_tags = {str(tag).strip().lower() for tag in actor_tags}
        return (
            (actor_name.startswith("ScenarioGroundRegion") and len(actor_tags) == 0)
            or actor_name.startswith("ScenarioCorridorRuntimeActor")
            or "wall" in normalized_tags
            or any(tag.endswith(".wall") for tag in normalized_tags)
        )

    def mark_blocked_forward_corridor(self, request: ScenarioDecideRequest, state: AgentState, ray) -> None:
        if state.grid is None or state.goal is None:
            return

        start_x = request.robotState.x
        start_y = request.robotState.y
        ray_yaw_degree = normalize_angle_degree(ray.rayYawDegree)
        world_yaw_radian = math.radians(request.robotState.yawDegree + ray_yaw_degree)
        corridor_distance_cm = max(
            ray.distanceM + self.blockedCorridorExtraDistanceM,
            self.stopDistanceM,
        ) * 100.0

        end_x = start_x + math.cos(world_yaw_radian) * corridor_distance_cm
        end_y = start_y + math.sin(world_yaw_radian) * corridor_distance_cm
        half_width_cm = self.blockedCorridorHalfWidthM * 100.0

        robot_cell = self.world_to_cell(start_x, start_y, state.grid)
        goal_cell = self.world_to_cell(state.goal.x, state.goal.y, state.grid)
        blocked_cells: set[tuple[int, int]] = set()

        for grid_cell in state.grid.cells:
            cell = (grid_cell.x, grid_cell.y)
            if cell == robot_cell or cell == goal_cell:
                continue

            cell_x, cell_y = self.cell_to_world_center(grid_cell, state.grid)
            if self.distance_to_segment_cm(cell_x, cell_y, start_x, start_y, end_x, end_y) > half_width_cm:
                continue

            self.mark_cell_blocked(grid_cell)
            state.dynamicBlockedCells.add(cell)
            blocked_cells.add(cell)

        state.lastBlockedCorridorCells = blocked_cells

    def mark_cell_blocked(self, grid_cell: GridCell) -> None:
        grid_cell.blocked = True
        grid_cell.areaType = "Blocked"
        grid_cell.cost = 999999.0
        grid_cell.sourceCollisionProfile = "ForwardBlockedCorridor"

    def cell_to_world_center(self, grid_cell: GridCell, grid) -> tuple[float, float]:
        return (
            grid.originCm.x + (grid_cell.x + 0.5) * grid.cellSizeCm,
            grid.originCm.y + (grid_cell.y + 0.5) * grid.cellSizeCm,
        )

    def world_to_cell(self, x: float, y: float, grid) -> tuple[int, int]:
        return (
            int((x - grid.originCm.x) // grid.cellSizeCm),
            int((y - grid.originCm.y) // grid.cellSizeCm),
        )

    def distance_to_segment_cm(
        self,
        point_x: float,
        point_y: float,
        start_x: float,
        start_y: float,
        end_x: float,
        end_y: float,
    ) -> float:
        segment_x = end_x - start_x
        segment_y = end_y - start_y
        segment_length_sq = segment_x * segment_x + segment_y * segment_y

        if segment_length_sq <= 0.0001:
            return math.hypot(point_x - start_x, point_y - start_y)

        alpha = clamp(
            ((point_x - start_x) * segment_x + (point_y - start_y) * segment_y) / segment_length_sq,
            0.0,
            1.0,
        )
        closest_x = start_x + segment_x * alpha
        closest_y = start_y + segment_y * alpha

        return math.hypot(point_x - closest_x, point_y - closest_y)


def normalize_angle_degree(angle_degree: float) -> float:
    return (angle_degree + 180.0) % 360.0 - 180.0
