# DeliveryBot User Config Fields

이 문서는 Episode JSON에서 사용자가 입력할 수 있는 DeliveryBot 설정값을 정리한다.

현재 방향은 다음을 기준으로 한다.

- Python 서버가 정책 판단과 길찾기를 담당한다.
- Unreal은 주변 정보, 로봇 상태, Grid, Episode 설정을 Python에 보낸다.
- Unreal은 Python이 반환한 action을 검증한 뒤 실행한다.
- JSON에 없는 값은 Unreal 구조체 기본값을 사용한다.
- `NavigationConfigInfo`, `HybridAStarConfigInfo`는 Python policy 모드에서는 사용자 JSON 입력에서 제외한다.

## 지금 당장 할 구조체 정리

| 구분 | 대상 | 지금 할 일 | 이유 |
|---|---|---|---|
| 수정 | `FDeliveryBotPathFollowConfigInfo` | `FDeliveryBotMotionControlConfigInfo`로 이름 변경 | Python policy와 Unreal 실행부가 함께 참고하는 이동 제어 설정으로 의미를 넓힌다 |
| 수정 | `FDeliveryBotSetupInfo::PathFollowConfigInfo` | `MotionControlConfigInfo`로 멤버명 변경 | 구조체 이름 변경과 의미를 맞춘다 |
| 유지 | `FDeliveryBotLocationSetupInfo` | 그대로 유지 | 기존 Start/Goal 구조를 최대한 유지한다 |
| 유지 | `FDeliveryBotDriveConfigInfo` | 그대로 유지 | 차량 물리와 action 검증 기준으로 계속 사용한다 |
| 유지 | `FDeliveryBotLidarSensorConfigInfo` | 그대로 유지 | Python observation의 센서 의미를 정의한다 |
| 유지 | `FDeliveryBotNavigationConfigInfo` | C++에는 유지하되 Python policy JSON에서는 제외 | 기존 Unreal 내부 길찾기 legacy 용도로만 남긴다 |
| 유지 | `FDeliveryBotHybridAStarConfigInfo` | C++에는 유지하되 Python policy JSON에서는 제외 | Python 길찾기 설정과 혼동되지 않게 한다 |
| 추가 | 새 구조체 변수 | 지금은 추가하지 않음 | 현재 구조를 최대한 유지하고, 필요한 값은 기존 구조체에서 고른다 |

## 추후 필요할 때 바꿀 것

| 대상 | 추후 변경 후보 | 지금 미루는 이유 |
|---|---|---|
| `PathPointAcceptanceDistanceM` | `WaypointAcceptanceDistanceM` | 의미는 더 좋지만 현재 코드 수정 범위가 커진다 |
| `TargetSpeedKmh` | `DefaultTargetSpeedKmh` | Python이 매번 `targetSpeedKmh`를 반환하는 구조가 안정된 뒤 바꾼다 |
| `FDeliveryBotNavigationConfigInfo` | legacy 전용 구조체로 분리 | 지금 제거하면 기존 ChaosActor, GlobalPathComponent와 충돌할 수 있다 |
| `FDeliveryBotHybridAStarConfigInfo` | Python policy config로 이동 | Python 길찾기 구현 단계에서 별도 관리한다 |
| `TraceChannel` | 문자열 기반 JSON 매핑 지원 | collision channel 변환 규칙을 따로 정해야 하므로 나중에 연다 |
| Policy loop 설정 | 별도 `PolicyRuntimeSpec` 추가 | episode start/config update 흐름이 안정된 뒤 추가한다 |

## JSON으로 받을 값

### `locationSpec`

| JSON 필드 | Unreal 변수 | 타입 | 기본값 | 역할 |
|---|---|---|---:|---|
| `startLocationCm` | `StartLocationCm` | vector cm | `(0,0,0)` | Episode 시작 시 로봇을 배치할 위치 |
| `goalLocationCm` | `GoalLocationCm` | vector cm | `(0,0,0)` | 로봇이 도달해야 하는 목표 위치 |
| `autoStartRoute` | `bAutoStartRoute` | bool | `true` | Episode 시작 후 자동으로 policy loop를 시작할지 결정 |

