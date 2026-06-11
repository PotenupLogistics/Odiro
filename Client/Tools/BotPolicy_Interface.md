# PythonAgent BotPolicy 내부 인터페이스 명세

## 문서 목적

이 문서는 `Tools/PythonAgent.md`와 함께 읽는 보조 문서다.

`PythonAgent.md`는 Unreal과 Python 서버가 주고받는 외부 HTTP 기준이다. 이 문서는 Python 서버 내부에서 사용자 정책 코드를 어떤 함수와 데이터 구조로 호출할지 정한다.

즉, 전체 흐름은 아래처럼 나뉜다.

```text
Unreal DeliveryBot
  -> HTTP: /scenario/start, /scenario/decide, /scenario/end
  -> Tools/PythonAgent/server.py
  -> Tools/PythonAgent/agent/__init__.py
  -> Tools/PythonAgent/agent/user_agent.py
  -> BotPolicy
  -> policies/*, pathfinding/astar.py
```

Unreal은 월드 상태, Grid, LiDAR, 로봇 상태를 Python으로 보낸다. Python은 정책 선택, 길찾기, 정지/감속/재경로 판단, path following을 수행한 뒤 Action을 반환한다. Unreal은 반환된 Action을 검증하고 실행한다.

## 기준 문서

이 문서보다 우선하는 기준은 `Tools/PythonAgent.md`다.

확정 HTTP API는 아래 3개다.

| 단계 | HTTP | 역할 |
|---|---|---|
| Start | `POST /scenario/start` | episode 시작 시 Grid, Start, Goal, 차량/센서 설정 전달 |
| Decide | `POST /scenario/decide` | 주행 중 observation을 보내고 action을 받음 |
| End | `POST /scenario/end` | 도착 또는 실패 결과 전달/기록 |

중간 설정 변경 흐름은 현재 구현하지 않는다. 주행 중 설정을 바꾸는 기능이 필요해지면 별도 설계로 추가한다.

이 문서의 새 기준 API는 위 3개만 다룬다. 이전 호환 endpoint는 이 문서의 구현 대상이 아니다.

## 책임 분리

| 영역 | 책임 |
|---|---|
| Unreal | 월드/물리 실행, 센서 데이터 생성, Grid 전달, Action 검증, 실패 기록 |
| PythonAgent `server.py` | HTTP 수신/응답, JSON 파싱, BotPolicy 호출, 로그 출력 |
| BotPolicy | Stop, SlowDown, RePath, PathFollower 호출 순서 결정 |
| User PathFinding | A* 경로 탐색 |
| User Policy | 장애물/상태를 보고 정지, 감속, 재경로 필요 여부 판단 |
| Action helper | steering, targetSpeedKmh, brake, direction 생성 |

중요한 원칙은 하나다.

```text
Unreal은 정책을 선택하거나 길찾기를 대신하지 않는다.
Python이 잘못된 Action을 반환하면 Unreal은 조용히 보정하지 않고 거부하거나 실패로 기록한다.
```

## 폴더 역할

권장 폴더 구조는 `PythonAgent.md` 기준을 따른다.

```text
Tools/PythonAgent/
  server.py
  agent/
    __init__.py
    contract.py
    user_agent.py
    state.py
    action.py
    pathfinding/
      __init__.py
      astar.py
    policies/
      __init__.py
      stop_policy.py
      slowdown_policy.py
      repath_policy.py
      path_follower.py
```

각 파일의 책임은 아래와 같다.

| 파일 | 책임 | 사용자가 주로 수정하는가 |
|---|---|---|
| `server.py` | HTTP endpoint 처리, JSON 변환, BotPolicy 호출 | 아니오 |
| `agent/__init__.py` | 패키지 진입 편의용. 필요하면 `user_agent.py`의 `create_policy()`를 re-export한다. | 거의 아니오 |
| `agent/contract.py` | request/response 데이터 구조와 검증 규칙 | 가끔 |
| `agent/user_agent.py` | Python에서 작성한 정책 동작의 기준 파일. BotPolicy 생성과 start/decide/end 공통 판단 흐름 | 예 |
| `agent/state.py` | episode, grid, path, policy runtime 상태 저장 | 가끔 |
| `agent/action.py` | Action 생성 helper | 가끔 |
| `agent/pathfinding/__init__.py` | `pathfinding` 폴더를 Python 패키지로 표시한다. 비워둬도 된다. | 아니오 |
| `agent/pathfinding/astar.py` | A* 구현 | 예 |
| `agent/policies/__init__.py` | `policies` 폴더를 Python 패키지로 표시한다. 비워둬도 된다. | 아니오 |
| `agent/policies/stop_policy.py` | 즉시 정지 판단 | 예 |
| `agent/policies/slowdown_policy.py` | 감속 판단 | 예 |
| `agent/policies/repath_policy.py` | 재경로 판단 | 예 |
| `agent/policies/path_follower.py` | 경로를 action으로 변환 | 예 |

