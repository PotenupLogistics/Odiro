import math

from ..action import BotAction, clamp, drive_action, soft_stop_action, stop_action
from ..contract import GridMap, RobotState, ScenarioDecideRequest
from ..state import AgentState


# 현재 경로를 로봇 위치 기준으로 따라가는 기본 주행 정책
class PathFollower:
    name = "PathFollower"

   # 경로 추종 속도, 조향 제한, 경로 이탈 허용 거리를 설정한다.
    def __init__(
        self,
        follow_speed_kmh: float = 4.5,
        waypoint_acceptance_ratio: float = 0.45,
        slow_down_distance_m: float = 4.8,
        slow_down_speed_kmh: float = 1.0,
        front_angle_degree: float = 25.0,
        stop_distance_m: float = 1.4,
        near_obstacle_warning_distance_m: float = 2.0,
        look_ahead_distance_m: float = 1.2,
        min_look_ahead_distance_m: float = 0.75,
        max_look_ahead_distance_m: float = 2.4,
        look_ahead_speed_gain_m_per_kmh: float = 0.12,
        look_ahead_steering_reduction_ratio: float = 0.45,
        look_ahead_smoothing_ratio: float = 0.35,
        path_point_acceptance_distance_m: float = 0.45,
        path_smoothing_distance_m: float = 0.35,
        steering_full_scale_degree: float = 80.0,
        steering_sensitivity: float = 1.1,
        max_steering: float = 0.5,
        max_steering_delta: float = 0.09,
        min_turn_speed_kmh: float = 0.8,
        max_path_error_m: float = 1.2,
        collision_stop_half_angle_degree: float = 8.0,
        collision_stop_distance_m: float = 0.45,
        obstacle_turn_slowdown_steering_ratio: float = 0.6,
        obstacle_turn_slowdown_max_reduction: float = 0.3,
        goal_slow_down_distance_m: float = 1.8,
        goal_approach_speed_kmh: float = 0.7,
        goal_approach_look_ahead_distance_m: float = 0.7,
        soft_stop_brake: float = 0.18,
        use_exact_goal_as_final_point: bool = True,
    ):
        self.followSpeedKmh = follow_speed_kmh
        self.waypointAcceptanceRatio = waypoint_acceptance_ratio
        self.slowDownDistanceM = slow_down_distance_m
        self.slowDownSpeedKmh = slow_down_speed_kmh
        self.frontAngleDegree = front_angle_degree
        self.stopDistanceM = stop_distance_m
        self.nearObstacleWarningDistanceM = near_obstacle_warning_distance_m
        self.lookAheadDistanceM = look_ahead_distance_m
        self.minLookAheadDistanceM = min_look_ahead_distance_m
        self.maxLookAheadDistanceM = max_look_ahead_distance_m
        self.lookAheadSpeedGainMPerKmh = look_ahead_speed_gain_m_per_kmh
        self.lookAheadSteeringReductionRatio = look_ahead_steering_reduction_ratio
        self.lookAheadSmoothingRatio = look_ahead_smoothing_ratio
        self.pathPointAcceptanceDistanceM = path_point_acceptance_distance_m
        self.pathSmoothingDistanceM = path_smoothing_distance_m
        self.steeringFullScaleDegree = steering_full_scale_degree
        self.steeringSensitivity = steering_sensitivity
        self.maxSteering = max_steering
        self.maxSteeringDelta = max_steering_delta
        self.minTurnSpeedKmh = min_turn_speed_kmh
        self.maxPathErrorM = max_path_error_m
        self.collisionStopHalfAngleDegree = collision_stop_half_angle_degree
        self.collisionStopDistanceM = collision_stop_distance_m
        self.obstacleTurnSlowdownSteeringRatio = obstacle_turn_slowdown_steering_ratio
        self.obstacleTurnSlowdownMaxReduction = obstacle_turn_slowdown_max_reduction
        self.goalSlowDownDistanceM = goal_slow_down_distance_m
        self.goalApproachSpeedKmh = goal_approach_speed_kmh
        self.goalApproachLookAheadDistanceM = goal_approach_look_ahead_distance_m
        self.softStopBrake = soft_stop_brake
        self.bUseExactGoalAsFinalPoint = use_exact_goal_as_final_point

    # /scenario/start의 spec 값으로 주행 기준을 갱신한다.
    def configure_from_start(self, request) -> None:
        lidar_spec = request.lidarSpec or {}
        control_spec = request.controlSpec or {}
        robot_spec = request.robotSpec or request.vehicleSpec or {}
        drive_spec = request.driveSpec or {}

        self.followSpeedKmh = float(control_spec.get("targetSpeedKmh", self.followSpeedKmh))
        self.maxPathErrorM = float(control_spec.get("maxPathErrorM", self.maxPathErrorM))
        self.slowDownSpeedKmh = float(control_spec.get("obstacleSlowSpeedKmh", self.slowDownSpeedKmh))
        self.lookAheadDistanceM = float(control_spec.get("lookAheadDistanceM", self.lookAheadDistanceM))
        self.minLookAheadDistanceM = float(
            control_spec.get("minLookAheadDistanceM", self.minLookAheadDistanceM)
        )
        self.maxLookAheadDistanceM = float(
            control_spec.get("maxLookAheadDistanceM", self.maxLookAheadDistanceM)
        )
        self.lookAheadSpeedGainMPerKmh = float(
            control_spec.get("lookAheadSpeedGainMPerKmh", self.lookAheadSpeedGainMPerKmh)
        )
        self.lookAheadSteeringReductionRatio = float(
            control_spec.get("lookAheadSteeringReductionRatio", self.lookAheadSteeringReductionRatio)
        )
        self.lookAheadSmoothingRatio = float(
            control_spec.get("lookAheadSmoothingRatio", self.lookAheadSmoothingRatio)
        )
        self.pathPointAcceptanceDistanceM = float(
            control_spec.get("pathPointAcceptanceDistanceM", self.pathPointAcceptanceDistanceM)
        )
        self.pathSmoothingDistanceM = float(
            control_spec.get("pathSmoothingDistanceM", self.pathSmoothingDistanceM)
        )
        self.steeringFullScaleDegree = float(
            control_spec.get("steeringFullScaleDegree", self.steeringFullScaleDegree)
        )
        self.steeringSensitivity = float(control_spec.get("steeringSensitivity", self.steeringSensitivity))
        self.maxSteering = float(control_spec.get("maxSteering", self.maxSteering))
        self.maxSteeringDelta = float(control_spec.get("maxSteeringDelta", self.maxSteeringDelta))
        self.minTurnSpeedKmh = float(control_spec.get("minTurnSpeedKmh", self.minTurnSpeedKmh))
        self.obstacleTurnSlowdownSteeringRatio = float(
            control_spec.get("obstacleTurnSlowdownSteeringRatio", self.obstacleTurnSlowdownSteeringRatio)
        )
        self.obstacleTurnSlowdownMaxReduction = float(
            control_spec.get("obstacleTurnSlowdownMaxReduction", self.obstacleTurnSlowdownMaxReduction)
        )
        self.goalSlowDownDistanceM = float(
            control_spec.get("goalSlowDownDistanceM", self.goalSlowDownDistanceM)
        )
        self.goalApproachSpeedKmh = float(
            control_spec.get("goalApproachSpeedKmh", self.goalApproachSpeedKmh)
        )
        self.goalApproachLookAheadDistanceM = float(
            control_spec.get("goalApproachLookAheadDistanceM", self.goalApproachLookAheadDistanceM)
        )
        self.softStopBrake = float(
            control_spec.get("softStopBrakeInput", drive_spec.get("stopBrakeInput", self.softStopBrake))
        )
        self.bUseExactGoalAsFinalPoint = self.get_bool_config(
            control_spec,
            "useExactGoalAsFinalPoint",
            self.bUseExactGoalAsFinalPoint,
        )
        self.stopDistanceM = float(lidar_spec.get("stopDistanceM", self.stopDistanceM))
        self.nearObstacleWarningDistanceM = float(
            lidar_spec.get(
                "nearObstacleWarningDistanceM",
                lidar_spec.get("nearMissDistanceM", self.nearObstacleWarningDistanceM),
            )
        )
        self.slowDownDistanceM = max(
            self.nearObstacleWarningDistanceM + 0.1,
            float(lidar_spec.get("slowDownDistanceM", self.slowDownDistanceM)),
        )
        self.frontAngleDegree = float(lidar_spec.get("frontHalfAngleDegree", self.frontAngleDegree))
        self.collisionStopHalfAngleDegree = float(
            lidar_spec.get("collisionStopHalfAngleDegree", self.collisionStopHalfAngleDegree)
        )
        self.collisionStopDistanceM = float(
            lidar_spec.get("collisionStopDistanceM", self.collisionStopDistanceM)
        )
        max_speed_kmh = float(robot_spec.get("maxSpeedKmh", 0.0))
        if max_speed_kmh > 0.0:
            self.followSpeedKmh = min(self.followSpeedKmh, max_speed_kmh)
        self.followSpeedKmh = max(0.0, self.followSpeedKmh)
        self.slowDownSpeedKmh = max(0.0, self.slowDownSpeedKmh)
        self.lookAheadDistanceM = max(0.1, self.lookAheadDistanceM)
        self.minLookAheadDistanceM = max(0.1, self.minLookAheadDistanceM)
        self.maxLookAheadDistanceM = max(self.maxLookAheadDistanceM, self.lookAheadDistanceM, self.minLookAheadDistanceM)
        self.lookAheadSpeedGainMPerKmh = max(0.0, self.lookAheadSpeedGainMPerKmh)
        self.lookAheadSteeringReductionRatio = clamp(self.lookAheadSteeringReductionRatio, 0.0, 0.9)
        self.lookAheadSmoothingRatio = clamp(self.lookAheadSmoothingRatio, 0.05, 1.0)
        self.pathPointAcceptanceDistanceM = max(0.0, self.pathPointAcceptanceDistanceM)
        self.pathSmoothingDistanceM = clamp(self.pathSmoothingDistanceM, 0.0, 2.0)
        self.steeringFullScaleDegree = max(1.0, self.steeringFullScaleDegree)
        self.steeringSensitivity = max(0.0, self.steeringSensitivity)
        self.maxSteering = clamp(self.maxSteering, 0.01, 1.0)
        self.maxSteeringDelta = clamp(self.maxSteeringDelta, 0.001, self.maxSteering)
        self.minTurnSpeedKmh = max(0.0, self.minTurnSpeedKmh)
        self.collisionStopHalfAngleDegree = clamp(self.collisionStopHalfAngleDegree, 0.0, self.frontAngleDegree)
        self.collisionStopDistanceM = max(0.0, self.collisionStopDistanceM)
        self.obstacleTurnSlowdownSteeringRatio = clamp(self.obstacleTurnSlowdownSteeringRatio, 0.0, 1.0)
        self.obstacleTurnSlowdownMaxReduction = clamp(self.obstacleTurnSlowdownMaxReduction, 0.0, 0.8)
        self.goalSlowDownDistanceM = max(0.1, self.goalSlowDownDistanceM)
        self.goalApproachSpeedKmh = max(0.0, self.goalApproachSpeedKmh)
        self.goalApproachLookAheadDistanceM = max(0.1, self.goalApproachLookAheadDistanceM)
        self.softStopBrake = clamp(self.softStopBrake, 0.0, 1.0)
        self.stopDistanceM = max(0.0, self.stopDistanceM)
        self.nearObstacleWarningDistanceM = max(self.stopDistanceM + 0.1, self.nearObstacleWarningDistanceM)
        self.slowDownDistanceM = max(self.nearObstacleWarningDistanceM + 0.1, self.slowDownDistanceM)
        self.frontAngleDegree = clamp(self.frontAngleDegree, 0.0, 180.0)
        
        
    # 현재 path 상태와 로봇 위치를 보고 이동 Action을 만든다.
    def decide(
        self,
        request: ScenarioDecideRequest,
        state: AgentState,
    ) -> tuple[BotAction | None, str]:
        if not state.has_path():
            state.followPathWorldPoints = []
            return stop_action(), "no_path"

        if state.grid is None:
            state.followPathWorldPoints = []
            return stop_action(), "missing_grid"

        follow_points = self.get_follow_path_points(state)
        self.update_follow_path_world_points(state, follow_points)
        state.pathIndex = min(state.pathIndex, max(len(follow_points) - 1, 0))

        if not follow_points:
            return stop_action(), "empty_follow_path"

        self.update_path_index_by_robot_location(request.robotState, state, follow_points)

        if self.is_follow_path_finished(state, follow_points):
            return self.make_goal_stop_action(state), "path_finished"

        lookahead_distance_m = self.calculate_adaptive_lookahead_distance_m(
            robot_state=request.robotState,
            state=state,
            steering=state.lastSteering,
            speed_kmh=self.get_previous_target_speed_kmh(state),
            bSmooth=False,
        )
        target_index, target_x, target_y = self.get_lookahead_target(
            request.robotState,
            state,
            follow_points,
            lookahead_distance_m,
        )
        self.update_path_tracking_debug(
            request.robotState,
            state,
            follow_points,
            lookahead_distance_m,
            target_index,
            target_x,
            target_y,
        )

        robot_grid_cell = self.get_robot_grid_cell(request.robotState, state)
        if robot_grid_cell is None:
            state.bRepathRequested = True
            state.lastSteering = 0.0
            return stop_action(), "robot_outside_grid_bounds"

        if robot_grid_cell.blocked:
            self.record_near_obstacle_warning_once(state, robot_grid_cell)

        if self.is_too_far_from_path(state):
            state.bRepathRequested = True
            state.lastSteering = 0.0
            return stop_action(), "path_deviation_repath_required"

        self.record_lidar_near_obstacle_warnings(request, state)

        speed_kmh, reason = self.get_target_speed_kmh(request, state)
        steering = self.calculate_steering(request.robotState, target_x, target_y)
        steering = self.apply_front_obstacle_avoidance(request, steering)
        preview_steering = self.preview_smooth_steering(state, steering)
        preview_speed_kmh = self.limit_speed_by_steering(speed_kmh, preview_steering)
        preview_speed_kmh = self.limit_obstacle_slowdown_speed_by_steering(
            preview_speed_kmh,
            preview_steering,
            reason,
        )

        final_lookahead_distance_m = self.calculate_adaptive_lookahead_distance_m(
            robot_state=request.robotState,
            state=state,
            steering=preview_steering,
            speed_kmh=preview_speed_kmh,
            bSmooth=True,
        )

        if abs(final_lookahead_distance_m - lookahead_distance_m) >= 0.05:
            target_index, target_x, target_y = self.get_lookahead_target(
                request.robotState,
                state,
                follow_points,
                final_lookahead_distance_m,
            )
            self.update_path_tracking_debug(
                request.robotState,
                state,
                follow_points,
                final_lookahead_distance_m,
                target_index,
                target_x,
                target_y,
            )
            steering = self.calculate_steering(request.robotState, target_x, target_y)
            steering = self.apply_front_obstacle_avoidance(request, steering)

        steering = self.smooth_steering(state, steering)
        speed_kmh = self.limit_speed_by_steering(speed_kmh, steering)
        speed_kmh = self.limit_obstacle_slowdown_speed_by_steering(speed_kmh, steering, reason)
        speed_kmh = self.limit_speed_by_goal_approach(speed_kmh, request.robotState, state)

        if reason == "front_obstacle_slowdown":
            state.slowdownCount += 1

        return drive_action(
            steering=steering,
            speed_kmh=speed_kmh,
        ), reason

    # 로봇이 다음 path cell 중심에 가까워졌을 때만 pathIndex를 증가시킨다.
    def get_follow_path_points(self, state: AgentState) -> list[tuple[float, float]]:
        if state.grid is None:
            return []

        raw_points = [self.cell_to_world_center(cell, state.grid) for cell in state.path]
        raw_points = self.with_exact_goal_point(raw_points, state)
        return self.get_smoothed_path_points(raw_points, state)

    def with_exact_goal_point(
        self,
        raw_points: list[tuple[float, float]],
        state: AgentState,
    ) -> list[tuple[float, float]]:
        if (
            not self.bUseExactGoalAsFinalPoint
            or state.goal is None
            or not state.goal.hasGoal
        ):
            return raw_points

        goal_point = (state.goal.x, state.goal.y)
        if not raw_points:
            return [goal_point]

        if self.get_distance_cm(raw_points[-1][0], raw_points[-1][1], goal_point[0], goal_point[1]) <= 1.0:
            result = list(raw_points)
            result[-1] = goal_point
            return result

        grid_cell_lookup = self.build_grid_cell_lookup(state)
        if self.is_smoothing_segment_walkable(raw_points[-1], goal_point, state, grid_cell_lookup):
            result = list(raw_points)
            result.append(goal_point)
            return result

        return raw_points

    def get_smoothed_path_points(
        self,
        raw_points: list[tuple[float, float]],
        state: AgentState,
    ) -> list[tuple[float, float]]:
        if len(raw_points) < 3 or self.pathSmoothingDistanceM <= 0.0:
            return list(raw_points)

        cut_distance_cm = self.pathSmoothingDistanceM * 100.0
        grid_cell_lookup = self.build_grid_cell_lookup(state)
        result: list[tuple[float, float]] = [raw_points[0]]

        for index in range(1, len(raw_points) - 1):
            prev_x, prev_y = raw_points[index - 1]
            current_x, current_y = raw_points[index]
            next_x, next_y = raw_points[index + 1]

            in_x = current_x - prev_x
            in_y = current_y - prev_y
            out_x = next_x - current_x
            out_y = next_y - current_y
            in_length_cm = math.hypot(in_x, in_y)
            out_length_cm = math.hypot(out_x, out_y)

            if in_length_cm <= 0.0001 or out_length_cm <= 0.0001:
                result.append((current_x, current_y))
                continue

            in_unit_x = in_x / in_length_cm
            in_unit_y = in_y / in_length_cm
            out_unit_x = out_x / out_length_cm
            out_unit_y = out_y / out_length_cm

            if self.is_straight_path_corner(in_unit_x, in_unit_y, out_unit_x, out_unit_y):
                result.append((current_x, current_y))
                continue

            corner_cut_cm = min(cut_distance_cm, in_length_cm * 0.35, out_length_cm * 0.35)
            if corner_cut_cm <= 1.0:
                result.append((current_x, current_y))
                continue

            before_corner = (
                current_x - in_unit_x * corner_cut_cm,
                current_y - in_unit_y * corner_cut_cm,
            )
            after_corner = (
                current_x + out_unit_x * corner_cut_cm,
                current_y + out_unit_y * corner_cut_cm,
            )

            if not self.is_smoothing_segment_walkable(before_corner, after_corner, state, grid_cell_lookup):
                result.append((current_x, current_y))
                continue

            result.append(before_corner)
            result.append(after_corner)

        result.append(raw_points[-1])
        return self.remove_duplicate_path_points(result)

    def build_grid_cell_lookup(self, state: AgentState):
        if state.grid is None:
            return {}

        return {
            (grid_cell.x, grid_cell.y): grid_cell
            for grid_cell in state.grid.cells
        }

    def is_straight_path_corner(
        self,
        in_unit_x: float,
        in_unit_y: float,
        out_unit_x: float,
        out_unit_y: float,
    ) -> bool:
        cross = abs(in_unit_x * out_unit_y - in_unit_y * out_unit_x)
        dot = in_unit_x * out_unit_x + in_unit_y * out_unit_y
        return cross <= 0.01 and dot > 0.0

    def is_smoothing_segment_walkable(
        self,
        start: tuple[float, float],
        end: tuple[float, float],
        state: AgentState,
        grid_cell_lookup,
    ) -> bool:
        if state.grid is None:
            return True

        start_x, start_y = start
        end_x, end_y = end
        distance_cm = self.get_distance_cm(start_x, start_y, end_x, end_y)
        step_cm = max(state.grid.cellSizeCm * 0.25, 5.0)
        sample_count = max(1, math.ceil(distance_cm / step_cm))

        for sample_index in range(sample_count + 1):
            alpha = sample_index / sample_count
            sample_x = start_x + (end_x - start_x) * alpha
            sample_y = start_y + (end_y - start_y) * alpha
            grid_cell = grid_cell_lookup.get(self.world_to_cell(sample_x, sample_y, state.grid))

            if grid_cell is None or grid_cell.blocked:
                return False

        return True

    def remove_duplicate_path_points(
        self,
        path_points: list[tuple[float, float]],
    ) -> list[tuple[float, float]]:
        result: list[tuple[float, float]] = []

        for point in path_points:
            if result and self.get_distance_cm(result[-1][0], result[-1][1], point[0], point[1]) <= 0.1:
                continue

            result.append(point)

        return result

    def update_follow_path_world_points(
        self,
        state: AgentState,
        path_points: list[tuple[float, float]],
    ) -> None:
        z = state.start.z if state.start is not None else 0.0
        state.followPathWorldPoints = [
            {
                "x": point_x,
                "y": point_y,
                "z": z,
            }
            for point_x, point_y in path_points
        ]

    def is_follow_path_finished(
        self,
        state: AgentState,
        path_points: list[tuple[float, float]],
    ) -> bool:
        return len(path_points) > 0 and state.pathIndex >= len(path_points) - 1

    def get_previous_target_speed_kmh(self, state: AgentState) -> float:
        if not state.lastAction:
            return self.followSpeedKmh

        try:
            return max(0.0, float(state.lastAction.get("targetSpeedKmh", self.followSpeedKmh)))
        except (TypeError, ValueError):
            return self.followSpeedKmh

    def calculate_adaptive_lookahead_distance_m(
        self,
        robot_state: RobotState,
        state: AgentState,
        steering: float,
        speed_kmh: float,
        bSmooth: bool,
    ) -> float:
        speed_reference_kmh = max(0.0, abs(robot_state.speedKmh), speed_kmh)
        steering_ratio = clamp(abs(steering) / max(self.maxSteering, 0.01), 0.0, 1.0)

        speed_scaled_distance_m = self.lookAheadDistanceM + (
            speed_reference_kmh * self.lookAheadSpeedGainMPerKmh
        )
        turn_scaled_distance_m = speed_scaled_distance_m * (
            1.0 - self.lookAheadSteeringReductionRatio * steering_ratio
        )
        target_distance_m = clamp(
            turn_scaled_distance_m,
            self.minLookAheadDistanceM,
            self.maxLookAheadDistanceM,
        )
        target_distance_m = self.limit_lookahead_by_goal_approach(
            target_distance_m,
            robot_state,
            state,
        )

        if not bSmooth:
            return target_distance_m

        previous_distance_m = state.currentLookAheadDistanceM
        if previous_distance_m <= 0.0:
            previous_distance_m = clamp(
                self.lookAheadDistanceM,
                self.minLookAheadDistanceM,
                self.maxLookAheadDistanceM,
            )

        smoothed_distance_m = previous_distance_m + (
            (target_distance_m - previous_distance_m) * self.lookAheadSmoothingRatio
        )
        state.currentLookAheadDistanceM = clamp(
            smoothed_distance_m,
            self.minLookAheadDistanceM,
            self.maxLookAheadDistanceM,
        )
        return state.currentLookAheadDistanceM

    def update_path_index_by_robot_location(
        self,
        robot_state: RobotState,
        state: AgentState,
        path_points: list[tuple[float, float]],
    ) -> None:
        if state.grid is None:
            return

        if len(path_points) < 2:
            state.pathIndex = 0
            return

        acceptance_cm = self.get_waypoint_acceptance_cm(state.grid)
        closest_progress = self.get_closest_path_progress(robot_state, path_points, start_index=state.pathIndex)

        if closest_progress is not None:
            closest_segment_index, closest_alpha, _ = closest_progress
            advance_index = closest_segment_index

            if closest_alpha >= 0.8 and closest_segment_index < len(path_points) - 2:
                advance_index = closest_segment_index + 1

            state.pathIndex = max(state.pathIndex, advance_index)

        state.pathIndex = min(state.pathIndex, len(path_points) - 1)

        while state.pathIndex < len(path_points) - 1:
            next_index = state.pathIndex + 1
            target_x, target_y = path_points[next_index]
            distance_cm = self.get_distance_cm(robot_state.x, robot_state.y, target_x, target_y)

            if distance_cm > acceptance_cm:
                break

            state.pathIndex = next_index

    def get_goal_distance_cm(self, robot_state: RobotState, state: AgentState) -> float:
        if state.goal is None or not state.goal.hasGoal:
            return float("inf")

        return self.get_distance_cm(robot_state.x, robot_state.y, state.goal.x, state.goal.y)

    def limit_speed_by_goal_approach(
        self,
        speed_kmh: float,
        robot_state: RobotState,
        state: AgentState,
    ) -> float:
        if speed_kmh <= 0.0 or state.goal is None or not state.goal.hasGoal:
            return speed_kmh

        distance_m = self.get_goal_distance_cm(robot_state, state) / 100.0
        stop_distance_m = 0.0
        slow_down_distance_m = max(self.goalSlowDownDistanceM, stop_distance_m + 0.1)

        if distance_m >= slow_down_distance_m:
            return speed_kmh

        if distance_m <= stop_distance_m:
            return 0.0

        distance_ratio = clamp(
            (distance_m - stop_distance_m) / max(slow_down_distance_m - stop_distance_m, 0.01),
            0.0,
            1.0,
        )
        smooth_ratio = distance_ratio * distance_ratio * (3.0 - 2.0 * distance_ratio)
        approach_speed_kmh = min(self.goalApproachSpeedKmh, speed_kmh)
        target_speed_kmh = approach_speed_kmh + ((speed_kmh - approach_speed_kmh) * smooth_ratio)

        return max(0.0, target_speed_kmh)

    def limit_lookahead_by_goal_approach(
        self,
        lookahead_distance_m: float,
        robot_state: RobotState,
        state: AgentState,
    ) -> float:
        if state.goal is None or not state.goal.hasGoal:
            return lookahead_distance_m

        distance_m = self.get_goal_distance_cm(robot_state, state) / 100.0
        stop_distance_m = 0.0
        slow_down_distance_m = max(self.goalSlowDownDistanceM, stop_distance_m + 0.1)

        if distance_m >= slow_down_distance_m:
            return lookahead_distance_m

        distance_ratio = clamp(
            (distance_m - stop_distance_m) / max(slow_down_distance_m - stop_distance_m, 0.01),
            0.0,
            1.0,
        )
        approach_lookahead_m = clamp(
            min(self.goalApproachLookAheadDistanceM, max(distance_m * 0.75, self.minLookAheadDistanceM)),
            self.minLookAheadDistanceM,
            self.maxLookAheadDistanceM,
        )
        blended_lookahead_m = approach_lookahead_m + (
            (lookahead_distance_m - approach_lookahead_m) * distance_ratio
        )

        return clamp(blended_lookahead_m, self.minLookAheadDistanceM, self.maxLookAheadDistanceM)

    def make_goal_stop_action(self, state: AgentState) -> BotAction:
        return soft_stop_action(
            brake=self.softStopBrake,
            steering=state.lastSteering * 0.5,
        )

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

    def get_lookahead_target_index(
        self,
        robot_state: RobotState,
        state: AgentState,
        path_points: list[tuple[float, float]],
        lookahead_distance_m: float,
    ) -> int:
        if state.grid is None or not path_points:
            return 0

        target_index = min(state.pathIndex + 1, len(path_points) - 1)
        lookahead_cm = max(lookahead_distance_m * 100.0, self.get_waypoint_acceptance_cm(state.grid))

        for index in range(target_index, len(path_points)):
            target_x, target_y = path_points[index]
            distance_cm = self.get_distance_cm(robot_state.x, robot_state.y, target_x, target_y)

            if distance_cm >= lookahead_cm:
                return index

        return len(path_points) - 1

    def get_lookahead_target(
        self,
        robot_state: RobotState,
        state: AgentState,
        path_points: list[tuple[float, float]],
        lookahead_distance_m: float,
    ) -> tuple[int, float, float]:
        if state.grid is None or not path_points:
            target_index = 0
            return target_index, robot_state.x, robot_state.y

        if len(path_points) == 1:
            target_x, target_y = path_points[0]
            return 0, target_x, target_y

        closest_progress = self.get_closest_path_progress(robot_state, path_points, start_index=state.pathIndex)
        if closest_progress is None:
            target_index = self.get_lookahead_target_index(
                robot_state,
                state,
                path_points,
                lookahead_distance_m,
            )
            target_x, target_y = path_points[target_index]
            return target_index, target_x, target_y

        closest_segment_index, closest_alpha, _ = closest_progress
        segment_start_x, segment_start_y = path_points[closest_segment_index]
        segment_end_x, segment_end_y = path_points[closest_segment_index + 1]
        current_x = segment_start_x + (segment_end_x - segment_start_x) * closest_alpha
        current_y = segment_start_y + (segment_end_y - segment_start_y) * closest_alpha
        remain_distance_cm = max(lookahead_distance_m * 100.0, self.get_waypoint_acceptance_cm(state.grid))

        for index in range(closest_segment_index, len(path_points) - 1):
            if index == closest_segment_index:
                start_x, start_y = current_x, current_y
            else:
                start_x, start_y = path_points[index]

            end_x, end_y = path_points[index + 1]
            segment_distance_cm = self.get_distance_cm(start_x, start_y, end_x, end_y)

            if segment_distance_cm <= 0.0001:
                continue

            if remain_distance_cm <= segment_distance_cm:
                alpha = remain_distance_cm / segment_distance_cm
                target_x = start_x + (end_x - start_x) * alpha
                target_y = start_y + (end_y - start_y) * alpha
                return index + 1, target_x, target_y

            remain_distance_cm -= segment_distance_cm

        target_index = len(path_points) - 1
        target_x, target_y = path_points[target_index]
        return target_index, target_x, target_y

    # 두 world 좌표 사이의 XY 평면 거리를 계산한다.
    def get_distance_cm(self, ax: float, ay: float, bx: float, by: float) -> float:
        return math.hypot(bx - ax, by - ay)

    # 현재 추종 목표점과 경로 이탈 거리 디버그 값을 저장한다.
    def update_path_tracking_debug(
        self,
        robot_state: RobotState,
        state: AgentState,
        path_points: list[tuple[float, float]],
        lookahead_distance_m: float,
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
        state.closestPathDistanceCm = self.get_closest_path_distance_cm(
            robot_state,
            path_points,
            start_index=state.pathIndex,
        )
        state.maxPathErrorCm = self.maxPathErrorM * 100.0
        state.currentLookAheadDistanceM = lookahead_distance_m

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

    def record_near_obstacle_warning_once(self, state: AgentState, grid_cell) -> None:
        source = f"GridCell:{grid_cell.x}:{grid_cell.y}:{grid_cell.sourceCollisionProfile}"
        if not self.record_near_obstacle_warning_source_once(state, source):
            return

        state.lastNearObstacleWarningCell = (grid_cell.x, grid_cell.y)
        state.lastNearObstacleWarningSource = grid_cell.sourceCollisionProfile

    def record_lidar_near_obstacle_warning_once(self, state: AgentState, ray) -> None:
        source = self.get_lidar_near_obstacle_warning_source(ray)
        if not self.record_near_obstacle_warning_source_once(state, source):
            return

        state.lastNearObstacleWarningCell = None
        state.lastNearObstacleWarningSource = source

    def record_lidar_near_obstacle_warnings(self, request: ScenarioDecideRequest, state: AgentState) -> None:
        for ray in request.lidarRays:
            if not self.is_lidar_near_obstacle_warning_ray(ray):
                continue

            self.record_lidar_near_obstacle_warning_once(state, ray)

    def record_near_obstacle_warning_source_once(self, state: AgentState, source: str) -> bool:
        if source in state.nearObstacleWarningRecordedSources:
            return False

        state.nearObstacleWarningRecordedSources.add(source)
        state.nearObstacleWarningCount += 1
        state.bNearObstacleWarningRecorded = True
        return True

    def get_lidar_near_obstacle_warning_source(self, ray) -> str:
        if ray.actorName:
            return ray.actorName

        if ray.rayIndex is not None:
            return f"LidarRay:{ray.rayIndex}"

        return "LidarNearObstacleWarning"

    def is_lidar_near_obstacle_warning_ray(self, ray) -> bool:
        if (
            not ray.hit
            or self.is_ignored_lidar_policy_ray(ray)
            or ray.distanceM > self.nearObstacleWarningDistanceM
        ):
            return False

        return not (ray.distanceM <= self.stopDistanceM and self.is_collision_stop_ray(ray))

    def is_ignored_lidar_policy_ray(self, ray) -> bool:
        actor_name = ray.actorName or ""
        actor_tags = ray.actorTags or []
        return actor_name.startswith("ScenarioGroundRegion") and len(actor_tags) == 0

    # 로봇과 전체 경로 선분 사이의 최소 거리를 계산한다.
    def get_closest_path_distance_cm(
        self,
        robot_state: RobotState,
        path_points: list[tuple[float, float]],
        start_index: int = 0,
    ) -> float:
        if not path_points:
            return 0.0

        if len(path_points) == 1:
            point_x, point_y = path_points[0]
            return self.get_distance_cm(robot_state.x, robot_state.y, point_x, point_y)

        closest_progress = self.get_closest_path_progress(robot_state, path_points, start_index=start_index)
        if closest_progress is None:
            return 0.0

        return closest_progress[2]

    def get_closest_path_progress(
        self,
        robot_state: RobotState,
        path_points: list[tuple[float, float]],
        start_index: int = 0,
    ) -> tuple[int, float, float] | None:
        if len(path_points) < 2:
            return None

        first_index = max(0, min(start_index, len(path_points) - 2))
        closest_segment_index = -1
        closest_alpha = 0.0
        closest_distance_cm = float("inf")

        for index in range(first_index, len(path_points) - 1):
            start_x, start_y = path_points[index]
            end_x, end_y = path_points[index + 1]

            alpha = self.get_segment_alpha_cm(
                robot_state.x,
                robot_state.y,
                start_x,
                start_y,
                end_x,
                end_y,
            )
            closest_x = start_x + (end_x - start_x) * alpha
            closest_y = start_y + (end_y - start_y) * alpha
            distance_cm = self.get_distance_cm(robot_state.x, robot_state.y, closest_x, closest_y)

            if distance_cm >= closest_distance_cm:
                continue

            closest_segment_index = index
            closest_alpha = alpha
            closest_distance_cm = min(closest_distance_cm, distance_cm)

        if closest_segment_index < 0:
            return None

        return closest_segment_index, closest_alpha, closest_distance_cm

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
        alpha = self.get_segment_alpha_cm(point_x, point_y, start_x, start_y, end_x, end_y)
        closest_x = start_x + (end_x - start_x) * alpha
        closest_y = start_y + (end_y - start_y) * alpha

        return self.get_distance_cm(point_x, point_y, closest_x, closest_y)

    def get_segment_alpha_cm(
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
            return 0.0

        return clamp(
            ((point_x - start_x) * segment_x + (point_y - start_y) * segment_y) / segment_length_sq,
            0.0,
            1.0,
        )

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
        state.lastSteering = self.preview_smooth_steering(state, target_steering)
        return state.lastSteering

    def preview_smooth_steering(self, state: AgentState, target_steering: float) -> float:
        target_steering = clamp(target_steering, -self.maxSteering, self.maxSteering)
        steering_delta = clamp(
            target_steering - state.lastSteering,
            -self.maxSteeringDelta,
            self.maxSteeringDelta,
        )

        return clamp(state.lastSteering + steering_delta, -self.maxSteering, self.maxSteering)

    # 조향이 커질수록 목표 속도를 낮춰 급회전을 줄인다.
    def limit_speed_by_steering(self, speed_kmh: float, steering: float) -> float:
        steering_ratio = abs(steering) / max(self.maxSteering, 0.01)
        speed_scale = 1.0 - 0.35 * clamp(steering_ratio, 0.0, 1.0)

        if speed_kmh <= 0.0:
            return 0.0

        return max(min(self.minTurnSpeedKmh, speed_kmh), speed_kmh * speed_scale)

    def limit_obstacle_slowdown_speed_by_steering(
        self,
        speed_kmh: float,
        steering: float,
        reason: str,
    ) -> float:
        if reason != "front_obstacle_slowdown" or speed_kmh <= 0.0:
            return speed_kmh

        steering_ratio = abs(steering) / max(self.maxSteering, 0.01)
        if steering_ratio <= self.obstacleTurnSlowdownSteeringRatio:
            return speed_kmh

        slowdown_range = max(1.0 - self.obstacleTurnSlowdownSteeringRatio, 0.01)
        turn_ratio = clamp(
            (steering_ratio - self.obstacleTurnSlowdownSteeringRatio) / slowdown_range,
            0.0,
            1.0,
        )
        speed_scale = 1.0 - (self.obstacleTurnSlowdownMaxReduction * turn_ratio)
        reduced_speed_kmh = speed_kmh * speed_scale

        return max(min(self.minTurnSpeedKmh, speed_kmh), reduced_speed_kmh)

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

        if not self.is_collision_stop_ray(front_ray) and front_ray.distanceM <= self.nearObstacleWarningDistanceM:
            self.record_lidar_near_obstacle_warning_once(state, front_ray)
            return self.followSpeedKmh, "front_obstacle_near_obstacle_warning_pass"

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
            if (
                ray.hit
                and not self.is_ignored_lidar_policy_ray(ray)
                and abs(self.normalize_angle_degree(ray.rayYawDegree)) <= self.frontAngleDegree
            )
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