### `driveSpec`

| JSON 필드 | Unreal 변수 | 타입 | 기본값 | 역할 |
|---|---|---|---:|---|
| `maxSpeedKmh` | `MaxSpeedKmh` | float | `10` | Python이 반환한 전진 목표 속도의 최대 허용값 |
| `maxReverseSpeedKmh` | `MaxReverseSpeedKmh` | float | `3` | Python이 반환한 후진 목표 속도의 최대 허용값 |
| `slowdownSpeedRangeKmh` | `SlowdownSpeedRangeKmh` | float | `4` | 목표 속도 근처에서 감속을 시작하는 속도 범위 |
| `stopBrakeInput` | `StopBrakeInput` | float | `0.15` | 정지 명령 또는 목표 속도 0일 때 적용할 기본 브레이크 입력 |
| `throttleInputRatePerSecond` | `ThrottleInputRatePerSecond` | float | `0.35` | throttle 입력 변화율 제한 |
| `brakeInputRatePerSecond` | `BrakeInputRatePerSecond` | float | `0.5` | brake 입력 변화율 제한 |
| `steeringInputRatePerSecond` | `SteeringInputRatePerSecond` | float | `3` | steering 입력 변화율 제한 |
| `accelerationRateKmhPerSecond` | `AccelerationRateKmhPerSecond` | float | `2` | target speed 모드에서 속도를 올리는 가속률 |
| `decelerationRateKmhPerSecond` | `DecelerationRateKmhPerSecond` | float | `1.5` | target speed 모드에서 속도를 낮추는 감속률 |
| `maxTorque` | `MaxTorque` | float | `220` | Chaos 차량 엔진 최대 토크 |
| `maxRPM` | `MaxRPM` | float | `2000` | Chaos 차량 엔진 최대 RPM |

### `lidarSpec`

| JSON 필드 | Unreal 변수 | 타입 | 기본값 | 역할 |
|---|---|---|---:|---|
| `scanRangeM` | `ScanRangeM` | float | `5` | 라이다 최대 탐지 거리 |
| `angleStepDegree` | `AngleStepDegree` | float | `2` | 라이다 ray 사이의 각도 간격 |
| `sensorHeightM` | `SensorHeightM` | float | `0.07` | 로봇 기준 라이다 센서 높이 |
| `frontHalfAngleDegree` | `FrontHalfAngleDegree` | float | `20` | 전방 객체로 판단할 좌우 반각 |
| `storeMissedRays` | `bStoreMissedRays` | bool | `false` | hit가 없는 ray도 observation에 저장할지 결정 |
| `stopDistanceM` | `StopDistanceM` | float | `1.5` | 전방 장애물을 기준으로 정지를 고려할 거리 |
| `slowDownDistanceM` | `SlowDownDistanceM` | float | `5` | 전방 장애물을 기준으로 감속을 고려할 거리 |
| `lidarModeType` | `LidarModeType` | enum string | `TwoD` | `OneD`, `TwoD`, `ThreeD`, `OneDAndTwoD`, `TwoDAndThreeD`, `All` 중 하나 |
| `ignoreTags` | `IgnoreTags` | string array | `["NoCollision"]` | 라이다에서 무시할 actor tag 목록 |

### `motionControlSpec`

기존 `FDeliveryBotPathFollowConfigInfo`를 `FDeliveryBotMotionControlConfigInfo`로 변경해서 사용한다.

