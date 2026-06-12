# PythonAgent 통신 JSON 형식

이 문서는 Unreal DeliveryBot과 `Tools/PythonAgent/server.py`가 HTTP로 주고받는 JSON 계약을 정리한다.
현재 Unreal은 모든 `POST` 요청을 envelope 형태로 보내고, Python은 같은 envelope의 `response` 영역을 채워서 반환한다.

## 핵심 규칙

- 실제 주행에 사용되는 값은 `response.action`이다.
- `response.debug`는 시각화, 로그, 진단용이다. 필드를 추가해도 되지만 기존 필드의 이름과 타입은 함부로 바꾸지 않는다.
- Python 서버는 envelope 안의 `request`를 우선 읽는다. 테스트 편의를 위해 raw request도 일부 허용하지만, Unreal 런타임은 envelope를 보낸다.
- 통신 계약을 깨는 변경이면 `version`을 올리고 Unreal/Python 양쪽을 같이 수정한다.

## 공통 Envelope

`POST /scenario/start`, `POST /scenario/decide`, `POST /scenario/end`가 같은 envelope를 사용한다.

```json
{
  "schema": "delivery_bot_python_message",
  "version": 1,
  "type": "scenario_decide",
  "request": {},
  "response": {
    "status": "pending",
    "action": null,
    "error": null,
    "debug": {}
  }
}
```

| 필드 | 구분 | 설명 |
| --- | --- | --- |
| `schema` | 고정 | 현재 값은 `delivery_bot_python_message` |
| `version` | 고정 | 현재 값은 `1` |
| `type` | 고정 | `scenario_start`, `scenario_decide`, `scenario_end` |
| `request` | 고정 | Unreal이 Python으로 보내는 입력 |
| `response` | 고정 | Python이 처리 결과를 채워 반환하는 영역 |
| `response.status` | 고정 | `pending`, `ok`, `error` |
| `response.action` | 고정 | `/scenario/decide`에서 Unreal이 실제 이동 명령으로 사용하는 값 |
| `response.error` | 고정 | 실패 시 에러 정보 |
| `response.debug` | 확장 가능 | 경로, 정책 이유, NearMiss 같은 디버그 정보 |

## GET /health

서버 생존 확인용이다. 이 요청은 envelope를 사용하지 않는다.

요청:

```http
GET /health
```

응답:

```json
{
  "status": "ok",
  "server": "PythonAgent",
  "version": "0.1"
}
```

## POST /scenario/start

episode 시작 시 한 번 호출한다. Python은 이 요청으로 시작 위치, 목표, grid, 차량/라이다/제어 설정을 받고 최초 경로를 만든다.

### 요청 예시

