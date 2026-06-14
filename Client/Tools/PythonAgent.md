# DeliveryBot Python Agent

DeliveryBot Python Agent는 사용자가 직접 작성한 `Policy`와 `PathFinding` 결과를 Unreal에 전달하는 외부 의사결정 모듈이다.

Unreal은 월드 상태, Grid, LiDAR, 로봇 상태를 Python에 전달하고, Python이 반환한 Action을 실행한다.

Unreal은 정책을 선택하거나 길찾기를 대신하지 않는다. Python 응답이 잘못되었거나 실행 불가능하면 Unreal은 조용히 보정하지 않고 실패로 기록한다.

## 책임 분리

| 영역 | 책임 |
|---|---|
| Unreal | 월드/물리 실행, 센서 데이터 생성, Grid 전달, Action 검증, 실패 기록 |
| Python Server | HTTP 수신/응답, JSON 파싱, Agent 호출, 로그 출력 |
| User Policy | Stop, SlowDown, RePath 판단 |
| User PathFinding | A* 경로 탐색 |
| Path Follower | 현재 경로를 따라가기 위한 steering/speed 계산 |

## 책임 경계

Unreal은 월드, 물리, Grid/LiDAR/상태 전달, Python 응답 검증, 실패 기록을 담당한다.

Python은 Policy 선택, PathFinding, 회피/감속/정지 판단, 다음 action 생성을 담당한다.

중요한 문장:

```text
Unreal은 정책을 선택하거나 보정하지 않는다.
Python이 반환한 action이 잘못되면 Unreal은 조용히 고치지 않고 거부/실패 처리한다.
```

## 필요 조건

Python 3.10 이상을 권장한다. 별도 패키지 설치는 필요 없다.

## 기본 실행

프로젝트 루트에서 자동으로 서버가 실행되도록 한다.

기본 host/port는 아래와 같다.

```text
host 127.0.0.1
port 8000
```

## Python HTTP 분류 3단계

### 1. Start

DeliveryBot이 소환되거나 episode가 시작될 때 최초 한 번 실행한다.

전달하는 대표 데이터:

- 로봇 스펙: 최대 속도, 후진 최대 속도 등
- 정책 판단에 필요한 값: 감속 거리, 정지 거리, 제한 시간 등
- 길찾기에 필요한 정보: Grid, Start Location, Goal Location
- LiDAR 스펙 정보

### 2. Decide

DeliveryBot이 움직이기 시작하면 Goal 도착 또는 실패 전까지 반복 호출한다.

전달하는 대표 데이터:

- LiDAR 센서 정보
- 로봇 위치 정보
- 로봇 상태 정보
- 관측된 장애물/객체 정보

Python은 현재 observation을 보고 Stop, RePath, SlowDown, PathFollower 순서로 판단한다.

### 3. End

Goal 도착 또는 실패로 episode가 끝날 때 한 번 호출한다.

전달하는 대표 데이터:

- 성공/실패 상태
- 실패 사유
- 주행 시간
- near miss 횟수, 정지 횟수, 사용한 정책 등 평가용 정보

## 구현하지 않는 흐름

중간 설정 변경 API는 현재 구현하지 않는다.

따라서 별도의 중간 설정 변경 API는 없다. 주행 중 차량 스펙, LiDAR 스펙, 제어 모드가 바뀌어야 하는 기능은 이번 계약 범위 밖이다. 필요해지면 별도 설계로 추가한다.

## 공통 데이터

- `experimentId`: 하나의 시뮬레이션 실행을 식별하는 값이다. 예: `1`, `2`, `3`
- `episodeId`: 하나의 episode를 식별하는 문자열이다. 예: `episode_001`
- `robotInstanceId`: Unreal 월드 안의 DeliveryBot 인스턴스를 식별하는 문자열이다.

## 확정 API

- `POST /scenario/start`
- `POST /scenario/decide`
- `POST /scenario/end`

## Start 데이터 전송

기본 흐름은 DeliveryBot 주행 전에 정적 데이터와 초기 설정을 한 번에 보내는 것이다.

api:

```text
http://127.0.0.1:8000/scenario/start
```

## `/scenario/start` 요청 예시

