from .action import BotAction
from .contract import ScenarioDecideRequest, ScenarioEndRequest, ScenarioStartRequest
from .debug_logger import DecisionLogWatcher
from .lidar_selector import build_lidar_input_debug, select_policy_lidar_rays_2d
from .lidar_point_cloud import LidarPointCloudRecorder
from .pathfinding.astar import AStarPathfinder
from .policies.path_follower import PathFollower
from .policies.repath_policy import RePathPolicy
from .policies.stop_policy import StopPolicy
from .state import AgentState


# 전방 LiDAR 최소 거리를 로그용 문자열로 만든다.
def get_front_lidar_min_distance_text(request: ScenarioDecideRequest, front_angle_degree: float = 25.0) -> str:
    lidar_rays = select_policy_lidar_rays_2d(request)
    hit_rays = [ray for ray in lidar_rays if ray.hit and not is_ignored_lidar_policy_ray(ray)]
    front_rays = [
        ray
        for ray in hit_rays
        if abs(normalize_angle_degree(ray.rayYawDegree)) <= front_angle_degree
    ]

    if not front_rays:
        return f"none(hitRays={len(hit_rays)})"

    nearest_ray = min(front_rays, key=lambda ray: ray.distanceM)
    nearest_yaw = normalize_angle_degree(nearest_ray.rayYawDegree)
    return f"{nearest_ray.distanceM:.2f}@{nearest_yaw:.0f}deg"


# LiDAR 정책 ray가 ground hit으로 무시되어야 하는지 판단한다.
def is_ignored_lidar_policy_ray(ray) -> bool:
    if not getattr(ray, "blocksPolicy", True):
        return True

    actor_name = ray.actorName or ""
    actor_tags = ray.actorTags or []
    return actor_name.startswith("ScenarioGroundRegion") and len(actor_tags) == 0


# 각도를 -180도부터 180도 범위로 정규화한다.
def normalize_angle_degree(angle_degree: float) -> float:
    return (angle_degree + 180.0) % 360.0 - 180.0


# path cell 목록을 Unreal world 좌표 목록으로 변환한다.
def build_path_world_points(state: AgentState) -> list[dict]:
    if state.followPathWorldPoints:
        return state.followPathWorldPoints

    if state.grid is None:
        return []

    z = state.start.z if state.start is not None else 0.0
    result = []

    for cell_x, cell_y in state.path:
        result.append({
            "x": state.grid.originCm.x + (cell_x + 0.5) * state.grid.cellSizeCm,
            "y": state.grid.originCm.y + (cell_y + 0.5) * state.grid.cellSizeCm,
            "z": z,
        })

    return result


# 현재 path 길이를 debug/log 표시에 맞춰 반환한다.
def get_debug_path_length(state: AgentState) -> int:
    if state.followPathWorldPoints:
        return len(state.followPathWorldPoints)

    return len(state.path)


# LiDAR snapshot 추적 상태를 로그용 dict로 만든다.
def build_sensor_debug(request: ScenarioDecideRequest, state: AgentState) -> dict:
    return {
        "sensorSequence": request.sensorSequence,
        "sensorTimeSeconds": request.sensorTimeSeconds,
        "sensorSequenceRepeatCount": state.sensorSequenceRepeatCount,
        "sensorSequenceChangeCount": state.sensorSequenceChangeCount,
        "lastSensorDeltaSeconds": state.lastSensorDeltaSeconds,
        **build_lidar_input_debug(request),
    }


# RePath가 이번 decide에서 본 전방 ray와 판단 조건을 로그용 dict로 만든다.
def build_repath_debug(state: AgentState) -> dict:
    return {
        "repathDecision": state.repathDecision,
        "bRepathRequested": state.bRepathRequested,
        "bRepathForced": state.bRepathForced,
        "bRepathFrontObstacle": state.bRepathFrontObstacle,
        "bRepathNeeded": state.bRepathNeeded,
        "bRepathCanRunNow": state.bRepathCanRunNow,
        "bRepathFrontRayExists": state.bRepathFrontRayExists,
        "bRepathFrontRayInNearObject": state.bRepathFrontRayInNearObject,
        "bRepathDebounced": state.bRepathDebounced,
        "repathFrontRayDistanceM": state.repathFrontRayDistanceM,
        "repathFrontRayYawDegree": state.repathFrontRayYawDegree,
        "repathFrontRayActorName": state.repathFrontRayActorName,
        "repathNearObjectDistanceM": state.repathNearObjectDistanceM,
        "repathDistanceGapM": state.repathDistanceGapM,
        "repathFrontAngleDegree": state.repathFrontAngleDegree,
        "repathBlockRadiusCells": state.repathBlockRadiusCells,
    }


