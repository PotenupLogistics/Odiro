from ..action import BotAction
from ..contract import ScenarioDecideRequest
from ..pathfinding.astar import AStarPathfinder
from ..state import AgentState


# 현재 경로가 없거나 막혔을 때 다시 경로를 찾는 정책
class RePathPolicy:
    name = "RePathPolicy"   # debug.selectedPolicy에 들어갈 이름

    def __init__(self):
        self.pathfinder = AStarPathfinder()   # A* 길찾기 객체


    # 현재 observation과 state를 보고 재경로 탐색이 필요한지 판단
    def decide(
        self,
        request: ScenarioDecideRequest,
        state: AgentState,
    ) -> tuple[BotAction | None, str]:
        if state.has_path():
            return None, ""

        if state.grid is None:
            return None, "missing_grid"

        if state.goal is None:
            return None, "missing_goal"

        result = self.pathfinder.find_path(
            start=request.robotState,
            goal=state.goal,
            grid=state.grid,
        )

        if not result.success:
            return None, result.reason

        state.path = result.path
        state.pathIndex = 0
        state.repathCount += 1

        return None, "repath_ready"