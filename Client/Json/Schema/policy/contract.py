from dataclasses import dataclass, field
from typing import Any



@dataclass
class PolicyError:   
    code: str     # 에러 구분 
    message: str  # 에러 설명
    retryable: bool = False  # 다시 요청 시도 가능한 에러인가?
    details: dict[str, Any] = field(default_factory=dict)  # 디버그 정보 

#  Transform 정보
@dataclass
class StartLocation:
    x: float
    y: float
    z: float = 0.0
    yawDegree: float = 0.0

#  목표 지점
@dataclass
@dataclass
class GoalLocation:
    hasGoal: bool   # 목표가 존재하는가
    x: float
    y: float
    z: float = 0.0


#  Grid 한 칸
@dataclass
class GridCell:
    x: int
    y: int
    areaType: str = "Walkable"  
    cost: float = 1.0 # 길찾기 비용
    blocked: bool = False 
    sourceCollisionProfile: str = ""   # 콜리전 프로필 정보


#  Grid 전체
@dataclass
class GridMap:
    gridSizeX: int      # X축 셀 개수
    gridSizeY: int      # Y축 셀 개수
    cellSizeCm: float   # 셀 크기
    cellCount: int      # 셀 개수
    originCm: StartLocation      # 그리드의 원점 위치
    cells: list[GridCell]   # 셀 목록


#  decide 단계에서 전달하는 로봇의 상태
@dataclass
class RobotState:
    x: float
    y: float
    z: float = 0.0
    yawDegree: float = 0.0
    speedKmh: float = 0.0
    bColliding: bool = False
    collisionActorName: str = ""
    collisionActorTags: list[str] = field(default_factory=list)

# LiDAR에서 발사한 Ray 하나의 관측 결과
@dataclass
class LidarRay:
    hit: bool  # 부딪혔는가
    distanceM: float
    rayIndex: int | None = None     # 인덱스가 없을 수 있다.
    rayYawDegree: float = 0.0       # 로봇의 방향 기준으로 Ray의 로컬 방향
    actorName: str | None = None    # 맞은 액터 이름
    actorTags: list[str] = field(default_factory=list)  # 맞은 액터의 태그 목록

# Scenario 시작 시 초기 설정
@dataclass
class ScenarioStartRequest:
    robotInstanceId: str# 로봇 ID (로봇 여러 개인 경우 대비)
    start: StartLocation
    goal: GoalLocation
    grid: GridMap
    robotSpec: dict[str, Any] = field(default_factory=dict)
    driveSpec: dict[str, Any] = field(default_factory=dict)
    lidarSpec: dict[str, Any] = field(default_factory=dict)
    vehicleSpec: dict[str, Any] = field(default_factory=dict)  # legacy: use robotSpec instead
    controlSpec: dict[str, Any] = field(default_factory=dict)  # legacy: Python-side policy config now owns these values


# decide 요청 데이터, 즉, observation 값
@dataclass
class ScenarioDecideRequest:
    sequence: int           # 요청 들어온 순서
    runTimeSeconds: float   # 플레이 시간 
    robotState: RobotState  # 현재 로봇 상태
    lidarRays: list[LidarRay] = field(default_factory=list) # Ray 목록
    observedObjects: list[dict[str, Any]] = field(default_factory=list) # 관측된 Actor 목록


@dataclass
class ScenarioEndRequest:
    robotInstanceId: str# 로봇 ID (로봇 여러 개인 경우 대비)
    sequence: int       # 마지막 요청 번호
    status: str         # 종료 상태. 보통 ok 또는 error
    error: PolicyError | None = None   # 실패한 경우 실패 이유
    metrics: dict[str, Any] = field(default_factory=dict)   # 주행 시간, 정지 횟수, near miss 같은 결과 지표
    debug: dict[str, Any] = field(default_factory=dict)     # 종료 시점의 디버그 정보