# 정책 선택 결과를 stable response 계약으로 만든다.
def build_decision_contract(policy_name: str, reason: str) -> dict:
    return {
        "selectedPolicy": policy_name,
        "reason": reason,
    }


# 현재 path 추적 상태를 stable response 계약으로 만든다.
def build_path_contract(state: AgentState) -> dict:
    return {
        "pathStatus": "valid" if state.has_path() else "empty",
        "pathIndex": state.pathIndex,
        "pathLength": get_debug_path_length(state),
        "targetPathIndex": state.targetPathIndex,
        "targetWorldPoint": state.targetWorldPoint,
        "pathWorldPoints": build_path_world_points(state),
    }


REPATH_EVENT_REASONS = {"dynamic_repath_ready", "collision_repath_ready"}


def build_policy_events(state: AgentState, policy_name: str, reason: str) -> list[dict]:
    if reason not in REPATH_EVENT_REASONS:
        return []

    return [
        {
            "type": "repath",
            "selectedPolicy": policy_name,
            "reason": reason,
            **build_path_contract(state),
            "closestPathDistanceCm": state.closestPathDistanceCm,
            "maxPathErrorCm": state.maxPathErrorCm,
            "obstacleWarningCount": state.obstacleWarningCount,
            "lastObstacleWarningSource": state.lastObstacleWarningSource,
            "blockedCorridorCellCount": len(state.lastBlockedCorridorCells),
            "dynamicBlockedCellCount": len(state.dynamicBlockedCells),
            **build_repath_debug(state),
        }
    ]


# decision watcher가 출력할 snapshot dict를 만든다.
def build_decision_log_snapshot(
    request: ScenarioDecideRequest,
    state: AgentState,
    policy_name: str,
    reason: str,
    action_dict: dict | None,
) -> dict:
    action_dict = action_dict or {}

    return {
        "seq": request.sequence,
        "runTimeSeconds": request.runTimeSeconds,
        "policy": policy_name,
        "reason": reason,
        "bColliding": request.robotState.bColliding,
        "collisionActorName": request.robotState.collisionActorName,
        **build_sensor_debug(request, state),
        **build_repath_debug(state),
        "frontMinM": get_front_lidar_min_distance_text(request),
        "obstacleWarning": state.obstacleWarningCount,
        "lastObstacleWarningCell": state.lastObstacleWarningCell,
        "lastObstacleWarningSource": state.lastObstacleWarningSource,
        "blockedCorridor": len(state.lastBlockedCorridorCells),
        "dynamicBlockedCellCount": len(state.dynamicBlockedCells),
        "repathDebounceKey": state.lastRepathDebounceKey,
        "repathDebounceCount": len(state.repathDebounceUntilSeconds),
        "pathIndex": state.pathIndex,
        "pathLength": get_debug_path_length(state),
        "targetPathIndex": state.targetPathIndex,
        "targetWorldPoint": state.targetWorldPoint,
        "closestPathDistanceCm": state.closestPathDistanceCm,
        "maxPathErrorCm": state.maxPathErrorCm,
        "lookAheadDistanceM": state.currentLookAheadDistanceM,
        "recoveryUntilSeconds": state.recoveryUntilSeconds,
        "direction": action_dict.get("direction"),
        "targetSpeedKmh": action_dict.get("targetSpeedKmh"),
        "steering": action_dict.get("steering"),
        "brake": action_dict.get("brake"),
    }


