from ..action import BotAction, stop_action
from ..contract import ScenarioDecideRequest
from ..state import AgentState



# 즉시 정지가 필요한지 판단하는 정책
class StopPolicy:
    name = "StopPolicy"   # debug.selectedPolicy에 들어갈 이름

    def __init__(self, stop_distance_m: float = 0.6, front_angle_degree: float = 20.0):
        self.stopDistanceM = stop_distance_m          # 이 거리보다 가까우면 정지
        self.frontAngleDegree = front_angle_degree    # 정면으로 볼 각도 범위


    # 현재 observation을 보고 정지 Action을 만들지 판단
    def decide(
        self,
        request: ScenarioDecideRequest,
        state: AgentState,
    ) -> tuple[BotAction | None, str]:
        for ray in request.lidarRays:
            if not ray.hit:
                continue

            if abs(ray.rayYawDegree) > self.frontAngleDegree:
                continue

            if ray.distanceM <= self.stopDistanceM:
                state.stopCount += 1
                return stop_action(), "front_obstacle_too_close"

        return None, ""