| JSON 필드 | Unreal 변수 | 타입 | 기본값 | 역할 |
|---|---|---|---:|---|
| `drawDebug` | `bDrawDebug` | bool | `true` | 이동 제어, 경로, 목표점 관련 디버그 표시 여부 |
| `lookAheadDistanceM` | `LookAheadDistanceM` | float | `1` | 경로 또는 waypoint를 따라갈 때 앞쪽 목표점을 잡는 거리 |
| `pathPointAcceptanceDistanceM` | `PathPointAcceptanceDistanceM` | float | `0.4` | 중간 path point 또는 waypoint에 도달했다고 판단하는 거리 |
| `goalAcceptanceDistanceM` | `GoalAcceptanceDistanceM` | float | `0.8` | 최종 목표에 도착했다고 판단하는 거리 |
| `steeringSensitivity` | `SteeringSensitivity` | float | `0.8` | 목표 방향으로 회전할 때 조향값을 얼마나 민감하게 줄지 결정 |
| `minTurnSpeedKmh` | `MinTurnSpeedKmh` | float | `1` | 회전 중 너무 낮은 속도로 멈추지 않도록 유지할 최소 속도 |
| `obstacleSlowSpeedKmh` | `ObstacleSlowSpeedKmh` | float | `0.5` | 장애물 근처에서 사용할 기본 감속 속도 |

## JSON에서 제외하고 기본값으로 쓸 값

### Drive 기본값

| Unreal 변수 | 기본값 | 제외 이유 |
|---|---:|---|
| `ReverseAccelerationRateKmhPerSecond` | `1` | 후진 전용 가속률은 초기 사용자 설정 복잡도를 높인다 |
| `GearSwitchStopSpeedKmh` | `0.3` | 전/후진 기어 전환 안정성 내부값에 가깝다 |
| `GearSwitchBrakeInput` | `0.2` | 기어 전환 시 보조 브레이크 내부값에 가깝다 |
| `SpeedLimitToleranceKmh` | `0.5` | 속도 제한 검증 여유값으로 기본값 유지가 안전하다 |
| `SpeedLimitBrake` | `0.08` | 제한 속도 초과 보정용 내부값에 가깝다 |
| `bUseHandbrakeWhenBrake` | `false` | 특수 주행 설정이라 초기 JSON 후보에서 제외한다 |
| `EngineIdleRPM` | `600` | 엔진 세부 튜닝값이다 |
| `EngineBrakeEffect` | `0.04` | 엔진 세부 튜닝값이다 |
| `EngineRevUpMOI` | `5` | 엔진 세부 튜닝값이다 |
| `EngineRevDownRate` | `600` | 엔진 세부 튜닝값이다 |

### Lidar 기본값

| Unreal 변수 | 기본값 | 제외 이유 |
|---|---:|---|
| `bDrawDebug` | `true` | 개발 중에는 기본 표시를 유지한다 |
| `TraceChannel` | `Visibility` | Unreal collision channel 매핑 규칙을 별도로 정해야 한다 |

### Motion Control 기본값

| Unreal 변수 | 기본값 | 제외 이유 |
|---|---:|---|
| `TargetSpeedKmh` | `3` | Python이 매번 `targetSpeedKmh`를 반환하므로 초기 JSON에서 제외한다 |

### Navigation 기본값

| Unreal 변수 | 기본값 | 제외 이유 |
|---|---:|---|
| `PathFinderType` | `GridAStar` | Python이 길찾기 방식을 결정하므로 사용자 JSON에서 제외한다 |
| `DriveControllerType` | `PathFollow` | Python이 이동 판단을 결정하므로 사용자 JSON에서 제외한다 |
| `HybridAStarConfigInfo` | 기본 구조체 | Unreal 내부 길찾기 legacy 설정으로만 유지한다 |

### Hybrid A* 기본값

아래 값들은 Python policy 모드에서는 JSON으로 받지 않는다. Python에서 Hybrid A*를 구현할 경우 Python policy config에서 별도로 관리한다.

