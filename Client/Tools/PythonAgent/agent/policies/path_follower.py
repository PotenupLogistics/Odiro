from ..action import BotAction, drive_action, stop_action
from ..contract import ScenarioDecideRequest
from ..state import AgentState


# 현재 경로를 따라가는 기본 주행 정책
class PathFollower:
    name = "PathFollower"   # debug.selectedPolicy에 들어갈 이름

    def __init__(self, follow_speed_kmh: float = 4.0):
        self.followSpeedKmh = follow_speed_kmh   # 경로 추종 시 기본 목표 속도
        
        
    # 현재 path 상태를 보고 이동 Action을 생성
    def decide(
        self,
        request: ScenarioDecideRequest,
        state: AgentState,
    ) -> tuple[BotAction | None, str]:
        if not state.has_path():
            return stop_action(), "no_path"

        if state.is_path_finished():
            return stop_action(), "path_finished"

        state.pathIndex += 1

        return drive_action(
            steering=0.0,
            speed_kmh=self.followSpeedKmh,
        ), "follow_path"