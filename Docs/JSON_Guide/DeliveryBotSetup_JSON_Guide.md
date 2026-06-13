# DeliveryBotSetup JSON Guide

이 문서는 DeliveryBot의 주행/센서/정책 튜닝값을 `FDeliveryBotSetupInfo`에 채우기 위한 JSON 범위를 정리한다.

DeliveryBotSetup JSON은 로봇 액터를 어디에, 어떤 ID로, 어떤 목표로 배치할지 결정하지 않는다. 그 책임은 ScenarioSetup JSON에 둔다.
Runner는 ScenarioSetup JSON과 DeliveryBotSetup JSON을 하나의 실행 pair로 묶어 컴파일한다.

## 책임 범위

ScenarioSetup JSON이 담당하는 값:

| 범위 | 예시 |
| --- | --- |
| Scenario 실행 정보 | `run.base_seed`, `run.iteration_index`, `run.time_limit_s` |
| 로봇 액터 식별 | `actors.robot.instance_id`, `actors.robot.asset_id` |
| 로봇 배치 | `actors.robot.spawn_only`, `actors.robot.xy_m`, `actors.robot.yaw_deg` |
| 로봇 목적지 | `actors.robot.route.goal_xy_m`, `actors.robot.route.auto_start` |

DeliveryBotSetup JSON이 담당하는 값:

| 범위 | 예시 |
| --- | --- |
| 주행 속도 튜닝 | `robot.drive.max_speed_kmh`, `robot.drive.slowdown_speed_range_kmh` |
| 경로 추종 튜닝 | `robot.path_follow.target_speed_kmh`, `robot.path_follow.look_ahead_distance_m`, `robot.path_follow.obstacle_slow_speed_kmh` |
| 라이다 반응 튜닝 | `robot.lidar.scan_range_m`, `robot.lidar.angle_step_degree`, `robot.lidar.stop_distance_m`, `robot.lidar.slow_down_distance_m` |

JSON에서 생략한 값은 C++ 구조체 기본값으로 fallback된다.

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
| `speed_limit_tolerance_kmh` | 선택 | number, km/h | `0.5` | `ChaosDriveConfigInfo.SpeedLimitToleranceKmh` | 제한 속도 초과를 허용하는 여유 범위. |
| `speed_limit_brake` | 선택 | number, 0-1 | `0.08` | `ChaosDriveConfigInfo.SpeedLimitBrake` | 과속 시 보정 브레이크 입력. |
| `stop_brake_input` | 선택 | number, 0-1 | `0.2` | `ChaosDriveConfigInfo.StopBrakeInput` | 정지 명령 시 최소 브레이크 입력. |
| `throttle_input_rate_per_second` | 선택 | number | `0.35` | `ChaosDriveConfigInfo.ThrottleInputRatePerSecond` | throttle 입력 변화율. |
| `brake_input_rate_per_second` | 선택 | number | `0.5` | `ChaosDriveConfigInfo.BrakeInputRatePerSecond` | brake 입력 변화율. |
| `steering_input_rate_per_second` | 선택 | number | `3.0` | `ChaosDriveConfigInfo.SteeringInputRatePerSecond` | steering 입력 변화율. |
| `acceleration_rate_kmh_per_second` | 선택 | number | `2.0` | `ChaosDriveConfigInfo.AccelerationRateKmhPerSecond` | 목표 속도 상승률. |
| `deceleration_rate_kmh_per_second` | 선택 | number | `3.0` | `ChaosDriveConfigInfo.DecelerationRateKmhPerSecond` | 목표 속도 하강률. |
| `use_handbrake_when_brake` | 선택 | bool | `false` | `ChaosDriveConfigInfo.bUseHandbrakeWhenBrake` | brake 명령 시 handbrake도 함께 사용할지 여부. |
| `max_torque` | 선택 | number | `220.0` | `ChaosDriveConfigInfo.MaxTorque` | Chaos vehicle engine max torque. |
| `max_rpm` | 선택 | number | `4000.0` | `ChaosDriveConfigInfo.MaxRPM` | Chaos vehicle engine max RPM. |
| `engine_idle_rpm` | 선택 | number | `600.0` | `ChaosDriveConfigInfo.EngineIdleRPM` | 엔진 idle RPM. |
| `engine_brake_effect` | 선택 | number | `0.04` | `ChaosDriveConfigInfo.EngineBrakeEffect` | 엔진 브레이크 효과. |
| `engine_rev_up_moi` | 선택 | number | `5.0` | `ChaosDriveConfigInfo.EngineRevUpMOI` | 엔진 rev-up inertia. |
| `engine_rev_down_rate` | 선택 | number | `600.0` | `ChaosDriveConfigInfo.EngineRevDownRate` | 엔진 rev-down rate. |

