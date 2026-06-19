# PythonAgent 통신 JSON 형식

이 문서는 Unreal DeliveryBot과 `Client/Resources/policy-runtime.py`가 HTTP로 주고받는 JSON 계약을 정리한다.
현재 Unreal은 모든 `POST` 요청을 envelope 형태로 보내고, Python은 응답 envelope의 `response` 영역에 처리 결과를 채워 반환한다.

## 핵심 규칙

- 실제 주행에 사용되는 값은 `response.action`이다.
- `response.debug`는 시각화, 로그, 진단용이다. 필드를 추가해도 되지만 기존 필드의 이름과 타입은 함부로 바꾸지 않는다.
- Python 서버는 envelope 안의 `request`를 우선 읽는다. 테스트 편의를 위해 raw request도 일부 허용하지만, Unreal 런타임은 envelope를 보낸다.
- Unreal 요청 payload에는 `response`가 없다.
- `/scenario/decide` 응답만 Unreal이 보낸 `request`를 그대로 유지하고 `response`를 추가한 같은 JSON 형식을 사용한다.
- `/scenario/start`, `/scenario/end`는 기존처럼 요청 JSON과 응답 JSON을 분리해서 다룬다.
- 통신 계약을 깨는 변경이면 `version`을 올리고 Unreal/Python 양쪽을 같이 수정한다.

## 공통 Envelope

`POST /scenario/start`, `POST /scenario/decide`, `POST /scenario/end`는 같은 envelope 필드를 사용한다.
다만 request를 response에 보존해서 하나의 JSON으로 묶는 규칙은 `/scenario/decide`에만 적용한다.

예: `/scenario/decide` Unreal 요청:

```json
{
  "schema": "delivery_bot_python_message",
  "version": 1,
  "type": "scenario_decide",
  "request": {
    "sequence": 75
  }
}
```

예: `/scenario/decide` Python 응답:

```json
{
  "schema": "delivery_bot_python_message",
  "version": 1,
  "type": "scenario_decide",
  "request": {
    "sequence": 75
  },
  "response": {
    "status": "ok"
  }
}
```

