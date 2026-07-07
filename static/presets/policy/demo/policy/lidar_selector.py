import math

from .contract import LidarRay, LidarRay1D, LidarRay2D, LidarRay3D, ScenarioDecideRequest


# typed 1D ray를 정책이 쓰는 전방 ray 형태로 변환한다.
def convert_lidar_ray_1d_to_policy_ray(ray: LidarRay1D) -> LidarRay:
    return LidarRay(
        hit=ray.hit,
        distanceM=ray.distanceM,
        blocksPolicy=ray.blocksPolicy,
        rayIndex=ray.rayIndex,
        rayYawDegree=0.0,
        actorName=ray.actorName,
        actorTags=list(ray.actorTags or []),
    )


# typed 2D ray를 정책이 쓰는 ray 형태로 변환한다.
def convert_lidar_ray_2d_to_policy_ray(ray: LidarRay2D) -> LidarRay:
    return LidarRay(
        hit=ray.hit,
        distanceM=ray.distanceM,
        blocksPolicy=ray.blocksPolicy,
        rayIndex=ray.rayIndex,
        rayYawDegree=ray.yawDegree,
        actorName=ray.actorName,
        actorTags=list(ray.actorTags or []),
    )


# typed 3D ray를 수평면에 투영해 정책이 쓰는 2D ray 형태로 변환한다.
def convert_lidar_ray_3d_to_policy_ray(ray: LidarRay3D) -> LidarRay:
    projected_distance_m = ray.distanceM * max(0.0, math.cos(math.radians(ray.pitchDegree)))

    return LidarRay(
        hit=ray.hit,
        distanceM=projected_distance_m,
        blocksPolicy=ray.blocksPolicy,
        rayIndex=ray.rayIndex,
        rayYawDegree=ray.yawDegree,
        actorName=ray.actorName,
        actorTags=list(ray.actorTags or []),
    )


# LiDAR mode 문자열을 비교하기 쉬운 소문자 key로 변환한다.
def normalize_lidar_mode(mode: str | None) -> str:
    return str(mode or "").strip().lower()


# 선택된 LiDAR mode가 정책에서 어느 차원 입력으로 해석되는지 반환한다.
def get_policy_lidar_family(request: ScenarioDecideRequest) -> str:
    mode = normalize_lidar_mode(request.lidar.mode)

    if mode in {"oned", "1d"}:
        return "1d"

    if mode in {"twod", "2d", "onedandtwod", "twodandthreed"}:
        return "2d"

    if mode in {"threed", "3d"}:
        return "3d"

    if mode == "all":
        if request.lidar.rays2d:
            return "2d"
        if request.lidar.rays3d:
            return "3d"
        if request.lidar.rays1d:
            return "1d"
        return "2d"

    if request.lidar.rays2d:
        return "2d"

    if request.lidar.rays3d:
        return "3d"

    if request.lidar.rays1d:
        return "1d"

    if request.lidarRays:
        return "legacy2d"

    return "none"


# 현재 정책 입력이 1D LiDAR만으로 만들어졌는지 확인한다.
def is_policy_lidar_one_dimensional(request: ScenarioDecideRequest) -> bool:
    return get_policy_lidar_family(request) == "1d"


# yaw 값을 비교 가능한 key로 정규화한다.
def get_lidar_yaw_key(yaw_degree: float) -> int:
    return round(yaw_degree * 100.0)


# 3D miss 대표 ray를 고를 때 기준이 되는 수평 row pitch 절대값을 고른다.
def get_horizontal_pitch_degree(rays: list[LidarRay3D]) -> float | None:
    if not rays:
        return None

    return min((abs(ray.pitchDegree) for ray in rays), default=0.0)


# 같은 yaw 방향에서 3D ray를 2D 정책 ray로 대표하기에 더 적합한지 판단한다.
def is_better_projected_3d_ray(
    candidate_ray: LidarRay3D,
    current_ray: LidarRay3D,
    horizontal_pitch_abs: float,
) -> bool:
    if candidate_ray.hit != current_ray.hit:
        return candidate_ray.hit

    if candidate_ray.hit:
        if candidate_ray.blocksPolicy != current_ray.blocksPolicy:
            return candidate_ray.blocksPolicy

        candidate_distance_m = candidate_ray.distanceM * max(0.0, math.cos(math.radians(candidate_ray.pitchDegree)))
        current_distance_m = current_ray.distanceM * max(0.0, math.cos(math.radians(current_ray.pitchDegree)))
        if candidate_distance_m != current_distance_m:
            return candidate_distance_m < current_distance_m

        return abs(candidate_ray.pitchDegree) < abs(current_ray.pitchDegree)

    candidate_pitch_gap = abs(abs(candidate_ray.pitchDegree) - horizontal_pitch_abs)
    current_pitch_gap = abs(abs(current_ray.pitchDegree) - horizontal_pitch_abs)
    if candidate_pitch_gap != current_pitch_gap:
        return candidate_pitch_gap < current_pitch_gap

    return candidate_ray.distanceM < current_ray.distanceM


