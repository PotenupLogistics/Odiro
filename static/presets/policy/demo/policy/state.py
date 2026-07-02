from dataclasses import dataclass, field
from typing import Any

from .contract import GoalLocation, GridMap, StartLocation



# 하나의 episode 동안 유지되는 PythonAgent 상태
@dataclass
class AgentState:
    robotInstanceId: str | None = None     # 현재 로봇 ID

    start: StartLocation | None = None     # 시작 위치
    goal: GoalLocation | None = None       # 목표 위치
    grid: GridMap | None = None            # 길찾기용 Grid

    path: list[tuple[int, int]] = field(default_factory=list)   # 현재 경로 cell 목록
    pathIndex: int = 0                                          # 현재 따라가고 있는 path index

    lastAction: dict[str, Any] | None = None  # 마지막으로 Unreal에 보낸 action
    lastReason: str = ""                      # 마지막 action을 선택한 이유

    lastSequence: int = 0                     # 마지막으로 처리한 decide sequence
    lastRunTimeSeconds: float = 0.0         # 마지막으로 받은 Unreal world time
    lastSensorSequence: int = -1             # 마지막으로 받은 LiDAR snapshot sequence
    lastSensorTimeSeconds: float = 0.0       # 마지막으로 받은 LiDAR snapshot 생성 시간
    sensorSequenceRepeatCount: int = 0       # 같은 LiDAR snapshot을 연속 재사용한 decide 횟수
    sensorSequenceChangeCount: int = 0       # LiDAR snapshot sequence가 바뀐 횟수
    lastSensorDeltaSeconds: float = 0.0      # 마지막 LiDAR snapshot 갱신 간격

    stopCount: int = 0                        # 정지 action 발생 횟수
    repathCount: int = 0                      # 재경로 탐색 횟수
    slowdownCount: int = 0                    # 감속 action 발생 횟수
    followPathWorldPoints: list[dict[str, float]] = field(default_factory=list)
    obstacleWarningCount: int = 0
    bObstacleWarningRecorded: bool = False
    obstacleWarningRecordedSources: set[str] = field(default_factory=set)
    lastObstacleWarningCell: tuple[int, int] | None = None
    lastObstacleWarningSource: str = ""
    dynamicBlockedCells: set[tuple[int, int]] = field(default_factory=set)  # LiDAR로 새로 막은 동적 장애물 cell 목록
    lastRepathTimeSeconds: float = -999.0                                   # 마지막 재탐색 시간
    repathDebounceUntilSeconds: dict[str, float] = field(default_factory=dict)
    lastRepathDebounceKey: str = ""
    frontObstacleStopStartSeconds: float | None = None
    lastBlockedCorridorCells: set[tuple[int, int]] = field(default_factory=set)
    recoveryUntilSeconds: float = 0.0                                       # 재경로 전환 상태 호환용 시간 값
    recoverySteering: float = 0.0                                           # 재경로 전환 상태 호환용 조향 값
    lastSteering: float = 0.0                                               # 마지막으로 보낸 조향 값
    stuckStartSeconds: float | None = None                                  # 전진 명령 중 정체가 시작된 시간
    bStuckRepathRequested: bool = False                                     # 다음 RePath에서 가까운 경로 재진입을 요청할지 저장한다.

    bRepathRequested: bool = False                                      # 다음 decide에서 재경로 탐색을 요청할지 저장한다.
    targetPathIndex: int = 0                                            # 현재 실제로 바라보는 path index
    targetWorldPoint: dict[str, float] | None = None                    # 현재 실제로 바라보는 world point
    closestPathDistanceCm: float = 0.0                                  # 로봇과 경로 선분 사이 최소 거리
    maxPathErrorCm: float = 0.0                                         # 허용 가능한 경로 이탈 거리
    repathDecision: str = ""                                            # 마지막 RePath 판단 단계 또는 차단 사유
    bRepathForced: bool = False                                         # 기존 요청 또는 빈 경로 때문에 강제 재탐색이 필요한지 저장한다.
    bRepathFrontObstacle: bool = False                                  # 전방 장애물 조건으로 재탐색이 필요한지 저장한다.
    bRepathNeeded: bool = False                                         # 이번 decide에서 최종적으로 재탐색이 필요한지 저장한다.
    bRepathCanRunNow: bool = False                                      # cooldown 기준으로 지금 재탐색을 실행할 수 있는지 저장한다.
    bRepathFrontRayExists: bool = False                                 # RePath가 판단에 사용할 전방 hit ray를 찾았는지 저장한다.
    bRepathFrontRayInNearObject: bool = False                           # 전방 hit ray가 재탐색 거리 기준 안에 들어왔는지 저장한다.
    bRepathDebounced: bool = False                                      # 같은 장애물에 대한 재탐색 debounce에 걸렸는지 저장한다.
    repathFrontRayDistanceM: float | None = None                        # RePath가 본 가장 가까운 전방 hit ray 거리
    repathFrontRayYawDegree: float | None = None                        # RePath가 본 가장 가까운 전방 hit ray yaw 각도
    repathFrontRayActorName: str = ""                                   # RePath가 본 전방 hit ray의 actor 이름
    repathNearObjectDistanceM: float = 0.0                              # RePath 재탐색 거리 기준. 현재 obstacle warning 거리와 같다.
    repathDistanceGapM: float | None = None                             # front ray 거리에서 재탐색 거리 기준을 뺀 값
    repathFrontAngleDegree: float = 0.0                                 # RePath 전방 판정 half angle
    repathBlockRadiusCells: int = 0                                     # RePath가 동적 장애물을 확장해 막는 cell 반경
    currentLookAheadDistanceM: float = 0.0
    lastPathfindMetrics: dict[str, Any] = field(default_factory=dict)    # 마지막 A* 길찾기 성능 계측값

    # /scenario/start가 들어왔을 때 episode 상태를 새로 시작
    def reset_for_start(
        self,
        robot_instance_id: str,
        start: StartLocation,
        goal: GoalLocation,
        grid: GridMap,
    ) -> None:
        self.robotInstanceId = robot_instance_id

        self.start = start
        self.goal = goal
        self.grid = grid

        self.path = []
        self.pathIndex = 0
        self.followPathWorldPoints = []

        self.lastAction = None
        self.lastReason = ""
        self.lastSequence = 0
        self.lastRunTimeSeconds = 0.0
        self.lastSensorSequence = -1
        self.lastSensorTimeSeconds = 0.0
        self.sensorSequenceRepeatCount = 0
        self.sensorSequenceChangeCount = 0
        self.lastSensorDeltaSeconds = 0.0

        self.stopCount = 0
        self.repathCount = 0
        self.slowdownCount = 0
        self.obstacleWarningCount = 0
        self.bObstacleWarningRecorded = False
        self.obstacleWarningRecordedSources = set()
        self.lastObstacleWarningCell = None
        self.lastObstacleWarningSource = ""
        self.dynamicBlockedCells = set()
        self.lastRepathTimeSeconds = -999.0
        self.repathDebounceUntilSeconds = {}
        self.lastRepathDebounceKey = ""
        self.frontObstacleStopStartSeconds = None
        self.lastBlockedCorridorCells = set()
        self.recoveryUntilSeconds = 0.0
        self.recoverySteering = 0.0
        self.lastSteering = 0.0
        self.stuckStartSeconds = None
        self.bStuckRepathRequested = False
        self.bRepathRequested = False
        self.targetPathIndex = 0
        self.targetWorldPoint = None
        self.closestPathDistanceCm = 0.0
        self.maxPathErrorCm = 0.0
        self.repathDecision = ""
        self.bRepathForced = False
        self.bRepathFrontObstacle = False
        self.bRepathNeeded = False
        self.bRepathCanRunNow = False
        self.bRepathFrontRayExists = False
        self.bRepathFrontRayInNearObject = False
        self.bRepathDebounced = False
        self.repathFrontRayDistanceM = None
        self.repathFrontRayYawDegree = None
        self.repathFrontRayActorName = ""
        self.repathNearObjectDistanceM = 0.0
        self.repathDistanceGapM = None
        self.repathFrontAngleDegree = 0.0
        self.repathBlockRadiusCells = 0
        self.currentLookAheadDistanceM = 0.0
        self.lastPathfindMetrics = {}

    # /scenario/decide가 들어왔을 때 마지막 observation 시간 정보 저장
    def update_decide_time(self, sequence: int, run_time_seconds: float) -> None:
        self.lastSequence = sequence
        self.lastRunTimeSeconds = run_time_seconds


    # decide 요청이 참조하는 LiDAR snapshot 상태를 갱신
    def update_sensor_snapshot(self, sensor_sequence: int, sensor_time_seconds: float) -> None:
        if sensor_sequence == self.lastSensorSequence:
            self.sensorSequenceRepeatCount += 1
            return

        if self.lastSensorSequence >= 0:
            self.sensorSequenceChangeCount += 1
            self.lastSensorDeltaSeconds = max(0.0, sensor_time_seconds - self.lastSensorTimeSeconds)

        self.lastSensorSequence = sensor_sequence
        self.lastSensorTimeSeconds = sensor_time_seconds
        self.sensorSequenceRepeatCount = 0


    # 현재 path가 있는지 확인
    def has_path(self) -> bool:
        return len(self.path) > 0
    
    
    # 현재 path를 모두 따라갔는지 확인
    def is_path_finished(self) -> bool:
        path_length = len(self.followPathWorldPoints) if self.followPathWorldPoints else len(self.path)
        return self.has_path() and self.pathIndex >= path_length - 1


    # 마지막 action 선택 결과 저장
    def remember_action(self, action: dict[str, Any], reason: str) -> None:
        self.lastAction = action
        self.lastReason = reason
        
        
        
    # /scenario/end가 들어왔을 때 다음 episode를 위해 상태 정리
    def clear_after_end(self) -> None:
        self.robotInstanceId = None

        self.start = None
        self.goal = None
        self.grid = None

        self.path = []
        self.pathIndex = 0
        self.followPathWorldPoints = []

        self.lastAction = None
        self.lastReason = ""
        self.lastSequence = 0
        self.lastRunTimeSeconds = 0.0
        self.lastSensorSequence = -1
        self.lastSensorTimeSeconds = 0.0
        self.sensorSequenceRepeatCount = 0
        self.sensorSequenceChangeCount = 0
        self.lastSensorDeltaSeconds = 0.0

        self.stopCount = 0
        self.repathCount = 0
        self.slowdownCount = 0
        self.obstacleWarningCount = 0
        self.bObstacleWarningRecorded = False
        self.obstacleWarningRecordedSources = set()
        self.lastObstacleWarningCell = None
        self.lastObstacleWarningSource = ""
        self.dynamicBlockedCells = set()
        self.lastRepathTimeSeconds = -999.0
        self.repathDebounceUntilSeconds = {}
        self.lastRepathDebounceKey = ""
        self.frontObstacleStopStartSeconds = None
        self.lastBlockedCorridorCells = set()
        self.recoveryUntilSeconds = 0.0
        self.recoverySteering = 0.0
        self.lastSteering = 0.0
        self.stuckStartSeconds = None
        self.bStuckRepathRequested = False
        self.bRepathRequested = False
        self.targetPathIndex = 0
        self.targetWorldPoint = None
        self.closestPathDistanceCm = 0.0
        self.maxPathErrorCm = 0.0
        self.repathDecision = ""
        self.bRepathForced = False
        self.bRepathFrontObstacle = False
        self.bRepathNeeded = False
        self.bRepathCanRunNow = False
        self.bRepathFrontRayExists = False
        self.bRepathFrontRayInNearObject = False
        self.bRepathDebounced = False
        self.repathFrontRayDistanceM = None
        self.repathFrontRayYawDegree = None
        self.repathFrontRayActorName = ""
        self.repathNearObjectDistanceM = 0.0
        self.repathDistanceGapM = None
        self.repathFrontAngleDegree = 0.0
        self.repathBlockRadiusCells = 0
        self.currentLookAheadDistanceM = 0.0
        self.lastPathfindMetrics = {}