```json
{
  "schema": "delivery_bot_python_message",
  "version": 1,
  "type": "scenario_start",
  "request": {
    "experimentId": "unreal_runtime",
    "episodeId": "62834EB0-4749-AAFC-91C8-4ABC942ED264",
    "robotInstanceId": "BP_DeliveryBot_C_0",
    "start": {
      "x": -500.0,
      "y": 0.0,
      "z": 11.5,
      "yawDegree": 0.0
    },
    "goal": {
      "hasGoal": true,
      "x": 500.0,
      "y": 0.0,
      "z": 0.0,
      "acceptanceRadiusCm": 80.0
    },
    "grid": {
      "gridSizeX": 28,
      "gridSizeY": 30,
      "cellSizeCm": 50.0,
      "cellCount": 840,
      "originCm": {
        "x": -700.0,
        "y": -750.0,
        "z": 0.0,
        "yawDegree": 0.0
      },
      "cells": [
        {
          "x": 0,
          "y": 0,
          "areaType": "Walkable",
          "cost": 1.0,
          "blocked": false,
          "sourceCollisionProfile": "Walkable"
        },
        {
          "x": 1,
          "y": 0,
          "areaType": "Blocked",
          "cost": 3.4028234663852886e+38,
          "blocked": true,
          "sourceCollisionProfile": "NoGroundHit"
        }
      ]
    },
    "vehicleSpec": {
      "maxSpeedKmh": 7.0,
      "maxReverseSpeedKmh": 3.0
    },
    "lidarSpec": {
      "scanRangeM": 6.0,
      "angleStepDegree": 5.0,
      "sensorHeightM": 0.07,
      "frontHalfAngleDegree": 25.0,
      "stopDistanceM": 1.4,
      "nearMissDistanceM": 2.0,
      "slowDownDistanceM": 4.8,
      "collisionStopHalfAngleDegree": 8.0,
      "collisionStopDistanceM": 0.45
    },
    "controlSpec": {
      "targetSpeedKmh": 4.5,
      "lookAheadDistanceM": 1.2,
      "pathPointAcceptanceDistanceM": 0.45,
      "steeringSensitivity": 1.1,
      "steeringFullScaleDegree": 80.0,
      "maxSteering": 0.5,
      "maxSteeringDelta": 0.09,
      "minTurnSpeedKmh": 0.8,
      "obstacleSlowSpeedKmh": 1.0,
      "nearMissDistanceM": 2.0,
      "collisionStopHalfAngleDegree": 8.0,
      "collisionStopDistanceM": 0.45,
      "softStopBrakeInput": 0.18,
      "emergencyBrakeInput": 0.45,
      "recoverySpeedKmh": 1.2
    }
  },
  "response": {
    "status": "pending",
    "action": null,
    "error": null,
    "debug": {}
  }
}
```

### 요청 필드

| 필드 | 구분 | 설명 |
| --- | --- | --- |
| `request.experimentId` | 고정 | 실험 또는 런타임 묶음 ID. 현재 Unreal 기본값은 `unreal_runtime` |
| `request.episodeId` | 고정 | episode 하나를 구분하는 ID |
| `request.robotInstanceId` | 고정 | 현재 DeliveryBot Actor 이름 |
| `request.start` | 고정 | 시작 위치와 yaw. 단위는 cm, degree |
| `request.goal` | 고정 | 목표 위치와 도착 판정 반경. 단위는 cm |
| `request.grid` | 고정 | Python A* 경로 탐색에 사용하는 grid |
| `request.grid.cells[]` | 고정 | 각 cell의 좌표, area, cost, blocked 여부 |
| `request.vehicleSpec.maxSpeedKmh` | 확장 가능 | 차량 최대 전진 속도 |
| `request.vehicleSpec.maxReverseSpeedKmh` | 확장 가능 | 차량 최대 후진 속도 |
| `request.lidarSpec.scanRangeM` | 확장 가능 | 라이다 전체 탐지 거리 |
| `request.lidarSpec.angleStepDegree` | 확장 가능 | 2D 라이다 ray 간격 |
| `request.lidarSpec.sensorHeightM` | 확장 가능 | 라이다 센서 높이 |
| `request.lidarSpec.frontHalfAngleDegree` | 확장 가능 | 전방으로 보는 좌우 반각 |
| `request.lidarSpec.stopDistanceM` | 확장 가능 | 일반 전방 정지 거리 |
| `request.lidarSpec.nearMissDistanceM` | 확장 가능 | actor별 NearMiss 기록 거리. `stopDistanceM < nearMissDistanceM < slowDownDistanceM` 순서를 유지한다. |
| `request.lidarSpec.slowDownDistanceM` | 확장 가능 | 감속/회피 판단 거리 |
| `request.lidarSpec.collisionStopHalfAngleDegree` | 확장 가능 | 중앙 충돌 코리더 반각. 이 각도 밖의 전방 hit는 NearMiss 후보가 될 수 있다. |
| `request.lidarSpec.collisionStopDistanceM` | 확장 가능 | 각도와 무관하게 너무 가까우면 충돌 위험으로 보는 거리 |
| `request.controlSpec.targetSpeedKmh` | 확장 가능 | 기본 경로 추종 목표 속도 |
| `request.controlSpec.lookAheadDistanceM` | 확장 가능 | 경로 추종 lookahead 거리 |
| `request.controlSpec.pathPointAcceptanceDistanceM` | 확장 가능 | path index 진행 판정 거리 |
| `request.controlSpec.steeringSensitivity` | 확장 가능 | Python 조향 민감도 |
| `request.controlSpec.steeringFullScaleDegree` | 확장 가능 | 조향 입력 1.0에 대응하는 yaw error 기준 |
| `request.controlSpec.maxSteering` | 확장 가능 | Python이 반환할 최대 조향 절대값 |
| `request.controlSpec.maxSteeringDelta` | 확장 가능 | decide 1회당 조향 변화 제한 |
| `request.controlSpec.minTurnSpeedKmh` | 확장 가능 | 큰 조향 시에도 유지할 최소 목표 속도 |
| `request.controlSpec.obstacleSlowSpeedKmh` | 확장 가능 | 장애물 감속 구간의 저속 기준 |
| `request.controlSpec.nearMissDistanceM` | 확장 가능 | Python 정책이 직접 사용하는 NearMiss 거리 |
| `request.controlSpec.collisionStopHalfAngleDegree` | 확장 가능 | Python 정책이 직접 사용하는 중앙 충돌 코리더 반각 |
| `request.controlSpec.collisionStopDistanceM` | 확장 가능 | Python 정책이 직접 사용하는 근접 충돌 거리 |
| `request.controlSpec.softStopBrakeInput` | 확장 가능 | 부드러운 정지용 brake 기준 |
| `request.controlSpec.emergencyBrakeInput` | 확장 가능 | 비상 정지용 brake 기준 |
| `request.controlSpec.recoverySpeedKmh` | 확장 가능 | RePathPolicy recovery reverse 속도 |

