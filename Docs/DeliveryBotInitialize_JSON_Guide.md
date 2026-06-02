# DeliveryBot Initialize JSON Guide

이 문서는 Episode JSON에서 DeliveryBot을 생성하고 `ADeliveryBot_ChaosActor::InitializeSetupInfo()`에 전달할 값을 정리한다.

현재 코드에서 실제로 JSON으로 연결된 값은 시작 위치, 목표 위치, 자동 경로 시작 여부이다.
아래의 사용자 조정 필드는 이후 JSON 파싱을 확장할 때 추가할 최종 후보이다.

## 출력 원칙

DeliveryBot 설정은 `actors.robot` 아래에 둔다.

위치는 기존 EpisodeSpec 규칙을 따른다. JSON 입력은 meter를 사용하고, Unreal 내부에서는 centimeter로 변환한다.
단, `start_location_cm`, `goal_location_cm`처럼 필드 이름에 `_cm`이 붙은 확장 구조를 쓴다면 값 자체를 centimeter로 취급한다.

사용자가 직접 조정하는 값은 너무 많이 열지 않는다. 이번 범위는 주행 속도, 경로 추종, 라이다 감지/반응 거리만 포함한다.
엔진 토크, RPM, 충돌 채널, IgnoreTag 같은 값은 차량 물리와 프로젝트 설정에 너무 가까우므로 일반 JSON 조정값에서 제외한다.

## 좌표계와 단위

| 항목 | JSON 필드 | 입력 단위 | 내부 단위 | 설명 |
| --- | --- | --- | --- | --- |
| 시작 위치 | `start_location_cm` | centimeter | centimeter | `FDeliveryBotLocationSetupInfo.StartLocationCm`에 들어간다. |
| 목표 위치 | `goal_location_cm` | centimeter | centimeter | `FDeliveryBotLocationSetupInfo.GoalLocationCm`에 들어간다. |
| 속도 | `max_speed_kmh`, `target_speed_kmh`, `obstacle_slow_speed_kmh` | km/h | km/h | 사용자가 이해하기 쉬운 주행 속도 단위. |
| 거리 | `look_ahead_distance_m`, `scan_range_m`, `stop_distance_m`, `slow_down_distance_m` | meter | meter 또는 centimeter 변환 | 컴포넌트 내부에서 필요하면 centimeter로 변환한다. |
| 각도 | `angle_step_degree` | degree | degree | 라이다 레이 간격. |
| 감속 범위 | `slowdown_speed_range_kmh` | km/h | km/h | 목표 속도 근처에서 스로틀/브레이크를 부드럽게 조정하는 범위. |

## 현재 JSON에서 실제로 받는 값

현재 `UEpisodeCompiler::CompileRobotSpawn()`과 `UEpisodeSimulationSubsystem::SpawnRobotActor()`가 실제로 연결하는 값은 아래 정도이다.

| JSON 경로 | 필수 | 타입/단위 | 기본값 | 설명 |
| --- | --- | --- | --- | --- |
| `actors.robot.instance_id` | 필수 | string | 없음 | 런타임 로봇 ID. 없으면 `actor_id`를 대체 필드로 읽는다. |
| `actors.robot.asset_id` | 필수 | string | 없음 | 로봇 에셋 ID. 없으면 `type`을 대체 필드로 읽는다. |
| `actors.robot.spawn_only` | 선택 | boolean | `true` | `true`면 스폰만 하고 자동 주행하지 않는다. |
| `actors.robot.transform.location_m` | 선택 | `[x, y, z]`, meter | `[0, 0, 0]` | 시작 위치로 사용된다. 내부에서는 cm로 변환된다. |
| `actors.robot.route.goal_m` | 선택 | `[x, y, z]`, meter | 없음 | 목적지로 사용된다. 내부에서는 cm로 변환된다. |
| `actors.robot.route.auto_start` | 선택 | boolean | `true` | `spawn_only=false`이고 목표 위치가 있을 때 자동 경로 추종을 시작할지 결정한다. |
| `actors.robot.goal_m` | 대체 가능 | `[x, y, z]`, meter | 없음 | `route.goal_m`이 없을 때 사용하는 목적지 대체 필드. |

