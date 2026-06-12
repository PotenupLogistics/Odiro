from dataclasses import dataclass, field
from typing import Any

from .contract import GoalLocation, GridMap, StartLocation



# 하나의 episode 동안 유지되는 PythonAgent 상태
@dataclass
class AgentState:
    experimentId: str | None = None        # 실험 ID
    episodeId: str | None = None           # 현재 episode ID
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

    stopCount: int = 0                        # 정지 action 발생 횟수
    repathCount: int = 0                      # 재경로 탐색 횟수
    slowdownCount: int = 0                    # 감속 action 발생 횟수
    nearMissCount: int = 0
    bNearMissRecorded: bool = False
    nearMissRecordedSources: set[str] = field(default_factory=set)
    lastNearMissCell: tuple[int, int] | None = None
    lastNearMissSource: str = ""
    dynamicBlockedCells: set[tuple[int, int]] = field(default_factory=set)  # LiDAR로 새로 막은 동적 장애물 cell 목록
    lastRepathTimeSeconds: float = -999.0                                   # 마지막 재탐색 시간
    frontObstacleStopStartSeconds: float | None = None
    lastBlockedCorridorCells: set[tuple[int, int]] = field(default_factory=set)
    recoveryUntilSeconds: float = 0.0                                       # 후진 회복 동작을 유지할 시간
    recoverySteering: float = 0.0                                           # 후진 회복 동작 중 사용할 조향 값
    lastSteering: float = 0.0                                               # 마지막으로 보낸 조향 값

    bRepathRequested: bool = False                                      # 다음 decide에서 재경로 탐색을 요청할지 저장한다.
    targetPathIndex: int = 0                                            # 현재 실제로 바라보는 path index
    targetWorldPoint: dict[str, float] | None = None                    # 현재 실제로 바라보는 world point
    closestPathDistanceCm: float = 0.0                                  # 로봇과 경로 선분 사이 최소 거리
    maxPathErrorCm: float = 0.0                                         # 허용 가능한 경로 이탈 거리

    # /scenario/start가 들어왔을 때 episode 상태를 새로 시작
    def reset_for_start(
        self,
        experiment_id: str | None,
        episode_id: str,
        robot_instance_id: str,
        start: StartLocation,
        goal: GoalLocation,
        grid: GridMap,
    ) -> None:
        self.experimentId = experiment_id
        self.episodeId = episode_id
        self.robotInstanceId = robot_instance_id

        self.start = start
        self.goal = goal
        self.grid = grid

        self.path = []
        self.pathIndex = 0

        self.lastAction = None
        self.lastReason = ""
        self.lastSequence = 0
        self.lastRunTimeSeconds = 0.0

        self.stopCount = 0
        self.repathCount = 0
        self.slowdownCount = 0
        self.nearMissCount = 0
        self.bNearMissRecorded = False
        self.nearMissRecordedSources = set()
        self.lastNearMissCell = None
        self.lastNearMissSource = ""
        self.dynamicBlockedCells = set()
        self.lastRepathTimeSeconds = -999.0
        self.frontObstacleStopStartSeconds = None
        self.lastBlockedCorridorCells = set()
        self.recoveryUntilSeconds = 0.0
        self.recoverySteering = 0.0
        self.lastSteering = 0.0
        self.bRepathRequested = False
        self.targetPathIndex = 0
        self.targetWorldPoint = None
        self.closestPathDistanceCm = 0.0
        self.maxPathErrorCm = 0.0

    # /scenario/decide가 들어왔을 때 마지막 observation 시간 정보 저장
    def update_decide_time(self, sequence: int, run_time_seconds: float) -> None:
        self.lastSequence = sequence
        self.lastRunTimeSeconds = run_time_seconds


    # 현재 path가 있는지 확인
    def has_path(self) -> bool:
        return len(self.path) > 0
    
    
    # 현재 path를 모두 따라갔는지 확인
    def is_path_finished(self) -> bool:
        return self.has_path() and self.pathIndex >= len(self.path) - 1


    # 마지막 action 선택 결과 저장
    def remember_action(self, action: dict[str, Any], reason: str) -> None:
        self.lastAction = action
        self.lastReason = reason
        
        
        
    # /scenario/end가 들어왔을 때 다음 episode를 위해 상태 정리
    def clear_after_end(self) -> None:
        self.experimentId = None
        self.episodeId = None
        self.robotInstanceId = None

        self.start = None
        self.goal = None
        self.grid = None

        self.path = []
        self.pathIndex = 0

        self.lastAction = None
        self.lastReason = ""
        self.lastSequence = 0
        self.lastRunTimeSeconds = 0.0

        self.stopCount = 0
        self.repathCount = 0
        self.slowdownCount = 0
        self.nearMissCount = 0
        self.bNearMissRecorded = False
        self.nearMissRecordedSources = set()
        self.lastNearMissCell = None
        self.lastNearMissSource = ""
        self.dynamicBlockedCells = set()
        self.lastRepathTimeSeconds = -999.0
        self.frontObstacleStopStartSeconds = None
        self.lastBlockedCorridorCells = set()
        self.recoveryUntilSeconds = 0.0
        self.recoverySteering = 0.0
        self.lastSteering = 0.0
        self.bRepathRequested = False
        self.targetPathIndex = 0
        self.targetWorldPoint = None
        self.closestPathDistanceCm = 0.0
        self.maxPathErrorCm = 0.0