주의:

- `nearMissDistanceM`, `collisionStopHalfAngleDegree`, `collisionStopDistanceM`은 현재 `lidarSpec`과 `controlSpec`에 모두 들어간다. Python 정책은 `controlSpec` 값을 우선 사용한다.
- `draw_near_miss_debug`는 DeliveryBotSetup JSON 설정값이며 Python 통신 payload에는 포함되지 않는다.

### 정상 응답 예시

```json
{
  "schema": "delivery_bot_python_message",
  "version": 1,
  "type": "scenario_start",
  "request": {},
  "response": {
    "status": "ok",
    "accepted": true,
    "pathStatus": "valid",
    "debug": {
      "reason": "initial_path_ready",
      "pathLength": 22
    }
  }
}
```

### 실패 응답 예시

```json
{
  "schema": "delivery_bot_python_message",
  "version": 1,
  "type": "scenario_start",
  "request": {},
  "response": {
    "status": "error",
    "accepted": false,
    "pathStatus": "failed",
    "error": {
      "code": "PATH_NOT_FOUND",
      "message": "goal_cell_blocked"
    },
    "debug": {
      "reason": "goal_cell_blocked"
    }
  }
}
```

## POST /scenario/decide

decision tick마다 호출한다. Unreal은 현재 로봇 상태, 라이다 ray, 관측 object 요약을 보내고 Python은 이동 action을 반환한다.

### 요청 예시