## 사용자 조정 필드 최종안

선택한 12개 값은 적절하다. 시뮬레이션 사용자가 결과를 이해하고 조정하기 쉬운 값들이고, Chaos Vehicle의 민감한 물리 튜닝값은 건드리지 않는다.
즉, "시나리오 설정"으로 열어둘 값과 "차량 구현 세부 튜닝"으로 숨겨둘 값의 경계가 꽤 깔끔하다.

권장 JSON 구조는 아래처럼 `actors.robot` 아래에 `location`, `drive`, `path_follow`, `lidar`를 두는 형태이다.

```json
"robot": {
  "location": {},
  "drive": {},
  "path_follow": {},
  "lidar": {}
}
```

### location fields

```json
"location": {
  "start_location_cm": [0.0, 0.0, 50.0],
  "goal_location_cm": [800.0, 300.0, 50.0],
  "auto_start_route": true
}
```

| 필드 | 필수 | 타입/단위 | 기본값 | C++ 매핑 | 설명 |
| --- | --- | --- | --- | --- | --- |
| `start_location_cm` | 선택 | `[x, y, z]`, centimeter | `[0, 0, 0]` | `LocationSetupInfo.StartLocationCm` | 로봇 시작 위치. 현재 `transform.location_m`에서 가져오던 값을 명시적으로 받을 때 사용한다. |
| `goal_location_cm` | 선택 | `[x, y, z]`, centimeter | 시작 위치 | `LocationSetupInfo.GoalLocationCm` | A* 경로 생성의 목표 위치. 현재 `route.goal_m`에서 가져오던 값을 명시적으로 받을 때 사용한다. |
| `auto_start_route` | 선택 | boolean | `true` | `LocationSetupInfo.bAutoStartRoute` | BeginPlay 이후 자동으로 A* 경로를 만들고 추종할지 결정한다. |

### drive fields

```json
"drive": {
  "max_speed_kmh": 10.0,
  "slowdown_speed_range_kmh": 2.0
}
```

| 필드 | 필수 | 타입/단위 | 기본값 | C++ 매핑 | 설명 |
| --- | --- | --- | --- | --- | --- |
| `max_speed_kmh` | 선택 | number, km/h | `10.0` | `ChaosDriveConfigInfo.MaxSpeedKmh` | 차량 주행 속도의 최종 상한. `target_speed_kmh`가 더 높아도 이 값으로 제한된다. |
| `slowdown_speed_range_kmh` | 선택 | number, km/h | `2.0` | `ChaosDriveConfigInfo.SlowdownSpeedRangeKmh` | 목표 속도에 가까워질수록 입력을 줄이는 범위. 값이 크면 속도 변화가 더 부드럽다. |

### path_follow fields

```json
"path_follow": {
  "target_speed_kmh": 10.0,
  "look_ahead_distance_m": 1.0,
  "obstacle_slow_speed_kmh": 2.0
}
```

| 필드 | 필수 | 타입/단위 | 기본값 | C++ 매핑 | 설명 |
| --- | --- | --- | --- | --- | --- |
| `target_speed_kmh` | 선택 | number, km/h | `10.0` | `PathFollowConfigInfo.TargetSpeedKmh` | 경로 추종 목표 속도. 일반 주행 속도를 정한다. |
| `look_ahead_distance_m` | 선택 | number, meter | `1.0` | `PathFollowConfigInfo.LookAheadDistanceM` | 현재 위치보다 앞쪽의 추종 목표점을 얼마나 멀리 볼지 정한다. 작으면 급하게 꺾고, 크면 완만하게 움직인다. |
| `obstacle_slow_speed_kmh` | 선택 | number, km/h | `2.0` | `PathFollowConfigInfo.ObstacleSlowSpeedKmh` | 장애물 감속 구간에서 사용할 목표 속도. `slow_down_distance_m` 안에 장애물이 있을 때 의미가 있다. |

### lidar fields

```json
"lidar": {
  "scan_range_m": 5.0,
  "angle_step_degree": 5.0,
  "stop_distance_m": 1.5,
  "slow_down_distance_m": 3.5
}
```

