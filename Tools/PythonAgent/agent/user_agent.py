from .action import BotAction
from .contract import ScenarioDecideRequest, ScenarioEndRequest, ScenarioStartRequest
from .pathfinding.astar import AStarPathfinder
from .policies.path_follower import PathFollower
from .policies.repath_policy import RePathPolicy
from .policies.slowdown_policy import SlowDownPolicy
from .policies.stop_policy import StopPolicy
from .state import AgentState



# PythonAgent의 전체 정책 흐름을 담당하는 클래스
class BotPolicy:

    # 사용할 정책 목록과 길찾기 객체를 준비
    def __init__(self, policies: list):
        self.policies = policies                  # decide 때 순서대로 실행할 정책 목록
        self.pathfinder = AStarPathfinder()       # start 때 최초 경로를 만들 A* 객체


  # /scenario/start 요청 처리
    def start(
        self,
        request: ScenarioStartRequest,
        state: AgentState,
    ) -> dict:
        state.reset_for_start(
            experiment_id=request.experimentId,
            episode_id=request.episodeId,
            robot_instance_id=request.robotInstanceId,
            start=request.start,
            goal=request.goal,
            grid=request.grid,
        )

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

        return {
            "status": "ok",
            "accepted": True,
            "pathStatus": "valid",
            "debug": {
                "reason": "initial_path_ready",
                "pathLength": len(state.path),
            },
        }
        
        
        # /scenario/decide 요청 처리
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

            return {
                "sequence": request.sequence,
                "status": "ok",
                "action": action_dict,
                "debug": {
                    "selectedPolicy": policy.name,
                    "reason": reason,
                    "pathStatus": "valid" if state.has_path() else "empty",
                    "pathIndex": state.pathIndex,
                },
            }

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
            },
        }
        
        
        
        # /scenario/end 요청 처리
    def end(
        self,
        request: ScenarioEndRequest,
        state: AgentState,
    ) -> dict:
        debug = {
            "reason": "episode_end_recorded",
            "episodeId": request.episodeId,
            "status": request.status,
            "stopCount": state.stopCount,
            "repathCount": state.repathCount,
            "slowdownCount": state.slowdownCount,
        }

        state.clear_after_end()

        return {
            "status": "ok",
            "accepted": True,
            "debug": debug,
        }


# PythonAgent가 사용할 BotPolicy를 생성
# Unreal은 policy를 선택하지 않고, Python은 여기 작성된 순서대로 동작
def create_policy() -> BotPolicy:
    return BotPolicy([
        StopPolicy(),
        RePathPolicy(),
        SlowDownPolicy(),
        PathFollower(),
    ])