| Unreal 변수 | 기본값 |
|---|---:|
| `MotionModelType` | `ForwardReverse` |
| `StepDistanceCm` | `75` |
| `MinTurningRadiusCm` | `300` |
| `HeadingBinCount` | `72` |
| `MaxSearchCount` | `15000` |
| `GoalAcceptanceDistanceCm` | `150` |
| `GoalAcceptanceAngleDegree` | `25` |
| `ReverseCostMultiplier` | `2.2` |
| `GearSwitchCostPenalty` | `500` |
| `MaxContinuousReverseDistanceCm` | `250` |
| `ReverseStepDistanceScale` | `0.6` |
| `TurnCostPenalty` | `15` |
| `ReverseTurnCostPenalty` | `25` |
| `TurnSwitchCostPenalty` | `40` |

## Episode Start JSON 형식

사용자는 필요한 값만 넣을 수 있다. 누락된 값은 Unreal 구조체 기본값을 사용한다.

```json
{
  "episodeId": "episode_001",
  "robotInstanceId": "delivery_bot_01",
  "locationSpec": {
    "startLocationCm": {
      "x": 0.0,
      "y": 0.0,
      "z": 0.0
    },
    "goalLocationCm": {
      "x": 2500.0,
      "y": -800.0,
      "z": 0.0
    },
    "autoStartRoute": true
  },
  "driveSpec": {
    "maxSpeedKmh": 10.0,
    "maxReverseSpeedKmh": 3.0,
    "slowdownSpeedRangeKmh": 4.0,
    "stopBrakeInput": 0.15,
    "throttleInputRatePerSecond": 0.35,
    "brakeInputRatePerSecond": 0.5,
    "steeringInputRatePerSecond": 3.0,
    "accelerationRateKmhPerSecond": 2.0,
    "decelerationRateKmhPerSecond": 1.5,
    "maxTorque": 220.0,
    "maxRPM": 2000.0
  },
  "lidarSpec": {
    "scanRangeM": 5.0,
    "angleStepDegree": 2.0,
    "sensorHeightM": 0.07,
    "frontHalfAngleDegree": 20.0,
    "storeMissedRays": false,
    "stopDistanceM": 1.5,
    "slowDownDistanceM": 5.0,
    "lidarModeType": "TwoD",
    "ignoreTags": [
      "NoCollision"
    ]
  },
  "motionControlSpec": {
    "drawDebug": true,
    "lookAheadDistanceM": 1.0,
    "pathPointAcceptanceDistanceM": 0.4,
    "goalAcceptanceDistanceM": 0.8,
    "steeringSensitivity": 0.8,
    "minTurnSpeedKmh": 1.0,
    "obstacleSlowSpeedKmh": 0.5
  }
}
```

## 최소 JSON 예시

아래처럼 일부 값만 넣어도 된다.

```json
{
  "episodeId": "episode_001",
  "robotInstanceId": "delivery_bot_01",
  "locationSpec": {
    "startLocationCm": {
      "x": 0.0,
      "y": 0.0,
      "z": 0.0
    },
    "goalLocationCm": {
      "x": 2500.0,
      "y": -800.0,
      "z": 0.0
    }
  },
  "driveSpec": {
    "maxSpeedKmh": 8.0,
    "maxReverseSpeedKmh": 2.0
  },
  "lidarSpec": {
    "scanRangeM": 6.0,
    "lidarModeType": "TwoD"
  }
}
```

## 구현 순서

1. `FDeliveryBotPathFollowConfigInfo`를 `FDeliveryBotMotionControlConfigInfo`로 이름 변경한다.
2. `FDeliveryBotSetupInfo::PathFollowConfigInfo`를 `MotionControlConfigInfo`로 이름 변경한다.
3. Episode JSON compiler에서 `locationSpec`, `driveSpec`, `lidarSpec`, `motionControlSpec`를 읽는다.
4. JSON에 없는 값은 각 구조체 기본값을 유지한다.
5. `/episode/start` 전송 JSON에는 위 네 spec만 포함한다.
6. `navigationSpec`, `hybridAStarSpec`은 Python policy 모드에서 보내지 않는다.