| 필드 | 구분 | 설명 |
| --- | --- | --- |
| `schema` | 고정 | 현재 값은 `delivery_bot_python_message` |
| `version` | 고정 | 현재 값은 `1` |
| `type` | 고정 | `scenario_start`, `scenario_decide`, `scenario_end` |
| `request` | 고정 | Unreal이 Python으로 보내는 입력. `/scenario/decide` 응답에서는 원본 값이 유지된다. |
| `response` | 고정 | Unreal 요청에는 없고 Python 응답에서 추가되는 처리 결과 영역 |
| `response.status` | 고정 | `ok`, `error` |
| `response.action` | 고정 | `/scenario/decide`에서 Unreal이 실제 이동 명령으로 사용하는 값 |
| `response.error` | 고정 | 실패 시 에러 정보 |
| `response.debug` | 확장 가능 | 경로, 정책 이유, obstacle warning 같은 디버그 정보 |

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
      "z": 0.0
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
    "robotSpec": {
      "maxSpeedKmh": 7.0,
      "bodyLengthCm": 72.0,
      "bodyWidthCm": 48.0,
      "bodyHeightCm": 55.0,
      "wheelBaseCm": 42.0,
      "turningRadiusCm": 120.0
    },
    "driveSpec": {
      "accelerationRateKmhPerSecond": 1.2,
      "decelerationRateKmhPerSecond": 0.9,
      "steeringInputRatePerSecond": 3.2,
      "throttleInputRatePerSecond": 0.28,
      "brakeInputRatePerSecond": 0.35,
      "stopBrakeInput": 0.18
    },
    "lidarSpec": {
      "scanRangeM": 6.0,
      "angleStepDegree": 5.0,
      "sensorHeightM": 0.07
    }
  }
}
```

### 요청 필드

| 필드 | 구분 | 설명 |
| --- | --- | --- |
| `request.robotInstanceId` | 고정 | 현재 DeliveryBot Actor 이름 |
| `request.start` | 고정 | 시작 위치와 yaw. 단위는 cm, degree |
| `request.goal` | 고정 | 목표 위치와 도착 판정 반경. 단위는 cm |
| `request.grid` | 고정 | Python A* 경로 탐색에 사용하는 grid |
| `request.grid.cells[]` | 고정 | 각 cell의 좌표, area, cost, blocked 여부 |
| `request.robotSpec.maxSpeedKmh` | 확장 가능 | 로봇 최대 전진 속도 |
| `request.robotSpec.bodyLengthCm` | 확장 가능 | 로봇 본체 길이. Unreal의 로봇 충돌 박스 크기에서 가져온다. |
| `request.robotSpec.bodyWidthCm` | 확장 가능 | 로봇 본체 폭. Unreal의 로봇 충돌 박스 크기에서 가져온다. |
| `request.robotSpec.bodyHeightCm` | 확장 가능 | 로봇 본체 높이. Unreal의 로봇 충돌 박스 크기에서 가져온다. |
| `request.robotSpec.wheelBaseCm` | 확장 가능 | 앞/뒤 바퀴 축 사이 거리 또는 그에 준하는 로봇 주행 기준 길이 |
| `request.robotSpec.turningRadiusCm` | 확장 가능 | 로봇 최소 회전 반경 |
| `request.driveSpec.accelerationRateKmhPerSecond` | 확장 가능 | 목표 속도 증가율 |
| `request.driveSpec.decelerationRateKmhPerSecond` | 확장 가능 | 목표 속도 감소율 |
| `request.driveSpec.steeringInputRatePerSecond` | 확장 가능 | Unreal 차량 입력에 적용되는 조향 입력 변화율 |
| `request.driveSpec.throttleInputRatePerSecond` | 확장 가능 | Unreal 차량 입력에 적용되는 스로틀 입력 변화율 |
| `request.driveSpec.brakeInputRatePerSecond` | 확장 가능 | Unreal 차량 입력에 적용되는 브레이크 입력 변화율 |
| `request.driveSpec.stopBrakeInput` | 확장 가능 | 정지 시 사용하는 기본 브레이크 입력 |
| `request.lidarSpec.scanRangeM` | 확장 가능 | 라이다 전체 탐지 거리 |
| `request.lidarSpec.angleStepDegree` | 확장 가능 | 2D 라이다 ray 간격 |
| `request.lidarSpec.sensorHeightM` | 확장 가능 | 라이다 센서 높이 |

주의:

- `scenario_start`는 Unreal에서 실제로 설정/측정할 수 있는 로봇, 구동, 라이다 스펙만 보낸다.
- `obstacleWarningDistanceM`, `stopDistanceM`, `slowDownDistanceM`, `frontHalfAngleDegree` 같은 정책 판정값은 더 이상 start JSON에 싣지 않는다. Python 예시 정책의 기본값 또는 사용자가 작성한 Python 정책 코드에서 관리한다.
- 과거 호환을 위해 Python parser는 legacy `vehicleSpec`, `controlSpec`를 읽을 수 있지만, 새 Unreal payload는 `robotSpec`, `driveSpec`, `lidarSpec`만 보낸다.
- `draw_obstacle_warning_debug`는 DeliveryBotSetup JSON 설정값이며 Python 통신 payload에는 포함되지 않는다.
- `allowDiagonalPathfinding=true`이면 A*가 대각선 cell 이동을 허용한다. 단, 대각선으로 장애물 모서리를 뚫고 지나가지 않도록 양옆 cell이 모두 walkable일 때만 대각선 이동한다.
- `smoothPathWithLineOfSight=true`이면 A* 결과가 line-of-sight shortcut으로 줄어든다. 이후 `pathWorldPoints`에는 follower corner smoothing과 `useExactGoalAsFinalPoint`가 반영된 실제 추종 경로가 들어간다.

### 사용자가 수정하기 좋은 값

실제 사용자가 자신의 로봇 스펙을 입력해 시뮬레이션할 때 열어두기 좋은 값이다.

| 값 | 위치 | 의미 |
| --- | --- | --- |
| `robotSpec.maxSpeedKmh` | `robotSpec` | 로봇의 실제 최고 전진 속도 |
| `robotSpec.bodyLengthCm` | `robotSpec` | 로봇 본체 길이 |
| `robotSpec.bodyWidthCm` | `robotSpec` | 로봇 본체 폭 |
| `robotSpec.bodyHeightCm` | `robotSpec` | 로봇 본체 높이 |
| `robotSpec.wheelBaseCm` | `robotSpec` | 로봇 축거 또는 주행 기준 길이 |
| `robotSpec.turningRadiusCm` | `robotSpec` | 로봇 최소 회전 반경 |
| `driveSpec.accelerationRateKmhPerSecond` | `driveSpec` | 로봇 목표 속도 증가율 |
| `driveSpec.decelerationRateKmhPerSecond` | `driveSpec` | 로봇 목표 속도 감소율 |
| `driveSpec.steeringInputRatePerSecond` | `driveSpec` | 조향 입력 변화율 |
| `driveSpec.throttleInputRatePerSecond` | `driveSpec` | 스로틀 입력 변화율 |
| `driveSpec.brakeInputRatePerSecond` | `driveSpec` | 브레이크 입력 변화율 |
| `driveSpec.stopBrakeInput` | `driveSpec` | 정지 시 기본 브레이크 입력 |
| `lidarSpec.scanRangeM` | `lidarSpec` | 실제 라이다 탐지 거리 |
| `lidarSpec.angleStepDegree` | `lidarSpec` | 실제 라이다 ray 간격/해상도 |
| `lidarSpec.sensorHeightM` | `lidarSpec` | 실제 라이다 장착 높이 |

아래 값은 정책 안정성, 평가 기준, 안전 판정에 가까우므로 `scenario_start`로 받지 않고 Python 정책 코드의 관리자/개발자 튜닝값으로 두는 것을 권장한다.

| 값 | 이유 |
| --- | --- |
| `targetSpeedKmh` | 경로 추종 정책의 목표 속도다. 기본 예시 정책은 Python에서 `4.5`로 시작하고 `robotSpec.maxSpeedKmh`를 넘지 않게 제한한다. |
| `lookAheadDistanceM`, `minLookAheadDistanceM`, `maxLookAheadDistanceM` | 경로 추종 lookahead 튜닝값이다. |
| `lookAheadSpeedGainMPerKmh`, `lookAheadSteeringReductionRatio`, `lookAheadSmoothingRatio` | 속도와 회전량에 따른 자동 lookahead 보정값이다. |
| `pathPointAcceptanceDistanceM`, `pathSmoothingDistanceM` | path index 진행과 corner smoothing 기준이다. |
| `goalSlowDownDistanceM`, `goalApproachSpeedKmh`, `goalApproachLookAheadDistanceM` | 목표 지점 접근 시 감속/추종 기준이다. |
| `steeringSensitivity`, `steeringFullScaleDegree`, `maxSteering`, `maxSteeringDelta`, `minTurnSpeedKmh` | 조향 출력과 회전 시 속도 제한 튜닝값이다. |
| `frontHalfAngleDegree`, `stopDistanceM`, `obstacleWarningDistanceM`, `slowDownDistanceM` | 라이다 hit를 전방 장애물, obstacle warning, 감속 구간으로 해석하는 정책 판정값이다. |
| `collisionStopHalfAngleDegree`, `collisionStopDistanceM` | 충돌 위험 중앙 코리더와 즉시 정지 기준이다. |
| `obstacleWarningDistanceM`, `repathDebounceSeconds`, `blockRadiusCells` | RePathPolicy가 위험 장애물로 보고 재탐색을 시작하는 거리, 같은 actor/cell 재탐색 억제 시간, 동적 장애물 확장 cell 반경이다. `blockRadiusCells` 기본값은 1이다. |
| `pathCorridorHalfWidthM` | PathFollower가 현재 경로 위 장애물만 감속/정지 판단에 사용하기 위한 corridor 판정 폭이다. |
| `obstacleSlowSpeedKmh` | 장애물 감속 구간의 정책 속도 기준이다. |
| `obstacleSoftCostRadiusM`, `obstacleSoftCostMaxPenalty`, `obstacleSoftCostPower` | 장애물 주변을 미리 피하도록 만드는 A* soft cost 튜닝값이다. |
| `pathTurnCostPenalty` | A* 회전 비용이다. 값이 클수록 직각/지그재그 경로를 덜 선택한다. |
| `allowDiagonalPathfinding`, `smoothPathWithLineOfSight`, `useExactGoalAsFinalPoint` | 대각선 이동, line-of-sight shortcut, 실제 goal 좌표 사용 여부다. |
| `emergencyBrakeInput` | 예시 정책의 비상 정지 기준이다. `softStopBrakeInput`은 새 구조에서 `driveSpec.stopBrakeInput`을 기본으로 참고한다. |

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
이 endpoint만 요청과 응답이 같은 JSON 형식을 쓴다.
요청 때는 `request`에 주행 판단에 필요한 정보를 채우고, 응답 때는 같은 JSON에 `response.action`과 `response.debug`를 추가한다.

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
| `request.lidar.mode` | 확장 가능 | typed LiDAR 모드. 예: `OneD`, `TwoD`, `ThreeD`, `OneDAndTwoD`, `TwoDAndThreeD`, `All` |
| `request.lidar.rays1d[]` | 확장 가능 | 1D 전방 ray 목록. 정책에서는 yaw 0도 전방 장애물 판단으로만 사용 |
| `request.lidar.rays2d[]` | 확장 가능 | 2D 수평 ray 목록. 2D 사용 모드에서는 정책 판단과 재경로의 기준 입력 |
| `request.lidar.rays3d[]` | 확장 가능 | 3D ray 목록. 3D 단독 모드에서는 같은 yaw의 vertical ray 중 가장 가까운 hit를 2D 정책 ray로 투영해 사용 |
| `request.lidar.rays3d[].hitLocationCm` | 확장 가능 | Unreal raycast가 실제로 맞춘 world hit 위치. Point Cloud export는 이 값이 있으면 거리/각도 재계산보다 우선 사용한다. |
| `request.observedObjects[]` | 확장 가능 | ray를 actor 단위로 묶은 관측 요약 |

### LiDAR 정책 입력 선택 규칙

Python 예시 정책은 사용자가 선택한 LiDAR 모드를 기준으로 정책 입력을 엄격하게 고른다.
정책 판단에 쓰는 ray는 `Tools/PythonAgent/agent/lidar_selector.py`에서만 선택한다.

| 선택 모드 | Python 정책 입력 |
| --- | --- |
| `OneD` | `lidar.rays1d`만 사용한다. yaw는 0도 전방으로 변환해서 앞 장애물 거리 판단과 재경로 시작 판단에 사용한다. |
| `TwoD`, `OneDAndTwoD`, `TwoDAndThreeD` | `lidar.rays2d`만 사용한다. 2D가 있으면 3D나 legacy ray로 fallback하지 않는다. |
| `ThreeD` | `lidar.rays3d`를 같은 yaw별로 묶고, vertical 전체 중 가장 가까운 hit를 2D 정책 ray로 투영한다. 같은 yaw에 hit가 없으면 수평 row에 가장 가까운 miss를 대표 ray로 둔다. |
| `All` | `rays2d`가 있으면 2D를 우선 사용하고, 없으면 3D yaw별 최단 hit projection, 그 다음 1D 순서로 선택한다. |
| legacy | typed `lidar`가 비어 있고 `lidarRays`만 있으면 기존 2D legacy ray로 처리한다. |

RePathPolicy는 `obstacleWarningDistanceM`을 재탐색 시작 거리로 사용한다.
공식 평가나 결과 분석에서 쓰는 `NearMiss`와 다르며, 현재 RePathPolicy는 obstacle warning 거리 안의 전방 장애물이 있으면 재경로를 요청한다.
1D는 좌우 위치를 알 수 없으므로 obstacle warning 거리 안의 전방 hit를 path 차단 후보로 본다.
RePathPolicy는 재탐색 트리거를 단순하게 유지하기 위해 선택된 LiDAR 차원의 raw hit ray를 직접 검사한다. 3D 모드에서는 같은 yaw projection 결과가 아니라 `lidar.rays3d` 전체에서 전방 각도 안의 hit actor를 찾는다.
2D와 3D projection에서 PathFollower는 현재 추종 path corridor 밖의 hit를 감속/정지 판단에서 제외해 새 경로를 계속 따라갈 수 있게 한다.
RePathPolicy는 obstacle warning 거리 안의 front ray를 봤지만 cooldown 때문에 즉시 재탐색할 수 없으면 `front_obstacle_repath_cooldown_stop`으로 정지해 PathFollower가 같은 tick에서 다시 전진하지 못하게 한다.
PathFollower가 soft stop으로 강제 재탐색을 요청한 경우에는 같은 actor debounce 중이어도 동적 장애물 cell을 다시 갱신한다.

### 정상 응답 예시

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
      "obstacleWarningCount": 0,
      "lastObstacleWarningCell": null,
      "lastObstacleWarningSource": "",
      "blockedCorridorCellCount": 0,
      "recoveryUntilSeconds": 0.0
    }
  }
}
```