## BotPolicy 공개 인터페이스

사용자 정책 스크립트는 `BotPolicy` class를 제공한다.

Python method 이름은 API 단계와 같은 이름을 사용한다.

```python
class BotPolicy:
    def start(self, request: ScenarioStartRequest, state: AgentState) -> ScenarioStartResponse:
        ...

    def decide(self, request: ScenarioDecideRequest, state: AgentState) -> ScenarioDecideResponse:
        ...

    def end(self, request: ScenarioEndRequest, state: AgentState) -> ScenarioEndResponse:
        ...
```

`server.py`는 HTTP 요청을 받은 뒤 JSON을 typed request로 바꾸고, 위 method 중 하나를 호출한다.

| HTTP | BotPolicy method | 호출 시점 |
|---|---|---|
| `/scenario/start` | `BotPolicy.start()` | DeliveryBot 소환 또는 실행 시작 시 한 번 |
| `/scenario/decide` | `BotPolicy.decide()` | 주행 중 반복 |
| `/scenario/end` | `BotPolicy.end()` | 도착 또는 실패로 episode가 끝날 때 |

## ScenarioStartRequest

`/scenario/start`는 주행을 시작하기 전에 필요한 정적 정보와 초기 상태를 한 번에 전달한다.

```python
class ScenarioStartRequest:
    experimentId: int | None
    episodeId: str
    robotInstanceId: str
    start: Pose
    goal: Goal
    vehicleSpec: VehicleSpec
    lidarSpec: LidarSpec
    controlSpec: ControlSpec
    grid: GridMap
```

### Pose

```python
class Pose:
    x: float
    y: float
    z: float
    yawDegree: float
```

### Goal

```python
class Goal:
    hasGoal: bool
    x: float
    y: float
    z: float
    acceptanceRadiusCm: float
```

### VehicleSpec

```python
class VehicleSpec:
    maxSpeedKmh: float
    maxReverseSpeedKmh: float
```

초기 문서에서는 로봇 무게, 토크 같은 값도 언급하지만, 확정되지 않은 값은 `parameters` 같은 확장 필드로 둔다. Unreal이 실제 물리 실행을 담당하므로 Python에는 정책 판단과 action 생성에 필요한 값부터 넣는다.

### LidarSpec

```python
class LidarSpec:
    mode: str
    scanRangeM: float
```

`mode` 예시는 아래와 같다.

```text
OneD
TwoD
ThreeD
OneDAndTwoD
TwoDAndThreeD
All
```

`ignoreTags`, `drawDebug` 같은 Unreal 내부 표시/필터 설정은 BotPolicy 공통 인터페이스의 필수 필드로 두지 않는다. 필요하면 PythonAgent 설정 파일이나 Unreal 내부 디버그 설정으로 따로 관리한다.

### ControlSpec

```python
class ControlSpec:
    mode: str
```

현재 기본값은 `TargetSpeed`다.

### GridMap

```python
class GridMap:
    gridSizeX: int
    gridSizeY: int
    cellSizeCm: float
    cellCount: int
    originCm: Vector3
    cells: list[GridCell]
```

```python
class GridCell:
    x: int
    y: int
    areaType: str
    cost: float
    blocked: bool
    sourceCollisionProfile: str
```

`areaType` 값은 아래를 기본으로 한다.

```text
Walkable
Penalty
Blocked
```

## ScenarioStartResponse

```python
class ScenarioStartResponse:
    status: str
    accepted: bool
    pathStatus: str
    error: PolicyError | None
    debug: dict
```

`BotPolicy.start()`는 보통 아래 작업을 수행한다.

1. 이전 episode 상태를 초기화한다.
2. Grid, Start, Goal을 저장한다.
3. A*로 최초 경로를 만든다.
4. 실패하면 `PATH_NOT_FOUND` 같은 오류를 반환한다.
5. 성공하면 이후 `/scenario/decide`에서 사용할 runtime state를 준비한다.

## ScenarioDecideRequest

`/scenario/decide`는 주행 중 반복 호출되는 observation이다.