```json
{
  "experimentId": 1,
  "episodeId": "episode_001",
  "robotInstanceId": "delivery_bot_01",
  "start": {
    "x": 0,
    "y": 0,
    "z": 0,
    "yawDegree": 0
  },
  "goal": {
    "hasGoal": true,
    "x": 150,
    "y": 150,
    "z": 0
  },
  "vehicleSpec": {
    "maxSpeedKmh": 10,
    "maxReverseSpeedKmh": 3
  },
  "lidarSpec": {
    "mode": "TwoD",
    "scanRangeM": 5
  },
  "controlSpec": {
    "mode": "TargetSpeed"
  },
  "grid": {
    "gridSizeX": 2,
    "gridSizeY": 2,
    "cellSizeCm": 100,
    "cellCount": 4,
    "originCm": {
      "x": 0,
      "y": 0,
      "z": 0
    },
    "cells": [
      {
        "x": 0,
        "y": 0,
        "areaType": "Walkable",
        "cost": 1,
        "blocked": false,
        "sourceCollisionProfile": "Walkable"
      },
      {
        "x": 1,
        "y": 0,
        "areaType": "Penalty",
        "cost": 5,
        "blocked": false,
        "sourceCollisionProfile": "Penalty"
      },
      {
        "x": 0,
        "y": 1,
        "areaType": "Blocked",
        "cost": 9999,
        "blocked": true,
        "sourceCollisionProfile": "Blocked"
      },
      {
        "x": 1,
        "y": 1,
        "areaType": "Walkable",
        "cost": 1,
        "blocked": false,
        "sourceCollisionProfile": "Walkable"
      }
    ]
  }
}
```

Unreal에서 생성한 Grid JSON은 매 observation마다 보내지 않고, start 단계에서 한 번 전달한다.

Python은 마지막으로 받은 grid를 메모리에 보관하고, start 단계에서 최초 A* 경로를 생성한다.

## `/scenario/start` 응답 예시

```json
{
  "status": "ok",
  "accepted": true,
  "pathStatus": "valid",
  "debug": {
    "reason": "initial_path_ready"
  }
}
```

초기 경로를 찾지 못하면 Python은 실패를 숨기지 않는다.

```json
{
  "status": "error",
  "accepted": false,
  "pathStatus": "failed",
  "error": {
    "code": "PATH_NOT_FOUND",
    "message": "A* failed to find an initial path from start to goal"
  },
  "debug": {
    "reason": "initial_path_not_found"
  }
}
```

## Decide 데이터 전송

주행 중 반복 호출한다.

Unreal은 현재 observation을 Python에 보내고, Python은 로봇이 지금 실행할 Action을 반환한다.

api:

```text
http://127.0.0.1:8000/scenario/decide
```

## `/scenario/decide` 요청 예시

```json
{
  "sequence": 12,
  "worldTimeSeconds": 3.25,
  "robotState": {
    "x": 120.0,
    "y": 80.0,
    "z": 0.0,
    "yawDegree": 15.0,
    "speedKmh": 2.5
  },
  "lidarRays": [
    {
      "rayIndex": 0,
      "rayYawDegree": 0.0,
      "distanceM": 2.1,
      "hit": true,
      "actorName": "Obstacle_01",
      "actorTags": ["ObjectType.StaticObstacle"]
    }
  ],
  "observedObjects": []
}
```

## `/scenario/decide` 응답 예시

```json
{
  "sequence": 12,
  "status": "ok",
  "action": {
    "steering": 0.15,
    "targetSpeedKmh": 3.0,
    "brake": 0.0,
    "direction": "Forward"
  },
  "debug": {
    "selectedPolicy": "SlowDownPolicy",
    "reason": "front obstacle distance 2.1m",
    "pathStatus": "valid"
  }
}
```

Unreal은 응답값을 받아 action을 검증한 뒤 그대로 실행한다.

Python이 잘못된 action을 반환하면 Unreal은 몰래 값을 고쳐서 계속 주행하지 않는다.

## End 데이터 전송

도착 또는 실패로 episode가 끝날 때 한 번 호출한다.

api:

```text
http://127.0.0.1:8000/scenario/end
```

## `/scenario/end` 요청 예시

```json
{
  "episodeId": "episode_001",
  "sequence": 13,
  "status": "error",
  "error": {
    "code": "PATH_NOT_FOUND",
    "message": "A* failed to find a path from current cell to goal cell"
  },
  "metrics": {
    "elapsedTimeSeconds": 12.4,
    "stopCount": 2,
    "obstacleWarningCount": 0
  },
  "debug": {
    "selectedPolicy": "RePathPolicy",
    "pathStatus": "failed"
  }
}
```