### Near Obstacle Warning 통과 응답 예시

Obstacle warning은 모든 hit ray를 검사해서 actor/source별 1회만 기록하는 정책 디버그 값이다.
공식 episode near-miss 평가는 Unreal의 `UScenarioEvaluationSubsystem`이 ground truth 거리로 기록한다.
기본 공식은 아래와 같다.

```text
hit == true
AND distanceM <= obstacleWarningDistanceM
AND NOT (distanceM <= stopDistanceM AND isCollisionStopRay)
AND same actor/source not recorded yet
```

`isCollisionStopRay`는 기존 중앙 충돌 판정이다.

```text
abs(rayYawDegree) <= collisionStopHalfAngleDegree
OR distanceM <= collisionStopDistanceM
```

즉 `StopDistance < ObstacleWarningDistance < SlowDownDistance` 순서에서 실제 stop으로 보는 ray는 먼저 제외하고, 그 다음 warning을 actor/source별로 기록한다.

```json
{
  "schema": "delivery_bot_python_message",
  "version": 1,
  "type": "scenario_decide",
  "request": {
    "sequence": 76,
    "runTimeSeconds": 12.6,
    "robotState": {
      "x": -108.0,
      "y": 16.0,
      "z": 11.5,
      "yawDegree": 2.5,
      "speedKmh": 3.3
    },
    "lidarRays": [
      {
        "hit": true,
        "distanceM": 1.62,
        "rayIndex": 61,
        "rayYawDegree": 12.0,
        "actorName": "barrier_01",
        "actorTags": ["StaticObstacle"]
      }
    ],
    "observedObjects": [
      {
        "actorName": "barrier_01",
        "actorTags": ["StaticObstacle"],
        "closestDistanceM": 1.62,
        "closestRayYawDegree": 12.0,
        "totalHitRayCount": 4,
        "frontHitRayCount": 2,
        "inFront": true
      }
    ]
  },
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
      "reason": "front_obstacle_warning_pass",
      "pathStatus": "valid",
      "pathIndex": 4,
      "pathLength": 22,
      "obstacleWarningCount": 1,
      "lastObstacleWarningCell": null,
      "lastObstacleWarningSource": "barrier_01",
      "blockedCorridorCellCount": 0,
      "recoveryUntilSeconds": 0.0
    }
  }
}
```