## path_follow fields

```json
"path_follow": {
  "target_speed_kmh": 10.0,
  "look_ahead_distance_m": 1.0,
  "obstacle_slow_speed_kmh": 2.0,
  "obstacle_soft_cost_radius_m": 2.0,
  "obstacle_soft_cost_max_penalty": 8.0,
  "obstacle_soft_cost_power": 2.0,
  "path_turn_cost_penalty": 1.5
}
```

| 필드 | 필수 | 타입/단위 | 기본값 | C++ 매핑 | 설명 |
| --- | --- | --- | --- | --- | --- |
| `target_speed_kmh` | 선택 | number, km/h | `10.0` | `PathFollowConfigInfo.TargetSpeedKmh` | 경로 추종 목표 속도. |
| `look_ahead_distance_m` | 선택 | number, meter | `1.0` | `PathFollowConfigInfo.LookAheadDistanceM` | 현재 위치보다 앞쪽의 추종 목표점을 얼마나 멀리 볼지 정한다. |
| `obstacle_slow_speed_kmh` | 선택 | number, km/h | `1.5` | `PathFollowConfigInfo.ObstacleSlowSpeedKmh` | 장애물 감속 구간에서 사용할 목표 속도. |
| `obstacle_soft_cost_radius_m` | 선택 | number, meter | `2.0` | `PathFollowConfigInfo.ObstacleSoftCostRadiusM` | A*가 장애물 주변 cell을 더 비싸게 평가하는 반경. |
| `obstacle_soft_cost_max_penalty` | 선택 | number | `8.0` | `PathFollowConfigInfo.ObstacleSoftCostMaxPenalty` | 장애물 바로 옆 cell에 더하는 최대 비용. |
| `obstacle_soft_cost_power` | 선택 | number | `2.0` | `PathFollowConfigInfo.ObstacleSoftCostPower` | 장애물과의 거리별 비용 감쇠 곡선. |
| `path_turn_cost_penalty` | 선택 | number | `1.5` | `PathFollowConfigInfo.PathTurnCostPenalty` | A*가 방향을 바꿀 때 더하는 비용. 값이 클수록 덜 꺾는 경로를 선호한다. |
| `draw_debug` | 선택 | bool | `true` | `PathFollowConfigInfo.bDrawDebug` | path follow debug draw 여부. |
| `path_point_acceptance_distance_m` | 선택 | number, meter | `0.4` | `PathFollowConfigInfo.PathPointAcceptanceDistanceM` | 중간 path point 통과 판정 거리. |
| `goal_acceptance_distance_m` | 선택 | number, meter | `0.8` | `PathFollowConfigInfo.GoalAcceptanceDistanceM` | path follower의 목표 도착 판정 거리. |
| `steering_sensitivity` | 선택 | number | `0.8` | `PathFollowConfigInfo.SteeringSensitivity` | 목표 방향에 대한 조향 민감도. |
| `min_turn_speed_kmh` | 선택 | number, km/h | `1.5` | `PathFollowConfigInfo.MinTurnSpeedKmh` | 큰 조향 시 낮출 최소 속도. |

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
| `draw_debug` | 선택 | bool | `true` | `LidarSensorConfigInfo.bDrawDebug` | lidar debug draw 여부. |
| `sensor_height_m` | 선택 | number, meter | `0.07` | `LidarSensorConfigInfo.SensorHeightM` | 라이다 ray 시작 높이. |
| `front_half_angle_degree` | 선택 | number, degree | `20.0` | `LidarSensorConfigInfo.FrontHalfAngleDegree` | 전방 물체로 분류할 좌우 half angle. |
| `store_missed_rays` | 선택 | bool | `false` | `LidarSensorConfigInfo.bStoreMissedRays` | hit하지 않은 ray도 scan에 저장할지 여부. |
| `trace_channel` | 선택 | string/number | `visibility` | `LidarSensorConfigInfo.TraceChannel` | lidar line trace channel. |
| `ignore_tags` | 선택 | string array | `["NoCollision"]` | `LidarSensorConfigInfo.IgnoreTags` | lidar가 무시할 actor tag 목록. |

