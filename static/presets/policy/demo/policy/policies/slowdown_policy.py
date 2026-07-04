from ..action import BotAction, drive_action
from ..contract import ScenarioDecideRequest
from ..lidar_selector import select_policy_lidar_rays_2d
from ..state import AgentState


# 전방 장애물이 가까울 때 감속하는 정책
class SlowDownPolicy:
    name = "SlowDownPolicy"   # debug.selectedPolicy에 들어갈 이름

    def __init__(
        self,
        slowdown_distance_m: float = 2.5,
        front_angle_degree: float = 30.0,
        slowdown_speed_kmh: float = 2.0,
    ):
        self.slowdownDistanceM = slowdown_distance_m       # 이 거리보다 가까우면 감속
        self.frontAngleDegree = front_angle_degree         # 정면으로 볼 각도 범위
        self.slowdownSpeedKmh = slowdown_speed_kmh         # 감속 시 목표 속도


    # 현재 observation을 보고 감속 Action을 만들지 판단
    def decide(
        self,
        request: ScenarioDecideRequest,
        state: AgentState,
    ) -> tuple[BotAction | None, str]:
        lidar_rays = select_policy_lidar_rays_2d(request)

        for ray in lidar_rays:
            if not ray.hit or not ray.blocksPolicy or has_ground_tags(ray.actorTags or []):
                continue

            if abs(normalize_angle_degree(ray.rayYawDegree)) > self.frontAngleDegree:
                continue

            if ray.distanceM <= self.slowdownDistanceM:
                state.slowdownCount += 1
                return drive_action(
                    steering=0.0,
                    speed_kmh=self.slowdownSpeedKmh,
                ), "front_obstacle_slowdown"

        return None, ""


def normalize_angle_degree(angle_degree: float) -> float:
    return (angle_degree + 180.0) % 360.0 - 180.0


# Ground-tagged low surfaces are handled by PathFollower pass-over slowdown.
def has_ground_tags(actor_tags: list[str]) -> bool:
    normalized_tags = {str(tag).strip().lower() for tag in actor_tags}
    return "ground" in normalized_tags or any(tag.endswith(".ground") for tag in normalized_tags)