### 감속/정지 응답 예시

아래 짧은 예시는 `response` 영역만 발췌한 것이다.
실제 HTTP 응답은 원 요청 envelope를 유지한 채 이 `response`가 추가된 형태다.

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

### 응답 필드

| 필드 | 구분 | 설명 |
| --- | --- | --- |
| `response.sequence` | 고정 | 처리한 decide sequence |
| `response.status` | 고정 | 정상 처리 시 `ok` |
| `response.action.steering` | 고정 | 조향 입력. Unreal에서 `-1.0`부터 `1.0`으로 clamp |
| `response.action.targetSpeedKmh` | 고정 | 목표 속도. 단위는 km/h |
| `response.action.brake` | 고정 | brake 입력. Unreal에서 `0.0`부터 `1.0`으로 clamp |
| `response.action.direction` | 고정 | 현재 Python 예시 정책은 `Forward`만 반환한다. |
| `response.debug.selectedPolicy` | 확장 가능 | action을 반환한 Python policy 이름 |
| `response.debug.reason` | 확장 가능 | policy가 선택한 이유 |
| `response.debug.pathStatus` | 확장 가능 | `valid` 또는 `empty` |
| `response.debug.pathIndex` | 확장 가능 | 현재 따라가는 path index |
| `response.debug.pathLength` | 확장 가능 | 현재 path cell 개수 |
| `response.debug.pathWorldPoints` | 확장 가능 | Unreal debug line에 사용하는 path world 좌표. 대각선 A*, line-of-sight shortcut, follower corner smoothing, 실제 goal 최종점이 반영된 추종 경로다. |
| `response.debug.targetPathIndex` | 확장 가능 | 실제 추종 target index |
| `response.debug.targetWorldPoint` | 확장 가능 | 실제 추종 target world 좌표 |
| `response.debug.closestPathDistanceCm` | 확장 가능 | 로봇과 경로 선분 사이 최소 거리 |
| `response.debug.maxPathErrorCm` | 확장 가능 | 허용 가능한 경로 이탈 거리 |
| `response.debug.lookAheadDistanceM` | 확장 가능 | 현재 decide에서 적용한 자동 lookahead 거리 |
| `response.debug.obstacleWarningCount` | 확장 가능 | 현재 Python policy가 actor/source별로 기록한 obstacle warning 횟수. 공식 평가 metric이 아니다. |
| `response.debug.lastObstacleWarningCell` | 확장 가능 | grid cell 기반 warning일 때 마지막 cell. 라이다 기반이면 `null` |
| `response.debug.lastObstacleWarningSource` | 확장 가능 | 마지막 warning 원인 actor 또는 source 이름 |
| `response.debug.blockedCorridorCellCount` | 확장 가능 | 최근 blocked corridor cell 개수 |
| `response.debug.recoveryUntilSeconds` | 확장 가능 | 현재 예시 정책에서는 호환용으로 유지하며 기본값은 `0.0`이다. |
| `response.debug.selectedLidarPolicyMode` | 확장 가능 | Python 정책이 선택한 LiDAR 입력 차원. 예: `1d`, `2d`, `3d`, `legacy2d` |
| `response.debug.selectedLidarRaySource` | 확장 가능 | 실제 정책 입력으로 선택한 ray 출처 |
| `response.debug.selectedLidarHorizontalPitchDegree` | 확장 가능 | 3D projection에서 miss 대표 ray를 고를 때 기준이 되는 수평 row pitch 절대값. 2D/1D에서는 `null` |