## 검증 규칙

JSON에서 값이 빠지면 C++ 구조체 기본값을 그대로 사용한다.

| 필드 | 제한 |
| --- | --- |
| `max_speed_kmh` | `0` 이상 |
| `slowdown_speed_range_kmh` | 최소 `0.1` |
| `target_speed_kmh` | `0` 이상 |
| `look_ahead_distance_m` | 최소 `0.1` |
| `obstacle_slow_speed_kmh` | `0` 이상 |
| `obstacle_soft_cost_radius_m` | `0` 이상 |
| `obstacle_soft_cost_max_penalty` | `0` 이상 |
| `obstacle_soft_cost_power` | 최소 `0.1` |
| `path_turn_cost_penalty` | `0` 이상 |
| `scan_range_m` | `0` 이상 |
| `angle_step_degree` | 최소 `1.0` |
| `stop_distance_m` | `0` 이상 |
| `slow_down_distance_m` | `stop_distance_m + 0.1` 이상 |
| `speed_limit_brake`, `stop_brake_input` | `0`-`1` |
| `front_half_angle_degree` | `0`-`180` |

## Pair 실행 예시

```json
{
  "pair_id": "sample_0",
  "scenario_setup": "Json/Input/ScenarioSetupSample_0.json",
  "delivery_bot_setup": "Json/Input/DeliveryBotSetupSample_0.json"
}
```

여러 pair를 순서대로 실행할 때는 `Json/Input/ScenarioRunQueueSample.json`처럼 `runs` 배열로 묶고 `UScenarioRunnerSubsystem::StartBatchFromRunQueueJsonFile()`에 큐 파일 경로를 전달한다.
`scenario_setup`과 `delivery_bot_setup`은 둘 다 필수이며, Runner는 더 이상 기본 DeliveryBotSetup 파일로 fallback하지 않는다.

```json
{
  "schema": "episode_run_queue",
  "version": 1,
  "runs": [
    {
      "pair_id": "sample_0",
      "scenario_setup": "Json/Input/ScenarioSetupSample_0.json",
      "delivery_bot_setup": "Json/Input/DeliveryBotSetupSample_0.json"
    }
  ]
}
```

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
      "obstacle_slow_speed_kmh": 2.0,
      "obstacle_soft_cost_radius_m": 2.0,
      "obstacle_soft_cost_max_penalty": 8.0,
      "obstacle_soft_cost_power": 2.0,
      "path_turn_cost_penalty": 1.5
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

`UScenarioCompiler::CompileRobotSpawn()`은 로봇 배치와 목적지를 ScenarioSetup에서 읽는다. 시작 위치는 `actors.robot.xy_m`/`actors.robot.yaw_deg`, 목적지는 `actors.robot.route.goal_xy_m`을 사용한다.
`UDeliveryBotSetupCompiler`는 DeliveryBotSetup JSON을 `FDeliveryBotSetupInfo`로 컴파일한다. Runner는 두 결과를 merge할 때 `LocationSetupInfo`만 ScenarioSetup 결과로 유지하고, 나머지 DeliveryBot setup 값은 DeliveryBotSetup 결과를 사용한다.

DeliveryBot 튜닝값은 `drive`, `path_follow`, `lidar`만 `FDeliveryBotSetupInfo`로 전달한다. `LocationSetupInfo`는 DeliveryBotSetup JSON에서 직접 열지 않고 ScenarioSetup의 배치/route 결과로 채운다.
