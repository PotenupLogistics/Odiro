from dataclasses import dataclass, field
from typing import Any


# Python 정책 처리 실패 정보를 Unreal에 전달한다.
@dataclass
class PolicyError:
    code: str  # 에러 구분 코드
    message: str  # 에러 설명
    retryable: bool = False  # 같은 요청을 다시 시도할 수 있는 에러인지 나타낸다.
    details: dict[str, Any] = field(default_factory=dict)  # 추가 진단 정보


# Scenario 시작 위치와 방향을 Unreal world 좌표로 표현한다.
@dataclass
class StartLocation:
    x: float  # Unreal world X 좌표(cm)
    y: float  # Unreal world Y 좌표(cm)
    z: float = 0.0  # Unreal world Z 좌표(cm)
    yawDegree: float = 0.0  # 시작 방향(degree)


# 길찾기에 사용할 목표 지점을 표현한다.
@dataclass
class GoalLocation:
    hasGoal: bool  # 유효한 목표가 존재하는지 나타낸다.
    x: float  # Unreal world X 좌표(cm)
    y: float  # Unreal world Y 좌표(cm)
    z: float = 0.0  # Unreal world Z 좌표(cm)


# Navigation grid 한 칸의 이동 가능 상태를 표현한다.
@dataclass
class GridCell:
    x: int  # Grid X 인덱스
    y: int  # Grid Y 인덱스
    areaType: str = "Walkable"  # 이동 가능 영역 종류
    cost: float = 1.0  # 길찾기 비용
    blocked: bool = False  # 통과할 수 없는 셀인지 나타낸다.
    sourceCollisionProfile: str = ""  # 셀 판정에 사용된 collision profile


# Scenario의 전체 Navigation grid를 표현한다.
@dataclass
class GridMap:
    gridSizeX: int  # X축 셀 개수
    gridSizeY: int  # Y축 셀 개수
    cellSizeCm: float  # 셀 한 변의 크기(cm)
    cellCount: int  # 전체 셀 개수
    originCm: StartLocation  # Grid 원점의 Unreal world 위치
    cells: list[GridCell]  # 셀 상태 목록


# decide 단계에서 전달하는 로봇의 현재 상태를 표현한다.
@dataclass
class RobotState:
    x: float  # Unreal world X 좌표(cm)
    y: float  # Unreal world Y 좌표(cm)
    z: float = 0.0  # Unreal world Z 좌표(cm)
    yawDegree: float = 0.0  # 로봇 진행 방향(degree)
    speedKmh: float = 0.0  # 현재 주행 속도(km/h)
    bColliding: bool = False  # 현재 충돌 정지 상태인지 나타낸다.
    collisionActorName: str = ""  # 충돌한 Actor 이름
    collisionActorTags: list[str] = field(default_factory=list)  # 충돌한 Actor 태그 목록


# 기존 2D 주행 정책과 호환되는 LiDAR ray 하나를 표현한다.
@dataclass
class LidarRay:
    hit: bool  # Ray가 유효한 Actor와 충돌했는지 나타낸다.
    distanceM: float  # Ray 충돌 거리(m)
    rayIndex: int | None = None  # 센서 frame 안의 Ray 인덱스
    rayYawDegree: float = 0.0  # 로봇 방향 기준 Ray의 local yaw(degree)
    actorName: str | None = None  # 충돌한 Actor 이름
    actorTags: list[str] = field(default_factory=list)  # 충돌한 Actor 태그 목록


# 전방 단일 방향을 측정하는 1D LiDAR ray를 표현한다.
@dataclass
class LidarRay1D:
    hit: bool  # Ray가 유효한 Actor와 충돌했는지 나타낸다.
    distanceM: float  # Ray 충돌 거리(m)
    rayIndex: int | None = None  # 센서 frame 안의 Ray 인덱스
    actorName: str | None = None  # 충돌한 Actor 이름
    actorTags: list[str] = field(default_factory=list)  # 충돌한 Actor 태그 목록


# 수평 평면을 측정하는 2D LiDAR ray를 표현한다.
@dataclass
class LidarRay2D:
    hit: bool  # Ray가 유효한 Actor와 충돌했는지 나타낸다.
    distanceM: float  # Ray 충돌 거리(m)
    yawDegree: float = 0.0  # 로봇 방향 기준 Ray의 local yaw(degree)
    rayIndex: int | None = None  # 센서 frame 안의 Ray 인덱스
    actorName: str | None = None  # 충돌한 Actor 이름
    actorTags: list[str] = field(default_factory=list)  # 충돌한 Actor 태그 목록