현재 자주 쓰는 `reason` 값:

```text
follow_path
front_obstacle_warning_pass
front_obstacle_slowdown
front_obstacle_soft_stop
path_deviation_repath_required
dynamic_repath_ready
collision_stop
collision_repath_cooldown
front_obstacle_repath_cooldown_stop
collision_repath_ready
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
    "robotInstanceId": "BP_DeliveryBot_C_0",
    "sequence": 120,
    "status": "arrived",
    "metrics": {},
    "debug": {
      "endSource": "UScenarioEvaluationSubsystem"
    }
  }
}
```

### 요청 필드

| 필드 | 구분 | 설명 |
| --- | --- | --- |
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
      "obstacleWarningCount": 1,
      "stopCount": 0,
      "repathCount": 1,
      "slowdownCount": 3
    },
    "debug": {
      "reason": "episode_end_recorded",
      "status": "arrived",
      "stopCount": 0,
      "repathCount": 1,
      "slowdownCount": 3,
      "obstacleWarningCount": 1,
      "lastObstacleWarningCell": null,
      "lastObstacleWarningSource": "barrier_01",
      "blockedCorridorCellCount": 0
    }
  }
}
```

## Error 형식

Python 처리 중 예외가 발생하면 HTTP status code가 `500`이 될 수 있고, envelope의 `response.status`는 `error`가 된다.
정책 실패처럼 서버 예외가 아닌 실패는 HTTP `200` 안에서 `response.status: "error"`로 올 수 있다.
아래 예시는 `/scenario/decide` 에러 응답이므로 원 요청 `request`를 함께 보여준다.

```json
{
  "schema": "delivery_bot_python_message",
  "version": 1,
  "type": "scenario_decide",
  "request": {
    "sequence": 80,
    "runTimeSeconds": 13.0,
    "robotState": {
      "x": -90.0,
      "y": 18.0,
      "z": 11.5,
      "yawDegree": 3.0,
      "speedKmh": 2.8
    },
    "lidarRays": [],
    "observedObjects": []
  },
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
```

Python 응답:

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
request.robotInstanceId
request.start.x
request.start.y
request.start.z
request.start.yawDegree
request.goal.hasGoal
request.goal.x
request.goal.y
request.goal.z
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
request.robotSpec.maxSpeedKmh
request.robotSpec.bodyLengthCm
request.robotSpec.bodyWidthCm
request.robotSpec.bodyHeightCm
request.robotSpec.wheelBaseCm
request.robotSpec.turningRadiusCm
request.driveSpec.accelerationRateKmhPerSecond
request.driveSpec.decelerationRateKmhPerSecond
request.driveSpec.steeringInputRatePerSecond
request.driveSpec.throttleInputRatePerSecond
request.driveSpec.brakeInputRatePerSecond
request.driveSpec.stopBrakeInput
request.lidarSpec.scanRangeM
request.lidarSpec.angleStepDegree
request.lidarSpec.sensorHeightM
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
request.robotInstanceId
request.sequence
request.status
```

## 확장 가능한 위치

기존 필드를 제거하거나 타입을 바꾸지 않는 조건에서 아래 영역은 확장하기 좋다.

| 목적 | 권장 위치 |
| --- | --- |
| 목표 속도, 조향 민감도, 회복 속도 같은 정책 설정 | Python 정책 코드의 설정값 |
| 전방 각도, obstacle warning/충돌 경계 같은 라이다 해석 기준 | Python 정책 코드의 설정값 |
| 로봇 물리/속도 제약 | `request.robotSpec`, `request.driveSpec` |
| 라이다 장비 스펙 | `request.lidarSpec` |
| actor 단위 관측 요약 | `request.observedObjects` |
| 경로 시각화, 정책 선택 이유, obstacle warning 카운트 | `response.debug` |
| episode 종료 metric | `request.metrics`, `/scenario/end`의 `response.metrics` |

## Version을 올려야 하는 경우

다음 변경은 기존 Unreal/Python 중 한쪽을 깨뜨릴 수 있으므로 `version`을 올리는 것이 안전하다.

- `response.action` 필드 이름 변경
- `response.action.direction` 값 체계 변경
- `request.robotState`, `request.grid.cells`, `request.lidarRays`의 필수 필드 제거 또는 타입 변경
- `response.status` 값 체계 변경
- 기존 고정 필드의 의미 변경

반대로 `request.robotSpec`, `request.driveSpec`, `request.lidarSpec`, `response.debug`에 새 optional 필드를 추가하는 것은 보통 `version`을 올리지 않아도 된다.

## 관련 DeliveryBotSetup JSON 메모

상태: legacy local setup note.

- 최종 profile 기준: `contracts/specs/user-project-data.md`의 `profile.json`
- 새 writer는 `DeliveryBotSetup` JSON을 만들지 않음

아래 값은 Python 통신 payload가 아니라 `Json/Input/DeliveryBotSetup*.json`에서 읽는 로컬 설정이다.
obstacle warning 범위를 화면에서 보고 싶을 때 사용한다.

```json
{
  "robot": {
    "lidar": {
      "draw_debug": true,
      "draw_obstacle_warning_debug": true,
      "front_half_angle_degree": 25.0,
      "obstacle_warning_distance_m": 2.5,
      "collision_stop_half_angle_degree": 8.0,
      "collision_stop_distance_m": 0.45,
      "slow_down_distance_m": 4.8
    }
  }
}
```

색상 기준:

```text
Cyan   = Obstacle warning 후보 범위
Orange = 중앙 충돌/감속 코리더
Red    = 매우 가까운 충돌 반경
```
