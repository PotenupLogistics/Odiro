from .action import BotAction
from .contract import ScenarioDecideRequest, ScenarioEndRequest, ScenarioStartRequest
from .debug_logger import DecisionLogWatcher
from .pathfinding.astar import AStarPathfinder
from .policies.path_follower import PathFollower
from .policies.repath_policy import RePathPolicy
from .policies.stop_policy import StopPolicy
from .state import AgentState


# Print front lidar minimum distance for decide debugging.
def get_front_lidar_min_distance_text(request: ScenarioDecideRequest, front_angle_degree: float = 25.0) -> str:
    hit_rays = [ray for ray in request.lidarRays if ray.hit and not is_ignored_lidar_policy_ray(ray)]
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


def is_ignored_lidar_policy_ray(ray) -> bool:
    actor_name = ray.actorName or ""
    actor_tags = ray.actorTags or []
    return actor_name.startswith("ScenarioGroundRegion") and len(actor_tags) == 0


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


def get_debug_path_length(state: AgentState) -> int:
    if state.followPathWorldPoints:
        return len(state.followPathWorldPoints)

    return len(state.path)


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
        "frontMinM": get_front_lidar_min_distance_text(request),
        "nearObstacleWarning": state.nearObstacleWarningCount,
        "lastNearObstacleWarningCell": state.lastNearObstacleWarningCell,
        "lastNearObstacleWarningSource": state.lastNearObstacleWarningSource,
        "blockedCorridor": len(state.lastBlockedCorridorCells),
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


  # /scenario/start 요청 처리
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
        state.reset_for_start(
            robot_instance_id=request.robotInstanceId,
            start=request.start,
            goal=request.goal,
            grid=request.grid,
        )

        self.decisionLogWatcher.reset()
        self.configure_policies_from_start(request)
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
                "debug": {
                    "reason": result.reason,
                },
            }

        state.path = result.path
        state.pathIndex = 0
        state.followPathWorldPoints = []

        return {
            "status": "ok",
            "accepted": True,
            "pathStatus": "valid",
            "debug": {
                "reason": "initial_path_ready",
                "pathLength": len(state.path),
            },
        }
        
        
        # /scenario/end 요청 처리
    # /scenario/decide 요청을 처리하고 action과 path debug 정보를 반환한다.
    def decide(
        self,
        request: ScenarioDecideRequest,
        state: AgentState,
    ) -> dict:
        state.update_decide_time(
            sequence=request.sequence,
            run_time_seconds=request.runTimeSeconds,
        )

        last_reason = ""

        for policy in self.policies:
            action, reason = policy.decide(request, state)

            if reason:
                last_reason = reason

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
                "debug": {
                    "selectedPolicy": policy.name,
                    "reason": reason,
                    "pathStatus": "valid" if state.has_path() else "empty",
                    "pathIndex": state.pathIndex,
                    "pathLength": get_debug_path_length(state),
                    "pathWorldPoints": build_path_world_points(state),
                    "targetPathIndex": state.targetPathIndex,
                    "targetWorldPoint": state.targetWorldPoint,
                    "closestPathDistanceCm": state.closestPathDistanceCm,
                    "maxPathErrorCm": state.maxPathErrorCm,
                    "lookAheadDistanceM": state.currentLookAheadDistanceM,
                    "nearObstacleWarningCount": state.nearObstacleWarningCount,
                    "lastNearObstacleWarningCell": state.lastNearObstacleWarningCell,
                    "lastNearObstacleWarningSource": state.lastNearObstacleWarningSource,
                    "bColliding": request.robotState.bColliding,
                    "collisionActorName": request.robotState.collisionActorName,
                    "blockedCorridorCellCount": len(state.lastBlockedCorridorCells),
                    "dynamicBlockedCellCount": len(state.dynamicBlockedCells),
                    "recoveryUntilSeconds": state.recoveryUntilSeconds,
                },
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
            "debug": {
                "reason": last_reason or "no_action",
                "pathStatus": "valid" if state.has_path() else "empty",
                "pathIndex": state.pathIndex,
                "pathLength": get_debug_path_length(state),
                "pathWorldPoints": build_path_world_points(state),
                "targetPathIndex": state.targetPathIndex,
                "targetWorldPoint": state.targetWorldPoint,
                "closestPathDistanceCm": state.closestPathDistanceCm,
                "maxPathErrorCm": state.maxPathErrorCm,
                "lookAheadDistanceM": state.currentLookAheadDistanceM,
                "nearObstacleWarningCount": state.nearObstacleWarningCount,
                "lastNearObstacleWarningCell": state.lastNearObstacleWarningCell,
                "lastNearObstacleWarningSource": state.lastNearObstacleWarningSource,
                "bColliding": request.robotState.bColliding,
                "collisionActorName": request.robotState.collisionActorName,
                "blockedCorridorCellCount": len(state.lastBlockedCorridorCells),
                "dynamicBlockedCellCount": len(state.dynamicBlockedCells),
                "repathDebounceKey": state.lastRepathDebounceKey,
                "repathDebounceCount": len(state.repathDebounceUntilSeconds),
                "recoveryUntilSeconds": state.recoveryUntilSeconds,
            },
        }


    def end(
        self,
        request: ScenarioEndRequest,
        state: AgentState,
    ) -> dict:
        debug = {
            "reason": "episode_end_recorded",
            "status": request.status,
            "stopCount": state.stopCount,
            "repathCount": state.repathCount,
            "slowdownCount": state.slowdownCount,
            "nearObstacleWarningCount": state.nearObstacleWarningCount,
            "lastNearObstacleWarningCell": state.lastNearObstacleWarningCell,
            "lastNearObstacleWarningSource": state.lastNearObstacleWarningSource,
            "blockedCorridorCellCount": len(state.lastBlockedCorridorCells),
        }

        state.clear_after_end()

        return {
            "status": "ok",
            "accepted": True,
            "metrics": {
                "nearObstacleWarningCount": debug["nearObstacleWarningCount"],
                "stopCount": debug["stopCount"],
                "repathCount": debug["repathCount"],
                "slowdownCount": debug["slowdownCount"],
            },
            "debug": debug,
        }


# PythonAgent가 사용할 BotPolicy를 생성
# Unreal은 policy를 선택하지 않고, Python은 여기 작성된 순서대로 동작
def create_policy() -> BotPolicy:
    return BotPolicy([
        RePathPolicy(),
       # StopPolicy(),
        PathFollower()
    ])