# Point Cloud 생성에 사용하는 3D LiDAR ray를 표현한다.
@dataclass
class LidarRay3D:
    hit: bool  # Ray가 유효한 Actor와 충돌했는지 나타낸다.
    distanceM: float  # Ray 충돌 거리(m)
    yawDegree: float = 0.0  # 로봇 방향 기준 Ray의 local yaw(degree)
    pitchDegree: float = 0.0  # 로봇 방향 기준 Ray의 local pitch(degree)
    rayIndex: int | None = None  # 센서 frame 안의 Ray 인덱스
    actorName: str | None = None  # 충돌한 Actor 이름
    actorTags: list[str] = field(default_factory=list)  # 충돌한 Actor 태그 목록
    hitLocationCm: dict[str, float] | None = None  # Raycast가 맞은 Unreal world 위치(cm)


# 차원별 LiDAR ray와 센서 frame 정보를 하나의 관측값으로 묶는다.
@dataclass
class LidarObservation:
    mode: str = ""  # 현재 활성화된 LiDAR 관측 차원 조합
    sensorSequence: int = 0  # LiDAR sensor frame 순서 번호
    sensorTimeSeconds: float = 0.0  # Sensor frame의 simulation time
    rays1d: list[LidarRay1D] = field(default_factory=list)  # 1D Ray 목록
    rays2d: list[LidarRay2D] = field(default_factory=list)  # 2D Ray 목록
    rays3d: list[LidarRay3D] = field(default_factory=list)  # 3D Ray 목록


# Scenario 시작 시 Python 정책을 초기화하는 설정을 전달한다.
@dataclass
class ScenarioStartRequest:
    robotInstanceId: str  # 여러 로봇을 구분하는 Robot ID
    start: StartLocation  # Scenario 시작 위치
    goal: GoalLocation  # Scenario 목표 위치
    grid: GridMap  # 길찾기에 사용할 Navigation grid
    robotSpec: dict[str, Any] = field(default_factory=dict)  # 로봇 크기와 속도 제약
    driveSpec: dict[str, Any] = field(default_factory=dict)  # 구동 및 제동 설정
    lidarSpec: dict[str, Any] = field(default_factory=dict)  # LiDAR 장비와 capture 설정
    artifactSpec: dict[str, Any] = field(default_factory=dict)  # Python artifact 저장 위치와 참조 기준
    vehicleSpec: dict[str, Any] = field(default_factory=dict)  # Legacy 호환용 로봇 설정
    controlSpec: dict[str, Any] = field(default_factory=dict)  # Legacy 호환용 정책 설정


# decide 단계의 로봇 및 센서 관측값을 전달한다.
@dataclass
class ScenarioDecideRequest:
    sequence: int  # Decide 요청 순서 번호
    runTimeSeconds: float  # Episode 진행 시간
    robotState: RobotState  # 현재 로봇 상태
    sensorSequence: int = 0  # 현재 LiDAR sensor frame 순서 번호
    sensorTimeSeconds: float = 0.0  # 현재 LiDAR sensor frame의 simulation time
    lidar: LidarObservation = field(default_factory=LidarObservation)  # 차원별 LiDAR 관측값
    lidarRays: list[LidarRay] = field(default_factory=list)  # 기존 정책 호환용 2D Ray 목록
    observedObjects: list[dict[str, Any]] = field(default_factory=list)  # 관측된 Actor 요약 목록


# Episode 종료 상태를 Python 정책에 전달한다.
@dataclass
class ScenarioEndRequest:
    robotInstanceId: str  # 종료 대상 Robot ID
    sequence: int  # 마지막 decide 요청 번호
    status: str  # goal_reached, timeout 등의 Episode 종료 원인
    error: PolicyError | None = None  # 실패한 경우의 정책 오류 정보
    metrics: dict[str, Any] = field(default_factory=dict)  # 주행 시간과 이벤트 집계 등 결과 지표
    debug: dict[str, Any] = field(default_factory=dict)  # 종료 시점의 추가 진단 정보