# 3D LiDAR를 같은 yaw별 최단 vertical hit 우선의 2D 정책 ray로 투영한다.
def project_lidar_rays_3d_to_policy_rays(rays: list[LidarRay3D]) -> list[LidarRay]:
    horizontal_pitch_abs = get_horizontal_pitch_degree(rays)
    if horizontal_pitch_abs is None:
        return []

    projected_ray_by_yaw: dict[int, LidarRay3D] = {}

    for ray in rays:
        yaw_key = get_lidar_yaw_key(ray.yawDegree)
        current_ray = projected_ray_by_yaw.get(yaw_key)

        if current_ray is None or is_better_projected_3d_ray(ray, current_ray, horizontal_pitch_abs):
            projected_ray_by_yaw[yaw_key] = ray

    projected_rays = [
        convert_lidar_ray_3d_to_policy_ray(ray)
        for _, ray in sorted(projected_ray_by_yaw.items())
    ]

    for ray_index, ray in enumerate(projected_rays):
        ray.rayIndex = ray_index

    return projected_rays


# 현재 request에서 정책이 사용할 LiDAR ray 목록과 선택 출처를 만든다.
def select_policy_lidar_selection(request: ScenarioDecideRequest) -> dict:
    family = get_policy_lidar_family(request)

    if family == "1d":
        return {
            "family": family,
            "source": "lidar.rays1d",
            "horizontalPitchDegree": None,
            "rays": [
                convert_lidar_ray_1d_to_policy_ray(ray)
                for ray in request.lidar.rays1d
            ],
        }

    if family == "2d":
        return {
            "family": family,
            "source": "lidar.rays2d",
            "horizontalPitchDegree": None,
            "rays": [
                convert_lidar_ray_2d_to_policy_ray(ray)
                for ray in request.lidar.rays2d
            ],
        }

    if family == "3d":
        return {
            "family": family,
            "source": "lidar.rays3d.nearest_vertical_by_yaw",
            "horizontalPitchDegree": get_horizontal_pitch_degree(request.lidar.rays3d),
            "rays": project_lidar_rays_3d_to_policy_rays(request.lidar.rays3d),
        }

    if family == "legacy2d":
        return {
            "family": family,
            "source": "legacy.lidarRays",
            "horizontalPitchDegree": None,
            "rays": request.lidarRays,
        }

    return {
        "family": family,
        "source": "none",
        "horizontalPitchDegree": None,
        "rays": [],
    }


# 현재 정책이 사용할 LiDAR ray 목록을 선택한다.
def select_policy_lidar_rays(request: ScenarioDecideRequest) -> list[LidarRay]:
    return list(select_policy_lidar_selection(request)["rays"])


# 기존 정책 코드 호환용 이름으로 정책 LiDAR ray 목록을 선택한다.
def select_policy_lidar_rays_2d(request: ScenarioDecideRequest) -> list[LidarRay]:
    return select_policy_lidar_rays(request)


# 현재 정책이 어떤 LiDAR 입력을 선택했는지 반환한다.
def get_policy_lidar_ray_source(request: ScenarioDecideRequest) -> str:
    return str(select_policy_lidar_selection(request)["source"])


# LiDAR 입력 구조와 정책 선택 결과를 debug dict로 만든다.
def build_lidar_input_debug(request: ScenarioDecideRequest) -> dict:
    selection = select_policy_lidar_selection(request)
    selected_rays = list(selection["rays"])

    return {
        "lidarMode": request.lidar.mode,
        "lidarRayPayloadMode": request.lidar.rayPayloadMode,
        "bSendFullLidarRays": request.lidar.sendFullRays,
        "selectedLidarPolicyMode": selection["family"],
        "lidarRays1dCount": len(request.lidar.rays1d),
        "lidarRays2dCount": len(request.lidar.rays2d),
        "lidarRays3dCount": len(request.lidar.rays3d),
        "legacyLidarRayCount": len(request.lidarRays),
        "selectedLidarRaySource": selection["source"],
        "selectedLidarRayCount": len(selected_rays),
        "selectedLidarHorizontalPitchDegree": selection["horizontalPitchDegree"],
        "lidarRayCount": len(selected_rays),
    }
