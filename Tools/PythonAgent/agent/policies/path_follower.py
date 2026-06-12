import math

from ..action import BotAction, clamp, drive_action, stop_action
from ..contract import GridMap, RobotState, ScenarioDecideRequest
from ..state import AgentState


# 현재 경로를 로봇 위치 기준으로 따라가는 기본 주행 정책
class PathFollower:
    name = "PathFollower"

   # 경로 추종 속도, 조향 제한, 경로 이탈 허용 거리를 설정한다.
    def __init__(
        self,
        follow_speed_kmh: float = 4.0,
        waypoint_acceptance_ratio: float = 0.45,
        slow_down_distance_m: float = 4.5,
        slow_down_speed_kmh: float = 1.2,
        front_angle_degree: float = 35.0,
        stop_distance_m: float = 1.5,
        near_miss_distance_m: float = 2.0,
        look_ahead_distance_m: float = 1.2,
        path_point_acceptance_distance_m: float = 0.45,
        steering_full_scale_degree: float = 80.0,
        steering_sensitivity: float = 1.1,
        max_steering: float = 0.5,
        max_steering_delta: float = 0.09,
        min_turn_speed_kmh: float = 0.8,
        max_path_error_m: float = 1.2,
        collision_stop_half_angle_degree: float = 8.0,
        collision_stop_distance_m: float = 0.45,
    ):
        self.followSpeedKmh = follow_speed_kmh
        self.waypointAcceptanceRatio = waypoint_acceptance_ratio
        self.slowDownDistanceM = slow_down_distance_m
        self.slowDownSpeedKmh = slow_down_speed_kmh
        self.frontAngleDegree = front_angle_degree
        self.stopDistanceM = stop_distance_m
        self.nearMissDistanceM = near_miss_distance_m
        self.lookAheadDistanceM = look_ahead_distance_m
        self.pathPointAcceptanceDistanceM = path_point_acceptance_distance_m
        self.steeringFullScaleDegree = steering_full_scale_degree
        self.steeringSensitivity = steering_sensitivity
        self.maxSteering = max_steering
        self.maxSteeringDelta = max_steering_delta
        self.minTurnSpeedKmh = min_turn_speed_kmh
        self.maxPathErrorM = max_path_error_m
        self.collisionStopHalfAngleDegree = collision_stop_half_angle_degree
        self.collisionStopDistanceM = collision_stop_distance_m

    # /scenario/start의 spec 값으로 주행 기준을 갱신한다.
    def configure_from_start(self, request) -> None:
        lidar_spec = request.lidarSpec or {}
        control_spec = request.controlSpec or {}

        self.followSpeedKmh = float(control_spec.get("targetSpeedKmh", self.followSpeedKmh))
        self.maxPathErrorM = float(control_spec.get("maxPathErrorM", self.maxPathErrorM))
        self.slowDownSpeedKmh = float(control_spec.get("obstacleSlowSpeedKmh", self.slowDownSpeedKmh))
        self.lookAheadDistanceM = float(control_spec.get("lookAheadDistanceM", self.lookAheadDistanceM))
        self.pathPointAcceptanceDistanceM = float(
            control_spec.get("pathPointAcceptanceDistanceM", self.pathPointAcceptanceDistanceM)
        )
        self.steeringFullScaleDegree = float(
            control_spec.get("steeringFullScaleDegree", self.steeringFullScaleDegree)
        )
        self.steeringSensitivity = float(control_spec.get("steeringSensitivity", self.steeringSensitivity))
        self.maxSteering = float(control_spec.get("maxSteering", self.maxSteering))
        self.maxSteeringDelta = float(control_spec.get("maxSteeringDelta", self.maxSteeringDelta))
        self.minTurnSpeedKmh = float(control_spec.get("minTurnSpeedKmh", self.minTurnSpeedKmh))
        self.collisionStopHalfAngleDegree = float(
            control_spec.get("collisionStopHalfAngleDegree", self.collisionStopHalfAngleDegree)
        )
        self.collisionStopDistanceM = float(
            control_spec.get("collisionStopDistanceM", self.collisionStopDistanceM)
        )
        self.stopDistanceM = float(lidar_spec.get("stopDistanceM", self.stopDistanceM))
        self.nearMissDistanceM = float(
            control_spec.get("nearMissDistanceM", lidar_spec.get("nearMissDistanceM", self.nearMissDistanceM))
        )
        self.slowDownDistanceM = max(
            self.nearMissDistanceM + 0.1,
            float(lidar_spec.get("slowDownDistanceM", self.slowDownDistanceM)),
        )
        self.frontAngleDegree = float(lidar_spec.get("frontHalfAngleDegree", self.frontAngleDegree))
        self.followSpeedKmh = max(0.0, self.followSpeedKmh)
        self.slowDownSpeedKmh = max(0.0, self.slowDownSpeedKmh)
        self.lookAheadDistanceM = max(0.1, self.lookAheadDistanceM)
        self.pathPointAcceptanceDistanceM = max(0.0, self.pathPointAcceptanceDistanceM)
        self.steeringFullScaleDegree = max(1.0, self.steeringFullScaleDegree)
        self.steeringSensitivity = max(0.0, self.steeringSensitivity)
        self.maxSteering = clamp(self.maxSteering, 0.01, 1.0)
        self.maxSteeringDelta = clamp(self.maxSteeringDelta, 0.001, self.maxSteering)
        self.minTurnSpeedKmh = max(0.0, self.minTurnSpeedKmh)
        self.collisionStopHalfAngleDegree = clamp(self.collisionStopHalfAngleDegree, 0.0, self.frontAngleDegree)
        self.collisionStopDistanceM = max(0.0, self.collisionStopDistanceM)
        self.stopDistanceM = max(0.0, self.stopDistanceM)
        self.nearMissDistanceM = max(self.stopDistanceM + 0.1, self.nearMissDistanceM)
        self.slowDownDistanceM = max(self.nearMissDistanceM + 0.1, self.slowDownDistanceM)
        self.frontAngleDegree = clamp(self.frontAngleDegree, 0.0, 180.0)
        
        
    # 현재 path 상태와 로봇 위치를 보고 이동 Action을 만든다.
    def decide(
        self,
        request: ScenarioDecideRequest,
        state: AgentState,
    ) -> tuple[BotAction | None, str]:
        if not state.has_path():
            return stop_action(), "no_path"

        if state.grid is None:
            return stop_action(), "missing_grid"

        if self.is_goal_reached(request.robotState, state):
            state.pathIndex = len(state.path) - 1
            return stop_action(), "goal_reached"

        self.update_path_index_by_robot_location(request.robotState, state)

        if state.is_path_finished():
            return stop_action(), "path_finished"

        target_index = self.get_lookahead_target_index(request.robotState, state)
        target_x, target_y = self.cell_to_world_center(state.path[target_index], state.grid)
        self.update_path_tracking_debug(request.robotState, state, target_index, target_x, target_y)

        robot_grid_cell = self.get_robot_grid_cell(request.robotState, state)
        if robot_grid_cell is None:
            state.bRepathRequested = True
            state.lastSteering = 0.0
            return stop_action(), "robot_outside_grid_bounds"

        if robot_grid_cell.blocked:
            self.record_near_miss_once(state, robot_grid_cell)

        if self.is_too_far_from_path(state):
            state.bRepathRequested = True
            state.lastSteering = 0.0
            return stop_action(), "path_deviation_repath_required"

        self.record_lidar_near_misses(request, state)

        steering = self.calculate_steering(request.robotState, target_x, target_y)
        steering = self.apply_front_obstacle_avoidance(request, steering)
        steering = self.smooth_steering(state, steering)

        speed_kmh, reason = self.get_target_speed_kmh(request, state)
        speed_kmh = self.limit_speed_by_steering(speed_kmh, steering)

        if reason == "front_obstacle_slowdown":
            state.slowdownCount += 1

        return drive_action(
            steering=steering,
            speed_kmh=speed_kmh,
        ), reason

    # 로봇이 다음 path cell 중심에 가까워졌을 때만 pathIndex를 증가시킨다.
    def update_path_index_by_robot_location(self, robot_state: RobotState, state: AgentState) -> None:
        if state.grid is None:
            return

        acceptance_cm = self.get_waypoint_acceptance_cm(state.grid)

        while state.pathIndex < len(state.path) - 1:
            next_index = state.pathIndex + 1
            target_x, target_y = self.cell_to_world_center(state.path[next_index], state.grid)
            distance_cm = self.get_distance_cm(robot_state.x, robot_state.y, target_x, target_y)

            if distance_cm > acceptance_cm:
                break

            state.pathIndex = next_index

    # 목표 위치에 도착했는지 goal acceptance radius로 판단한다.
    def is_goal_reached(self, robot_state: RobotState, state: AgentState) -> bool:
        if state.goal is None or not state.goal.hasGoal:
            return False

        acceptance_cm = max(state.goal.acceptanceRadiusCm, 1.0)
        distance_cm = self.get_distance_cm(robot_state.x, robot_state.y, state.goal.x, state.goal.y)

        return distance_cm <= acceptance_cm

    # grid cell 좌표를 Unreal world 중심 좌표로 변환한다.
    def cell_to_world_center(self, cell: tuple[int, int], grid: GridMap) -> tuple[float, float]:
        cell_x, cell_y = cell

        world_x = grid.originCm.x + (cell_x + 0.5) * grid.cellSizeCm
        world_y = grid.originCm.y + (cell_y + 0.5) * grid.cellSizeCm

        return world_x, world_y

    # waypoint 도착 판정에 사용할 허용 거리를 계산한다.
    def get_waypoint_acceptance_cm(self, grid: GridMap) -> float:
        if self.pathPointAcceptanceDistanceM > 0.0:
            return max(self.pathPointAcceptanceDistanceM * 100.0, 10.0)

        return max(grid.cellSizeCm * self.waypointAcceptanceRatio, 10.0)

    def get_lookahead_target_index(self, robot_state: RobotState, state: AgentState) -> int:
        if state.grid is None:
            return min(state.pathIndex + 1, len(state.path) - 1)

        target_index = min(state.pathIndex + 1, len(state.path) - 1)
        lookahead_cm = max(self.lookAheadDistanceM * 100.0, self.get_waypoint_acceptance_cm(state.grid))

        for index in range(target_index, len(state.path)):
            target_x, target_y = self.cell_to_world_center(state.path[index], state.grid)
            distance_cm = self.get_distance_cm(robot_state.x, robot_state.y, target_x, target_y)

            if distance_cm >= lookahead_cm:
                return index

        return len(state.path) - 1

    # 두 world 좌표 사이의 XY 평면 거리를 계산한다.
    def get_distance_cm(self, ax: float, ay: float, bx: float, by: float) -> float:
        return math.hypot(bx - ax, by - ay)

    # 현재 추종 목표점과 경로 이탈 거리 디버그 값을 저장한다.
    def update_path_tracking_debug(
        self,
        robot_state: RobotState,
        state: AgentState,
        target_index: int,
        target_x: float,
        target_y: float,
    ) -> None:
        z = state.start.z if state.start is not None else robot_state.z

        state.targetPathIndex = target_index
        state.targetWorldPoint = {
            "x": target_x,
            "y": target_y,
            "z": z,
        }
        state.closestPathDistanceCm = self.get_closest_path_distance_cm(robot_state, state)
        state.maxPathErrorCm = self.maxPathErrorM * 100.0

    # 로봇이 허용 거리보다 경로 선분에서 멀어졌는지 판단한다.
    def is_too_far_from_path(self, state: AgentState) -> bool:
        if state.maxPathErrorCm <= 0.0:
            return False

        return state.closestPathDistanceCm > state.maxPathErrorCm

    # 로봇이 현재 grid의 walkable cell 위에 있는지 확인한다.
    def is_robot_on_walkable_grid(self, robot_state: RobotState, state: AgentState) -> bool:
        grid_cell = self.get_robot_grid_cell(robot_state, state)
        return grid_cell is not None and not grid_cell.blocked

    def get_robot_grid_cell(self, robot_state: RobotState, state: AgentState):
        if state.grid is None:
            return None

        cell = self.world_to_cell(robot_state.x, robot_state.y, state.grid)
        return self.get_grid_cell(cell, state.grid)

    def record_near_miss_once(self, state: AgentState, grid_cell) -> None:
        source = f"GridCell:{grid_cell.x}:{grid_cell.y}:{grid_cell.sourceCollisionProfile}"
        if not self.record_near_miss_source_once(state, source):
            return

        state.lastNearMissCell = (grid_cell.x, grid_cell.y)
        state.lastNearMissSource = grid_cell.sourceCollisionProfile

    def record_lidar_near_miss_once(self, state: AgentState, ray) -> None:
        source = self.get_lidar_near_miss_source(ray)
        if not self.record_near_miss_source_once(state, source):
            return

        state.lastNearMissCell = None
        state.lastNearMissSource = source

    def record_lidar_near_misses(self, request: ScenarioDecideRequest, state: AgentState) -> None:
        for ray in request.lidarRays:
            if not self.is_lidar_near_miss_ray(ray):
                continue

            self.record_lidar_near_miss_once(state, ray)

    def record_near_miss_source_once(self, state: AgentState, source: str) -> bool:
        if source in state.nearMissRecordedSources:
            return False

        state.nearMissRecordedSources.add(source)
        state.nearMissCount += 1
        state.bNearMissRecorded = True
        return True

    def get_lidar_near_miss_source(self, ray) -> str:
        if ray.actorName:
            return ray.actorName

        if ray.rayIndex is not None:
            return f"LidarRay:{ray.rayIndex}"

        return "LidarNearMiss"

    def is_lidar_near_miss_ray(self, ray) -> bool:
        if not ray.hit or ray.distanceM > self.nearMissDistanceM:
            return False

        return not (ray.distanceM <= self.stopDistanceM and self.is_collision_stop_ray(ray))

    # 로봇과 전체 경로 선분 사이의 최소 거리를 계산한다.
    def get_closest_path_distance_cm(self, robot_state: RobotState, state: AgentState) -> float:
        if state.grid is None or not state.has_path():
            return 0.0

        if len(state.path) == 1:
            point_x, point_y = self.cell_to_world_center(state.path[0], state.grid)
            return self.get_distance_cm(robot_state.x, robot_state.y, point_x, point_y)

        closest_distance_cm = float("inf")

        for index in range(0, len(state.path) - 1):
            start_x, start_y = self.cell_to_world_center(state.path[index], state.grid)
            end_x, end_y = self.cell_to_world_center(state.path[index + 1], state.grid)

            distance_cm = self.get_distance_to_segment_cm(
                robot_state.x,
                robot_state.y,
                start_x,
                start_y,
                end_x,
                end_y,
            )
            closest_distance_cm = min(closest_distance_cm, distance_cm)

        return 0.0 if closest_distance_cm == float("inf") else closest_distance_cm

    # 점과 선분 사이의 XY 평면 최단 거리를 계산한다.
    def get_distance_to_segment_cm(
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
            return self.get_distance_cm(point_x, point_y, start_x, start_y)

        alpha = clamp(
            ((point_x - start_x) * segment_x + (point_y - start_y) * segment_y) / segment_length_sq,
            0.0,
            1.0,
        )

        closest_x = start_x + segment_x * alpha
        closest_y = start_y + segment_y * alpha

        return self.get_distance_cm(point_x, point_y, closest_x, closest_y)

    # world 좌표를 grid cell 좌표로 변환한다.
    def world_to_cell(self, x: float, y: float, grid: GridMap) -> tuple[int, int]:
        cell_x = int((x - grid.originCm.x) // grid.cellSizeCm)
        cell_y = int((y - grid.originCm.y) // grid.cellSizeCm)

        return cell_x, cell_y

    # grid cell 좌표로 GridCell을 찾는다.
    def get_grid_cell(self, cell: tuple[int, int], grid: GridMap):
        cell_x, cell_y = cell

        if cell_x < 0 or cell_y < 0:
            return None

        if cell_x >= grid.gridSizeX or cell_y >= grid.gridSizeY:
            return None

        for grid_cell in grid.cells:
            if grid_cell.x == cell_x and grid_cell.y == cell_y:
                return grid_cell

        return None

    # 목표 위치 방향을 바라보도록 조향 값을 계산한다.
    def calculate_steering(self, robot_state: RobotState, target_x: float, target_y: float) -> float:
        target_yaw_degree = math.degrees(math.atan2(target_y - robot_state.y, target_x - robot_state.x))
        yaw_error_degree = self.normalize_angle_degree(target_yaw_degree - robot_state.yawDegree)

        steering = (yaw_error_degree / max(self.steeringFullScaleDegree, 1.0)) * self.steeringSensitivity
        return clamp(steering, -self.maxSteering, self.maxSteering)

    # 조향 값이 한 번에 크게 변하지 않도록 완만하게 보간한다.
    def smooth_steering(self, state: AgentState, target_steering: float) -> float:
        target_steering = clamp(target_steering, -self.maxSteering, self.maxSteering)
        steering_delta = clamp(
            target_steering - state.lastSteering,
            -self.maxSteeringDelta,
            self.maxSteeringDelta,
        )

        state.lastSteering = clamp(state.lastSteering + steering_delta, -self.maxSteering, self.maxSteering)
        return state.lastSteering

    # 조향이 커질수록 목표 속도를 낮춰 급회전을 줄인다.
    def limit_speed_by_steering(self, speed_kmh: float, steering: float) -> float:
        steering_ratio = abs(steering) / max(self.maxSteering, 0.01)
        speed_scale = 1.0 - 0.35 * clamp(steering_ratio, 0.0, 1.0)

        if speed_kmh <= 0.0:
            return 0.0

        return max(min(self.minTurnSpeedKmh, speed_kmh), speed_kmh * speed_scale)

    # 전방 장애물의 반대 방향으로 조향 보정값을 더한다.
    def apply_front_obstacle_avoidance(self, request: ScenarioDecideRequest, steering: float) -> float:
        front_ray = self.find_nearest_front_hit_ray(request)
        if front_ray is None or front_ray.distanceM > self.slowDownDistanceM:
            return steering

        front_yaw_degree = self.normalize_angle_degree(front_ray.rayYawDegree)
        if abs(front_yaw_degree) <= 2.0:
            avoid_direction = -1.0 if steering < 0.0 else 1.0
        else:
            avoid_direction = -1.0 if front_yaw_degree > 0.0 else 1.0

        distance_ratio = clamp(1.0 - (front_ray.distanceM / self.slowDownDistanceM), 0.0, 1.0)
        avoidance_steering = avoid_direction * (0.06 + 0.18 * distance_ratio)

        return clamp(steering + avoidance_steering, -self.maxSteering, self.maxSteering)

    # 전방 장애물 거리 기준으로 목표 속도와 reason을 결정한다.
    def get_target_speed_kmh(self, request: ScenarioDecideRequest, state: AgentState) -> tuple[float, str]:
        front_ray = self.find_nearest_front_hit_ray(request)
        if front_ray is None or front_ray.distanceM > self.slowDownDistanceM:
            return self.followSpeedKmh, "follow_path"

        if not self.is_collision_stop_ray(front_ray) and front_ray.distanceM <= self.nearMissDistanceM:
            self.record_lidar_near_miss_once(state, front_ray)
            return self.followSpeedKmh, "front_obstacle_near_miss_pass"

        if front_ray.distanceM <= self.stopDistanceM:
            return 0.0, "front_obstacle_soft_stop"

        slowdown_range_m = max(self.slowDownDistanceM - self.stopDistanceM, 0.1)
        distance_ratio = clamp((front_ray.distanceM - self.stopDistanceM) / slowdown_range_m, 0.0, 1.0)
        smooth_ratio = distance_ratio * distance_ratio * (3.0 - 2.0 * distance_ratio)
        roll_speed_kmh = min(self.slowDownSpeedKmh, self.followSpeedKmh)
        target_speed_kmh = roll_speed_kmh + ((self.followSpeedKmh - roll_speed_kmh) * smooth_ratio)

        return max(0.0, target_speed_kmh), "front_obstacle_slowdown"

    # 전방 각도 안에서 가장 가까운 hit ray를 찾는다.
    def find_nearest_front_hit_ray(self, request: ScenarioDecideRequest):
        front_rays = [
            ray
            for ray in request.lidarRays
            if ray.hit and abs(self.normalize_angle_degree(ray.rayYawDegree)) <= self.frontAngleDegree
        ]

        if not front_rays:
            return None

        return min(front_rays, key=lambda ray: ray.distanceM)

    def is_collision_stop_ray(self, ray) -> bool:
        yaw_degree = abs(self.normalize_angle_degree(ray.rayYawDegree))
        return yaw_degree <= self.collisionStopHalfAngleDegree or ray.distanceM <= self.collisionStopDistanceM

    # 각도를 -180도에서 180도 사이로 정규화한다.
    def normalize_angle_degree(self, angle_degree: float) -> float:
        return (angle_degree + 180.0) % 360.0 - 180.0