```json
{
  "schema": "delivery_bot_python_message",
  "version": 1,
  "type": "scenario_decide",
  "request": {
    "sequence": 75,
    "runTimeSeconds": 12.5,
    "robotState": {
      "x": -120.0,
      "y": 15.0,
      "z": 11.5,
      "yawDegree": 2.0,
      "speedKmh": 3.2
    },
    "lidarRays": [
      {
        "hit": true,
        "distanceM": 1.37,
        "rayIndex": 61,
        "rayYawDegree": 12.0,
        "actorName": "barrier_01",
        "actorTags": ["StaticObstacle"]
      },
      {
        "hit": false,
        "distanceM": 6.0,
        "rayIndex": 62,
        "rayYawDegree": 17.0,
        "actorName": "",
        "actorTags": []
      }
    ],
    "observedObjects": [
      {
        "actorName": "barrier_01",
        "actorTags": ["StaticObstacle"],
        "closestDistanceM": 1.37,
        "closestRayYawDegree": 12.0,
        "totalHitRayCount": 4,
        "frontHitRayCount": 2,
        "inFront": true
      }
    ]
  },
  "response": {
    "status": "pending",
    "action": null,
    "error": null,
    "debug": {}
  }
}
```

### 요청 필드

| 필드 | 구분 | 설명 |
| --- | --- | --- |
| `request.sequence` | 고정 | decide 요청 순서 번호 |
| `request.runTimeSeconds` | 고정 | Unreal world time |
| `request.robotState.x/y/z` | 고정 | 로봇 위치. 단위는 cm |
| `request.robotState.yawDegree` | 고정 | 로봇 yaw. 단위는 degree |
| `request.robotState.speedKmh` | 고정 | 현재 속도. 단위는 km/h |
| `request.lidarRays[].hit` | 고정 | 해당 ray가 유효 actor를 맞췄는지 여부 |
| `request.lidarRays[].distanceM` | 고정 | ray hit 또는 ray 끝까지의 거리. 단위는 m |
| `request.lidarRays[].rayIndex` | 고정 | ray index. 없으면 `null` 가능 |
| `request.lidarRays[].rayYawDegree` | 고정 | 로봇 기준 signed local yaw. 왼쪽/오른쪽 판단에 사용 |
| `request.lidarRays[].actorName` | 확장 가능 | hit actor 이름. miss면 빈 문자열 가능 |
| `request.lidarRays[].actorTags` | 확장 가능 | hit actor tag 목록 |
| `request.observedObjects[]` | 확장 가능 | ray를 actor 단위로 묶은 관측 요약 |

### 정상 응답 예시

```json
{
  "schema": "delivery_bot_python_message",
  "version": 1,
  "type": "scenario_decide",
  "request": {},
  "response": {
    "sequence": 75,
    "status": "ok",
    "action": {
      "steering": -0.12,
      "targetSpeedKmh": 3.8,
      "brake": 0.0,
      "direction": "Forward"
    },
    "debug": {
      "selectedPolicy": "PathFollower",
      "reason": "follow_path",
      "pathStatus": "valid",
      "pathIndex": 4,
      "pathLength": 22,
      "pathWorldPoints": [
        {
          "x": -500.0,
          "y": 0.0,
          "z": 11.5
        },
        {
          "x": -450.0,
          "y": 0.0,
          "z": 11.5
        }
      ],
      "targetPathIndex": 5,
      "targetWorldPoint": {
        "x": -250.0,
        "y": 50.0,
        "z": 11.5
      },
      "closestPathDistanceCm": 42.0,
      "maxPathErrorCm": 120.0,
      "nearMissCount": 0,
      "lastNearMissCell": null,
      "lastNearMissSource": "",
      "blockedCorridorCellCount": 0,
      "recoveryUntilSeconds": 0.0
    }
  }
}
```

### NearMiss 통과 응답 예시

NearMiss는 모든 hit ray를 검사해서 actor별 1회만 기록한다.
기본 공식은 아래와 같다.

```text
hit == true
AND distanceM <= nearMissDistanceM
AND NOT (distanceM <= stopDistanceM AND isCollisionStopRay)
AND same actor/source not recorded yet
```

`isCollisionStopRay`는 기존 중앙 충돌 판정이다.

```text
abs(rayYawDegree) <= collisionStopHalfAngleDegree
OR distanceM <= collisionStopDistanceM
```