## `/scenario/end` 응답 예시

```json
{
  "status": "ok",
  "accepted": true,
  "debug": {
    "reason": "episode_end_recorded"
  }
}
```

## 폴더 구조

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

## 파일별 책임

| 파일 | 책임 |
|---|---|
| `server.py` | HTTP 서버. 사용자가 수정하지 않는 영역 |
| `agent/__init__.py` | `agent` 패키지의 대표 진입점. 필요하면 `user_agent.py`의 생성 함수를 re-export한다. |
| `agent/contract.py` | Request/Response 데이터 구조 |
| `agent/user_agent.py` | Python에서 작성한 정책 동작의 기준 파일. BotPolicy 생성과 start/decide/end 공통 흐름 |
| `agent/state.py` | episode/path/policy 상태 |
| `agent/action.py` | Action 생성 유틸 |
| `agent/pathfinding/__init__.py` | `pathfinding` 폴더를 Python 패키지로 표시한다. 비워둬도 된다. |
| `agent/pathfinding/astar.py` | 사용자가 구현하는 A* |
| `agent/policies/__init__.py` | `policies` 폴더를 Python 패키지로 표시한다. 비워둬도 된다. |
| `agent/policies/stop_policy.py` | 즉시 정지 판단 |
| `agent/policies/slowdown_policy.py` | 감속 판단 |
| `agent/policies/repath_policy.py` | 재경로 탐색 판단 |
| `agent/policies/path_follower.py` | 경로를 action으로 변환 |

## 구현 원칙

1. Unreal은 policy를 선택하지 않는다. PythonAgent는 Python 코드에 작성된 BotPolicy대로 동작한다.
2. `server.py`는 HTTP 어댑터 역할만 하며 정책 판단을 넣지 않는다.
3. Python 쪽 동작 기준은 `agent/user_agent.py`의 `create_policy()`와 `BotPolicy`로 둔다.
4. `agent/__init__.py`는 패키지 편의용이며 정책 선택 기준으로 사용하지 않는다.
5. 정책 판단 세부 로직은 `user_agent.py`, `policies/*`, `pathfinding/astar.py`에 둔다.
6. 모든 판단 결과는 `debug.reason`에 남긴다.
7. 경로 실패, Action 오류, 필수 데이터 누락은 조용히 무시하지 않는다.
8. 단위는 명확히 고정한다. 위치는 cm, 각도는 degree, 속도는 km/h를 기본으로 사용한다.

## 하지 말아야 할 것

- Unreal에서 Python 대신 길찾기를 수행하지 않는다.
- Unreal에서 Stop/SlowDown/RePath 정책을 판단하지 않는다.
- Python이 잘못된 Action을 반환했을 때 Unreal이 몰래 보정해서 계속 주행하지 않는다.
- 사용자 Policy를 Unreal UI 선택지로 만들지 않는다.
- Unreal 요청 body에 policy 이름, policy spec, catalog 선택값을 넣지 않는다.
- HTTP 서버 코드와 사용자 알고리즘 코드를 한 파일에 섞지 않는다.
- A* 실패를 단순 정지로 숨기지 않는다. 반드시 실패 사유를 반환한다.
- 현재 범위에서는 별도 설정 변경 API를 만들지 않는다.

## 완료 기준

구현이 완료되었다고 판단하는 기준은 아래와 같다.

- Python Agent가 `127.0.0.1:8000`에서 실행된다.
- Unreal이 `/scenario/start`를 한 번 호출한다.
- Python이 Grid, Start, Goal을 저장한다.
- Python이 A* 경로를 생성한다.
- Unreal이 `/scenario/decide`를 반복 호출한다.
- Python이 `steering`, `targetSpeedKmh`, `brake`, `direction`을 반환한다.
- 장애물이 가까우면 `SlowDownPolicy` 또는 `StopPolicy`가 선택된다.
- 현재 경로가 막히면 `RePathPolicy`가 A*를 다시 실행한다.
- A* 실패 시 Python은 `PATH_NOT_FOUND` 오류를 반환한다.
- episode가 끝나면 Unreal이 `/scenario/end`를 호출한다.
- Unreal은 Python Action을 임의로 보정하지 않는다.