# PythonAgent의 전체 정책 흐름을 담당하는 클래스
class BotPolicy:

    # 사용할 정책 목록과 길찾기 객체를 준비
    def __init__(self, policies: list):
        self.decisionLogWatcher = DecisionLogWatcher()
        self.policies = policies                  # decide 때 순서대로 실행할 정책 목록
        self.pathfinder = AStarPathfinder()       # start 때 최초 경로를 만들 A* 객체
        self.pointCloudRecorder = LidarPointCloudRecorder() # point cloud capture 상태를 관리


    # /scenario/start의 spec 값을 각 정책 객체에 전달한다.
    def configure_policies_from_start(self, request: ScenarioStartRequest) -> None:
        for policy in self.policies:
            configure_from_start = getattr(policy, "configure_from_start", None)
            if callable(configure_from_start):
                configure_from_start(request)


    def start(
        self,
        request: ScenarioStartRequest,
        state: AgentState,
    ) -> dict:
        # /scenario/start 요청을 처리하고 episode 상태와 capture 설정을 초기화한다.
        state.reset_for_start(
            robot_instance_id=request.robotInstanceId,
            start=request.start,
            goal=request.goal,
            grid=request.grid,
        )

        self.decisionLogWatcher.reset()
        self.configure_policies_from_start(request)
        point_cloud_error = self.pointCloudRecorder.configure_from_start(request)
        if point_cloud_error is not None:
            return {
                "status": "error",
                "accepted": False,
                "pathStatus": "empty",
                "error": point_cloud_error,
                "decision": build_decision_contract("LidarPointCloudRecorder", "output_initialization_failed"),
                "path": build_path_contract(state),
                "events": [],
                "captures": [],
            }

        self.pathfinder.configure_from_control_spec(request.controlSpec)

        result = self.pathfinder.find_path(
            start=request.start,
            goal=request.goal,
            grid=request.grid,
        )

        if not result.success:
            return {
                "status": "error",
                "accepted": False,
                "pathStatus": "failed",
                "error": {
                    "code": "PATH_NOT_FOUND",
                    "message": result.reason,
                },
                "decision": build_decision_contract("AStarPathfinder", result.reason),
                "path": build_path_contract(state),
                "events": [],
                "captures": [],
            }

        state.path = result.path
        state.pathIndex = 0
        state.followPathWorldPoints = []

        return {
            "status": "ok",
            "accepted": True,
            "pathStatus": "valid",
            "decision": build_decision_contract("AStarPathfinder", "initial_path_ready"),
            "path": build_path_contract(state),
            "events": [],
            "captures": [],
        }

    # /scenario/decide 요청을 처리하고 action과 stable response metadata를 반환한다.
    def decide(
        self,
        request: ScenarioDecideRequest,
        state: AgentState,
    ) -> dict:
        # /scenario/decide 요청을 처리하고 stable action response를 반환한다.
        state.update_decide_time(
            sequence=request.sequence,
            run_time_seconds=request.runTimeSeconds,
        )
        state.update_sensor_snapshot(
            sensor_sequence=request.sensorSequence,
            sensor_time_seconds=request.sensorTimeSeconds,
        )

        capture_refs = self.pointCloudRecorder.capture_decide(request)
        last_reason = ""
        policy_events: list[dict] = []

        for policy in self.policies:
            action, reason = policy.decide(request, state)

            if reason:
                last_reason = reason
                policy_events.extend(build_policy_events(state, policy.name, reason))

            if action is None:
                continue

            action_dict = action.to_dict()
            state.remember_action(action_dict, reason)

            self.decisionLogWatcher.emit_if_changed(
                build_decision_log_snapshot(
                    request=request,
                    state=state,
                    policy_name=policy.name,
                    reason=reason,
                    action_dict=action_dict,
                )
            )

            return {
                "sequence": request.sequence,
                "status": "ok",
                "action": action_dict,
                "decision": build_decision_contract(policy.name, reason),
                "path": build_path_contract(state),
                "events": policy_events,
                "captures": capture_refs,
            }

        self.decisionLogWatcher.emit_if_changed(
            build_decision_log_snapshot(
                request=request,
                state=state,
                policy_name="None",
                reason=last_reason or "no_action",
                action_dict=None,
            )
        )

        return {
            "sequence": request.sequence,
            "status": "error",
            "action": None,
            "error": {
                "code": "POLICY_FAILED",
                "message": "no policy returned action",
            },
            "decision": build_decision_contract("None", last_reason or "no_action"),
            "path": build_path_contract(state),
            "events": policy_events,
            "captures": capture_refs,
        }


    def end(
        self,
        request: ScenarioEndRequest,
        state: AgentState,
    ) -> dict:
        # Episode 상태를 정리하고 최소 완료 응답을 반환한다.
        state.clear_after_end()

        return {
            "status": "ok",
            "accepted": True,
        }


# PythonAgent가 사용할 BotPolicy를 생성
# Unreal은 policy를 선택하지 않고, Python은 여기 작성된 순서대로 동작
def create_policy() -> BotPolicy:
    return BotPolicy([
        RePathPolicy(),
       # StopPolicy(),
        PathFollower()
    ])