즉 `StopDistance < NearMissDistance < SlowDownDistance` 순서에서 실제 stop으로 보는 ray는 먼저 제외하고, 그 다음 NearMiss를 actor별로 기록한다.

```json
{
  "schema": "delivery_bot_python_message",
  "version": 1,
  "type": "scenario_decide",
  "request": {},
  "response": {
    "sequence": 76,
    "status": "ok",
    "action": {
      "steering": -0.24,
      "targetSpeedKmh": 3.4,
      "brake": 0.0,
      "direction": "Forward"
    },
    "debug": {
      "selectedPolicy": "PathFollower",
      "reason": "front_obstacle_near_miss_pass",
      "pathStatus": "valid",
      "pathIndex": 4,
      "pathLength": 22,
      "nearMissCount": 1,
      "lastNearMissCell": null,
      "lastNearMissSource": "barrier_01",
      "blockedCorridorCellCount": 0,
      "recoveryUntilSeconds": 0.0
    }
  }
}
```

### 감속/정지/후진 응답 예시

감속:

```json
{
  "response": {
    "sequence": 77,
    "status": "ok",
    "action": {
      "steering": -0.18,
      "targetSpeedKmh": 1.2,
      "brake": 0.0,
      "direction": "Forward"
    },
    "debug": {
      "selectedPolicy": "PathFollower",
      "reason": "front_obstacle_slowdown"
    }
  }
}
```

부드러운 정지:

```json
{
  "response": {
    "sequence": 78,
    "status": "ok",
    "action": {
      "steering": 0.0,
      "targetSpeedKmh": 0.0,
      "brake": 1.0,
      "direction": "Forward"
    },
    "debug": {
      "selectedPolicy": "PathFollower",
      "reason": "front_obstacle_soft_stop"
    }
  }
}
```

repath recovery 후진:

```json
{
  "response": {
    "sequence": 79,
    "status": "ok",
    "action": {
      "steering": 0.0,
      "targetSpeedKmh": 1.2,
      "brake": 0.0,
      "direction": "Reverse"
    },
    "debug": {
      "selectedPolicy": "RePathPolicy",
      "reason": "recovery_reverse"
    }
  }
}
```

### 응답 필드

| 필드 | 구분 | 설명 |
| --- | --- | --- |
| `response.sequence` | 고정 | 처리한 decide sequence |
| `response.status` | 고정 | 정상 처리 시 `ok` |
| `response.action.steering` | 고정 | 조향 입력. Unreal에서 `-1.0`부터 `1.0`으로 clamp |
| `response.action.targetSpeedKmh` | 고정 | 목표 속도. 단위는 km/h |
| `response.action.brake` | 고정 | brake 입력. Unreal에서 `0.0`부터 `1.0`으로 clamp |
| `response.action.direction` | 고정 | `Forward` 또는 `Reverse` |
| `response.debug.selectedPolicy` | 확장 가능 | action을 반환한 Python policy 이름 |
| `response.debug.reason` | 확장 가능 | policy가 선택한 이유 |
| `response.debug.pathStatus` | 확장 가능 | `valid` 또는 `empty` |
| `response.debug.pathIndex` | 확장 가능 | 현재 따라가는 path index |
| `response.debug.pathLength` | 확장 가능 | 현재 path cell 개수 |
| `response.debug.pathWorldPoints` | 확장 가능 | Unreal debug line에 사용하는 path world 좌표 |
| `response.debug.targetPathIndex` | 확장 가능 | 실제 추종 target index |
| `response.debug.targetWorldPoint` | 확장 가능 | 실제 추종 target world 좌표 |
| `response.debug.closestPathDistanceCm` | 확장 가능 | 로봇과 경로 선분 사이 최소 거리 |
| `response.debug.maxPathErrorCm` | 확장 가능 | 허용 가능한 경로 이탈 거리 |
| `response.debug.nearMissCount` | 확장 가능 | 현재 episode에서 actor/source별로 기록한 NearMiss 횟수 |
| `response.debug.lastNearMissCell` | 확장 가능 | grid cell 기반 NearMiss일 때 마지막 cell. 라이다 기반이면 `null` |
| `response.debug.lastNearMissSource` | 확장 가능 | 마지막 NearMiss 원인 actor 또는 source 이름 |
| `response.debug.blockedCorridorCellCount` | 확장 가능 | 최근 blocked corridor cell 개수 |
| `response.debug.recoveryUntilSeconds` | 확장 가능 | recovery reverse가 유지되는 Unreal time |