```python
class ScenarioDecideRequest:
    sequence: int
    worldTimeSeconds: float
    robotState: RobotState
    lidarRays: list[LidarRay]
    observedObjects: list[ObservedObject]
```

### RobotState

```python
class RobotState:
    x: float
    y: float
    z: float
    yawDegree: float
    speedKmh: float
```

### LidarRay

```python
class LidarRay:
    hit: bool
    rayIndex: int | None
    rayYawDegree: float
    distanceM: float
    actorName: str | None
    actorTags: list[str]
```

### ObservedObject

```python
class ObservedObject:
    actorName: str
    actorTags: list[str]
    closestDistanceM: float
    closestRayYawDegree: float
    totalHitRayCount: int
    frontHitRayCount: int
    inFront: bool
```

## ScenarioDecideResponse

`/scenario/decide` 응답은 Unreal이 실제로 실행할 Action을 포함한다.

```python
class ScenarioDecideResponse:
    sequence: int
    status: str
    action: Action | None
    error: PolicyError | None
    debug: DecisionDebug
```

### Action

```python
class Action:
    steering: float
    targetSpeedKmh: float
    brake: float
    direction: str
```

검증 규칙은 아래와 같다.

| Field | 조건 |
|---|---|
| `steering` | `-1.0 <= steering <= 1.0` |
| `targetSpeedKmh` | `0.0` 이상 |
| `targetSpeedKmh` | `Forward`이면 `vehicleSpec.maxSpeedKmh` 이하 |
| `targetSpeedKmh` | `Reverse`이면 `vehicleSpec.maxReverseSpeedKmh` 이하 |
| `brake` | `0.0 <= brake <= 1.0` |
| `direction` | `Forward` 또는 `Reverse` |

Unreal은 이 범위를 벗어난 Action을 보정하지 않는다. 거부하거나 실패로 기록한다.

### DecisionDebug

```python
class DecisionDebug:
    selectedPolicy: str
    reason: str
    pathStatus: str
    values: dict
```

모든 판단은 `debug.reason`에 이유를 남긴다.

예:

```json
{
  "selectedPolicy": "SlowDownPolicy",
  "reason": "front obstacle distance 2.1m",
  "pathStatus": "valid"
}
```

## decide 판단 순서

`BotPolicy.decide()`는 아래 순서로 판단하는 것을 기본으로 한다.

1. 현재 로봇 상태와 LiDAR 상태를 저장한다.
2. `StopPolicy`가 즉시 정지가 필요한지 확인한다.
3. 현재 경로가 막혔는지 확인하고 필요하면 `RePathPolicy`가 A*를 다시 실행한다.
4. `SlowDownPolicy`가 감속이 필요한지 확인한다.
5. 위 정책들이 action을 만들지 않았다면 `PathFollower`가 현재 경로를 따라가는 action을 만든다.
6. 어떤 단계에서든 실패하면 `status: "error"`와 `PolicyError`를 반환한다.

이 순서는 기본값이다. 사용자가 다른 정책 순서를 원하면 Unreal UI가 아니라 `agent/user_agent.py`의 `create_policy()` 또는 BotPolicy 구성에서 바꾼다.

## ScenarioEndRequest

`/scenario/end`는 episode가 끝났을 때 한 번 호출된다.

```python
class ScenarioEndRequest:
    episodeId: str
    sequence: int
    status: str
    error: PolicyError | None
    metrics: dict
    debug: dict
```

`status`는 `"ok"` 또는 `"error"`를 기본으로 한다.

도착 성공 시에는 주행 시간, 정지 횟수, near miss 수, 사용한 정책 정보 같은 값을 `metrics`나 `debug`에 담을 수 있다. 실패 시에는 실패 사유를 `error`에 담는다.

## ScenarioEndResponse

```python
class ScenarioEndResponse:
    status: str
    accepted: bool
    error: PolicyError | None
    debug: dict
```

`BotPolicy.end()`는 보통 아래 작업을 수행한다.

1. 마지막 episode 결과를 저장한다.
2. 로그를 출력한다.
3. 다음 episode를 위해 필요한 runtime state를 정리한다.

## PolicyError

```python
class PolicyError:
    code: str
    message: str
    retryable: bool
    details: dict
```

에러 code 예:

```text
PATH_NOT_FOUND
INVALID_ACTION
INVALID_START_OR_GOAL
INVALID_GRID
MISSING_REQUIRED_FIELD
UNSUPPORTED_CONTROL_MODE
POLICY_FAILED
```

