# DeliveryBotSetup JSON Guide

이 문서는 DeliveryBot의 주행/센서 튜닝값을 `FDeliveryBotSetupInfo`에 채우기 위한 JSON 범위를 정리한다.

DeliveryBotSetup JSON은 로봇 액터를 어디에, 어떤 ID로, 어떤 목표로 배치할지 결정하지 않는다. 그 책임은 EpisodeSetup JSON에 둔다.

## 책임 범위

EpisodeSetup JSON이 담당하는 값:

| 범위 | 예시 |
| --- | --- |
| Episode 실행 정보 | `run.base_seed`, `run.iteration_index`, `run.time_limit_s` |
| 로봇 액터 식별 | `actors.robot.instance_id`, `actors.robot.asset_id` |
| 로봇 배치 | `actors.robot.spawn_only`, `actors.robot.xy_m`, `actors.robot.yaw_deg` |
| 로봇 목적지 | `actors.robot.route.goal_xy_m`, `actors.robot.route.auto_start` |

DeliveryBotSetup JSON이 담당하는 값:

| 범위 | 예시 |
| --- | --- |
| 주행 속도 튜닝 | `robot.drive.max_speed_kmh`, `robot.drive.slowdown_speed_range_kmh` |
| 경로 추종 튜닝 | `robot.path_follow.target_speed_kmh`, `robot.path_follow.look_ahead_distance_m`, `robot.path_follow.obstacle_slow_speed_kmh` |
| 라이다 반응 튜닝 | `robot.lidar.scan_range_m`, `robot.lidar.angle_step_degree`, `robot.lidar.stop_distance_m`, `robot.lidar.slow_down_distance_m` |

엔진 토크, RPM, 충돌 채널, IgnoreTag처럼 JSON으로 열지 않은 값은 C++ 구조체 기본값으로 fallback된다.

## 권장 구조

```json
{
  "schema": "delivery_bot_setup",
  "version": 1,
  "robot": {
    "drive": {},
    "path_follow": {},
    "lidar": {}
  }
}
```

`run`, `actors`, `instance_id`, `asset_id`, `spawn_only`, `transform`, `location`, `route`는 넣지 않는다.

## drive fields

```json
"drive": {
  "max_speed_kmh": 10.0,
  "slowdown_speed_range_kmh": 2.0
}
```

| 필드 | 필수 | 타입/단위 | 기본값 | C++ 매핑 | 설명 |
| --- | --- | --- | --- | --- | --- |
| `max_speed_kmh` | 선택 | number, km/h | `10.0` | `ChaosDriveConfigInfo.MaxSpeedKmh` | 차량 주행 속도의 최종 상한. |
| `slowdown_speed_range_kmh` | 선택 | number, km/h | `4.0` | `ChaosDriveConfigInfo.SlowdownSpeedRangeKmh` | 목표 속도 근처에서 입력을 줄이는 범위. 값이 크면 속도 변화가 더 부드럽다. |

## path_follow fields

```json
"path_follow": {
  "target_speed_kmh": 10.0,
  "look_ahead_distance_m": 1.0,
  "obstacle_slow_speed_kmh": 2.0
}
```

| 필드 | 필수 | 타입/단위 | 기본값 | C++ 매핑 | 설명 |
| --- | --- | --- | --- | --- | --- |
| `target_speed_kmh` | 선택 | number, km/h | `10.0` | `PathFollowConfigInfo.TargetSpeedKmh` | 경로 추종 목표 속도. |
| `look_ahead_distance_m` | 선택 | number, meter | `1.0` | `PathFollowConfigInfo.LookAheadDistanceM` | 현재 위치보다 앞쪽의 추종 목표점을 얼마나 멀리 볼지 정한다. |
| `obstacle_slow_speed_kmh` | 선택 | number, km/h | `1.5` | `PathFollowConfigInfo.ObstacleSlowSpeedKmh` | 장애물 감속 구간에서 사용할 목표 속도. |

## lidar fields

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
| `scan_range_m` | 선택 | number, meter | `5.0` | `LidarSensorConfigInfo.ScanRangeM` | 라이다가 장애물을 탐지할 최대 거리. |
| `angle_step_degree` | 선택 | number, degree | `2.0` | `LidarSensorConfigInfo.AngleStepDegree` | 라이다 레이 간격. 작을수록 촘촘하게 감지한다. |
| `stop_distance_m` | 선택 | number, meter | `1.2` | `LidarSensorConfigInfo.StopDistanceM` | 전방 장애물이 이 거리 안에 있으면 정지한다. |
| `slow_down_distance_m` | 선택 | number, meter | `3.5` | `LidarSensorConfigInfo.SlowDownDistanceM` | 전방 장애물이 이 거리 안에 있으면 감속한다. |

## 검증 규칙

JSON에서 값이 빠지면 C++ 구조체 기본값을 그대로 사용한다.

| 필드 | 제한 |
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

## 전체 예시

```json
{
  "schema": "delivery_bot_setup",
  "version": 1,
  "robot": {
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
```

## 구현 메모

`UEpisodeCompiler::CompileRobotSpawn()`은 로봇 배치와 목적지를 EpisodeSetup에서 읽는다. 시작 위치는 `actors.robot.xy_m`/`actors.robot.yaw_deg`, 목적지는 `actors.robot.route.goal_xy_m`을 사용한다.

DeliveryBot 튜닝값은 `drive`, `path_follow`, `lidar`만 `FDeliveryBotSetupInfo`로 전달한다. `LocationSetupInfo`는 DeliveryBotSetup JSON에서 직접 열지 않고 EpisodeSetup의 배치/route 결과로 채운다.