현재 자주 쓰는 `reason` 값:

```text
follow_path
front_obstacle_near_miss_pass
front_obstacle_slowdown
front_obstacle_soft_stop
path_deviation_repath_required
dynamic_repath_ready
dynamic_repath_near_obstacle
recovery_reverse
goal_reached
path_finished
no_path
missing_grid
robot_outside_grid_bounds
```

## POST /scenario/end

목표 도착, 실패, timeout 등 episode 종료 시 호출한다.

### 요청 예시

```json
{
  "schema": "delivery_bot_python_message",
  "version": 1,
  "type": "scenario_end",
  "request": {
    "experimentId": "unreal_runtime",
    "episodeId": "62834EB0-4749-AAFC-91C8-4ABC942ED264",
    "robotInstanceId": "BP_DeliveryBot_C_0",
    "sequence": 120,
    "status": "arrived",
    "metrics": {},
    "debug": {
      "endSource": "UScenarioEvaluationSubsystem"
    }
  },
  "response": {
    "status": "pending",
    "action": null,
    "error": null,
    "debug": {}
  }
}
```

### 요청 필드

| 필드 | 구분 | 설명 |
| --- | --- | --- |
| `request.experimentId` | 고정 | start와 같은 experiment ID |
| `request.episodeId` | 고정 | 종료할 episode ID |
| `request.robotInstanceId` | 고정 | 종료 대상 로봇 ID |
| `request.sequence` | 고정 | 마지막 decide sequence |
| `request.status` | 고정 | 예: `arrived`, `failed`, `timeout` |
| `request.error` | 확장 가능 | 실패 종료 원인. 현재 Unreal 기본 payload에서는 생략 가능 |
| `request.metrics` | 확장 가능 | Unreal 측 결과 metric |
| `request.debug` | 확장 가능 | 종료 원인 추적용 디버그 정보 |

### 정상 응답 예시

```json
{
  "schema": "delivery_bot_python_message",
  "version": 1,
  "type": "scenario_end",
  "request": {},
  "response": {
    "status": "ok",
    "accepted": true,
    "metrics": {
      "nearMissCount": 1,
      "stopCount": 0,
      "repathCount": 1,
      "slowdownCount": 3
    },
    "debug": {
      "reason": "episode_end_recorded",
      "episodeId": "62834EB0-4749-AAFC-91C8-4ABC942ED264",
      "status": "arrived",
      "stopCount": 0,
      "repathCount": 1,
      "slowdownCount": 3,
      "nearMissCount": 1,
      "lastNearMissCell": null,
      "lastNearMissSource": "barrier_01",
      "blockedCorridorCellCount": 0
    }
  }
}
```

## Error 형식

Python 처리 중 예외가 발생하면 HTTP status code가 `500`이 될 수 있고, envelope의 `response.status`는 `error`가 된다.
정책 실패처럼 서버 예외가 아닌 실패는 HTTP `200` 안에서 `response.status: "error"`로 올 수 있다.

```json
{
  "schema": "delivery_bot_python_message",
  "version": 1,
  "type": "scenario_decide",
  "request": {},
  "response": {
    "status": "error",
    "error": {
      "code": "SERVER_ERROR",
      "message": "'PathFollower' object has no attribute 'someField'"
    },
    "debug": {
      "reason": "server_exception"
    }
  }
}
```

주요 에러 코드:

```text
SERVER_ERROR
PATH_NOT_FOUND
POLICY_FAILED
NOT_FOUND
```

## 고정 필드 목록

다음 필드는 Unreal과 Python 양쪽이 직접 읽으므로 이름과 타입을 유지해야 한다.

공통:

```text
schema
version
type
request
response
response.status
response.action
response.error
response.debug
```

`/scenario/start`:

```text
request.experimentId
request.episodeId
request.robotInstanceId
request.start.x
request.start.y
request.start.z
request.start.yawDegree
request.goal.hasGoal
request.goal.x
request.goal.y
request.goal.z
request.goal.acceptanceRadiusCm
request.grid.gridSizeX
request.grid.gridSizeY
request.grid.cellSizeCm
request.grid.cellCount
request.grid.originCm
request.grid.cells[].x
request.grid.cells[].y
request.grid.cells[].areaType
request.grid.cells[].cost
request.grid.cells[].blocked
request.grid.cells[].sourceCollisionProfile
```

`/scenario/decide`:

```text
request.sequence
request.runTimeSeconds
request.robotState.x
request.robotState.y
request.robotState.z
request.robotState.yawDegree
request.robotState.speedKmh
request.lidarRays[].hit
request.lidarRays[].distanceM
request.lidarRays[].rayIndex
request.lidarRays[].rayYawDegree
request.lidarRays[].actorName
request.lidarRays[].actorTags
response.sequence
response.status
response.action.steering
response.action.targetSpeedKmh
response.action.brake
response.action.direction
```

`/scenario/end`:

```text
request.experimentId
request.episodeId
request.robotInstanceId
request.sequence
request.status
```

## 확장 가능한 위치

기존 필드를 제거하거나 타입을 바꾸지 않는 조건에서 아래 영역은 확장하기 좋다.

| 목적 | 권장 위치 |
| --- | --- |
| 목표 속도, 조향 민감도, 회복 속도 같은 정책 설정 | `request.controlSpec` |
| 라이다 거리, 전방 각도, NearMiss/충돌 경계 | `request.lidarSpec` |
| 차량 물리/속도 제약 | `request.vehicleSpec` |
| actor 단위 관측 요약 | `request.observedObjects` |
| 경로 시각화, 정책 선택 이유, NearMiss 카운트 | `response.debug` |
| episode 종료 metric | `request.metrics`, `/scenario/end`의 `response.metrics` |

## Version을 올려야 하는 경우

다음 변경은 기존 Unreal/Python 중 한쪽을 깨뜨릴 수 있으므로 `version`을 올리는 것이 안전하다.

- `response.action` 필드 이름 변경
- `response.action.direction` 값 체계 변경
- `request.robotState`, `request.grid.cells`, `request.lidarRays`의 필수 필드 제거 또는 타입 변경
- `response.status` 값 체계 변경
- 기존 고정 필드의 의미 변경

반대로 `request.controlSpec`, `request.lidarSpec`, `response.debug`에 새 optional 필드를 추가하는 것은 보통 `version`을 올리지 않아도 된다.

## 관련 DeliveryBotSetup JSON 메모

아래 값은 Python 통신 payload가 아니라 `Json/Input/DeliveryBotSetup*.json`에서 읽는 로컬 설정이다.
NearMiss 범위를 화면에서 보고 싶을 때 사용한다.

```json
{
  "robot": {
    "lidar": {
      "draw_debug": true,
      "draw_near_miss_debug": true,
      "front_half_angle_degree": 25.0,
      "near_miss_distance_m": 2.0,
      "collision_stop_half_angle_degree": 8.0,
      "collision_stop_distance_m": 0.45,
      "slow_down_distance_m": 4.8
    }
  }
}
```

색상 기준:

```text
Cyan   = NearMiss 후보 범위
Orange = 중앙 충돌/감속 코리더
Red    = 매우 가까운 충돌 반경
```