| 필드 | 필수 | 타입/단위 | 기본값 | C++ 매핑 | 설명 |
| --- | --- | --- | --- | --- | --- |
| `scan_range_m` | 선택 | number, meter | `5.0` | `LidarSensorConfigInfo.ScanRangeM` | 라이다가 장애물을 탐지할 최대 거리. 값이 크면 더 멀리 보지만 트레이스 비용이 늘어난다. |
| `angle_step_degree` | 선택 | number, degree | `5.0` | `LidarSensorConfigInfo.AngleStepDegree` | 라이다 레이 간격. 작을수록 촘촘하게 감지하지만 레이 수가 늘어난다. |
| `stop_distance_m` | 선택 | number, meter | `1.5` | `LidarSensorConfigInfo.StopDistanceM` | 전방 장애물이 이 거리 안에 있으면 정지한다. |
| `slow_down_distance_m` | 선택 | number, meter | `3.5` | `LidarSensorConfigInfo.SlowDownDistanceM` | 전방 장애물이 이 거리 안에 있으면 감속한다. `stop_distance_m`보다 커야 한다. |

## 권장 검증 규칙

JSON에서 값이 빠지면 C++ 구조체 기본값을 그대로 사용한다.
값이 들어온 경우에는 파싱 단계에서 아래 규칙으로 한 번 정리하는 것을 권장한다.

| 필드 | 권장 제한 |
| --- | --- |
| `max_speed_kmh` | `0` 이상 |
| `slowdown_speed_range_kmh` | 최소 `0.1` |
| `target_speed_kmh` | `0` 이상 |
| `look_ahead_distance_m` | 최소 `0.1` |
| `obstacle_slow_speed_kmh` | `0` 이상 |
| `scan_range_m` | `0` 이상 |
| `angle_step_degree` | 최소 `1.0` |
| `stop_distance_m` | `0` 이상 |
| `slow_down_distance_m` | `stop_distance_m + 0.1` 이상 |

## 전체 JSON 예시

아래 예시는 최종 후보 12개만 포함한 형태이다.

```json
{
  "scenario_id": "deliverybot_tuning_001",
  "version": 1,
  "run": {
    "base_seed": 1234,
    "iteration_index": 0,
    "time_limit_s": 60.0
  },
  "actors": {
    "robot": {
      "instance_id": "delivery_bot_01",
      "asset_id": "delivery_bot_chaos",
      "spawn_only": false,
      "location": {
        "start_location_cm": [0.0, 0.0, 50.0],
        "goal_location_cm": [800.0, 300.0, 50.0],
        "auto_start_route": true
      },
      "drive": {
        "max_speed_kmh": 10.0,
        "slowdown_speed_range_kmh": 2.0
      },
      "path_follow": {
        "target_speed_kmh": 10.0,
        "look_ahead_distance_m": 1.0,
        "obstacle_slow_speed_kmh": 2.0
      },
      "lidar": {
        "scan_range_m": 5.0,
        "angle_step_degree": 5.0,
        "stop_distance_m": 1.5,
        "slow_down_distance_m": 3.5
      }
    }
  }
}
```

## 구현 메모

현재 코드의 `actors.robot.transform.location_m`, `actors.robot.route.goal_m`, `actors.robot.route.auto_start`와 새 `actors.robot.location`은 역할이 겹친다.
둘 다 지원할 경우에는 새 `location` 값을 우선하고, 없을 때 기존 `transform`과 `route` 값을 fallback으로 쓰는 방식이 좋다.

권장 우선순위는 다음과 같다.

| 내부 필드 | 1순위 JSON | fallback JSON |
| --- | --- | --- |
| `StartLocationCm` | `actors.robot.location.start_location_cm` | `actors.robot.transform.location_m * 100` |
| `GoalLocationCm` | `actors.robot.location.goal_location_cm` | `actors.robot.route.goal_m * 100`, `actors.robot.goal_m * 100` |
| `bAutoStartRoute` | `actors.robot.location.auto_start_route` | `!spawn_only && route.auto_start && 목표 위치 있음` |