A* 실패는 단순 정지로 숨기지 않는다. 반드시 `PATH_NOT_FOUND`처럼 명확한 실패 사유를 반환한다.

## 최소 구현 예시

아래 코드는 실제 동작용 전체 구현이 아니라 인터페이스 모양을 설명하기 위한 최소 예시다.

```python
class BotPolicy:
    def start(self, request, state):
        state.reset_episode(request.episodeId)
        state.grid = request.grid
        state.start = request.start
        state.goal = request.goal
        state.vehicleSpec = request.vehicleSpec
        state.lidarSpec = request.lidarSpec

        path = state.astar.find_path(request.start, request.goal, request.grid)
        if not path:
            return {
                "status": "error",
                "accepted": False,
                "pathStatus": "failed",
                "error": {
                    "code": "PATH_NOT_FOUND",
                    "message": "A* failed to find initial path",
                    "retryable": False,
                    "details": {}
                },
                "debug": {
                    "reason": "initial_path_not_found"
                }
            }

        state.path = path
        return {
            "status": "ok",
            "accepted": True,
            "pathStatus": "valid",
            "error": None,
            "debug": {
                "reason": "initial_path_ready"
            }
        }

    def decide(self, request, state):
        state.robotState = request.robotState
        state.lidarRays = request.lidarRays
        state.observedObjects = request.observedObjects

        stop_action = state.stopPolicy.evaluate(request, state)
        if stop_action:
            return stop_action

        repath_action = state.repathPolicy.evaluate(request, state)
        if repath_action:
            return repath_action

        slowdown_action = state.slowdownPolicy.evaluate(request, state)
        if slowdown_action:
            return slowdown_action

        return state.pathFollower.build_action(request, state)

    def end(self, request, state):
        state.lastEpisodeStatus = request.status
        state.lastEpisodeMetrics = request.metrics
        state.clear_runtime_state()
        return {
            "status": "ok",
            "accepted": True,
            "error": None,
            "debug": {
                "reason": "episode_end_recorded"
            }
        }
```

## 기존 초안에서 제거할 내용

아래 내용은 이전 설계에는 맞지만, `PythonAgent.md` 기준 문서와 맞지 않으므로 새 문서에서는 필수 계약에서 제외한다.

| 이전 내용 | 새 기준에서의 처리 |
|---|---|
| `Tools/PythonPolicyServer/server.py` 기준 | `Tools/PythonAgent/server.py` 기준으로 변경 |
| 여러 개로 나뉜 이전 HTTP 단계 | `start`, `decide`, `end` 3단계로 정리 |
| 중간 설정 변경 전용 단계 | 현재 구현하지 않음 |
| 정책 spec 전용 전송 단계 | 필수 API에서 제외, 필요 시 Python 설정 파일로 처리 |
| `PolicySpec`, catalog, enabledPolicies 필수 구조 | 선택 기능 또는 추후 확장으로 이동 |
| `setConfig`, `setContext`, `initialize`, `decide` 4분할 | `start`, `decide`, `end` 3단계로 단순화 |
| `ContextItem`, `ComputeJob`, `CameraFrame` | 현재 범위 밖. 필요해질 때 별도 확장 문서로 추가 |
| `episodeVersion`, `configVersion`, `contextVersion` 강제 | 현재 새 API 필수값 아님. 필요한 경우 확장 필드로 추가 |
| `drawDebug`, `ignoreTags` 필수 필드 | BotPolicy 공통 인터페이스에서 제외 |

## 완료 기준

이 인터페이스를 구현했다고 판단하는 기준은 아래와 같다.

- `server.py`가 `/scenario/start` 요청을 `BotPolicy.start()`로 전달한다.
- `BotPolicy.start()`가 Grid, Start, Goal을 저장하고 최초 A* 경로를 만든다.
- `server.py`가 `/scenario/decide` 요청을 `BotPolicy.decide()`로 전달한다.
- `BotPolicy.decide()`가 Stop, RePath, SlowDown, PathFollower 순서로 판단한다.
- `BotPolicy.decide()`가 `steering`, `targetSpeedKmh`, `brake`, `direction`을 반환한다.
- `server.py`가 `/scenario/end` 요청을 `BotPolicy.end()`로 전달한다.
- `BotPolicy.end()`가 episode 결과를 저장하고 runtime state를 정리한다.
- A* 실패 시 `PATH_NOT_FOUND` 오류를 반환한다.
- Unreal은 Python Action을 임의로 보정하지 않고 검증 결과에 따라 실행 또는 실패 처리한다.
