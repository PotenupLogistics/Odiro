# DeliveryBot Episode 소환 JSON 가이드

이 문서는 Episode 담당자가 DeliveryBot을 Episode 안에 소환하고, 시작 위치와 도착 위치를 지정할 때 따라야 하는 JSON 구조를 정리한다.

핵심 원칙은 다음과 같다.

```text
EpisodeSetup JSON
-> 맵, 시작 위치, 도착 위치, 장애물, 보행자, 평가 반경 담당

DeliveryBotSetup JSON
-> 배송봇 속도, 라이다, 감속/정지 거리, 주행 튜닝 담당

EpisodeRunQueue JSON
-> EpisodeSetup + DeliveryBotSetup + PolicySpec 조합을 실행 단위로 묶음
```

## 1. 파일 역할 분리

| 파일 | 담당 |
|---|---|
| `EpisodeSetup*.json` | 로봇 배치, 시작 위치, 도착 위치, 평가 설정, 장애물/보행자 배치 |
| `DeliveryBotSetup*.json` | 로봇 주행 성능, 라이다, 정책 주행 튜닝 |
| `EpisodeRunQueue*.json` | 실행할 `EpisodeSetup`과 `DeliveryBotSetup` 파일 pair 지정 |

`EpisodeSetup` 안에 `drive`, `path_follow`, `lidar` 설정을 넣지 않는다. 이 값들은 반드시 `DeliveryBotSetup`에 둔다.

## 2. EpisodeSetup JSON 예시

시작 위치와 도착 위치는 `actors.robot` 아래에 지정한다.

```json
{
  "schema": "episode_actor_spawn_mvp",
  "version": 1,
  "scenario_id": "delivery_policy_test_001",
  "map_id": "EpisodeEditorMap",
  "run": {
    "base_seed": 2001,
    "iteration_index": 0,
    "time_limit_s": 60
  },
  "evaluation": {
    "goal_acceptance_radius_m": 1.0,
    "tip_over_angle_deg": 60,
    "near_miss": {
      "distance_m": 0.5
    },
    "scoring": {
      "static_obstacle_collision": -1,
      "blocked_region_collision": -1,
      "penalty_region_violation": -3,
      "pedestrian_near_miss": -3,
      "pedestrian_collision": -10
    }
  },
  "ground_model": {
    "default_region_type": "walkable",
    "regions": [
      {
        "region_id": "main_walkable_area",
        "region_type": "walkable",
        "shape": {
          "type": "rectangle",
          "center_xy_m": [0, 0],
          "size_m": [20, 12],
          "yaw_deg": 0
        },
        "traversability_score": 1
      }
    ]
  },
  "paths": [],
  "actors": {
    "static_obstacles": [],
    "pedestrians": [],
    "robot": {
      "instance_id": "robot_delivery_001",
      "asset_id": "delivery_bot",
      "spawn_only": false,
      "xy_m": [-6, 0],
      "yaw_deg": 0,
      "route": {
        "goal_xy_m": [6, 0],
        "auto_start": true
      }
    }
  }
}
```

## 3. DeliveryBot 시작/도착 위치 지정

### 시작 위치

```json
"xy_m": [-6, 0]
```

`actors.robot.xy_m`은 로봇 시작 위치다. 단위는 미터다.

### 시작 방향

```json
"yaw_deg": 0
```

`actors.robot.yaw_deg`는 로봇 시작 회전값이다. 단위는 degree다.

### 도착 위치

```json
"route": {
  "goal_xy_m": [6, 0],
  "auto_start": true
}
```

도착 위치는 반드시 `actors.robot.route.goal_xy_m`에 넣는다. 단위는 미터다.

### 자동 주행 여부

```json
"auto_start": true
```

`route.auto_start`가 `true`이면 로봇 setup 정보에 자동 경로 시작 플래그가 들어간다.

### 배치만 하는 경우

```json
"spawn_only": true
```

`spawn_only`가 `true`이면 로봇을 배치만 하고 주행 목표를 사용하지 않는 용도다. 정책 기반 주행 테스트에서는 일반적으로 `false`를 사용한다.

## 4. 도착 판정

도착 판정은 `EpisodeEvaluationSubsystem`이 담당한다.

```json
"evaluation": {
  "goal_acceptance_radius_m": 1.0
}
```

`goal_acceptance_radius_m`은 최종 도착 판정 반경이다. 단위는 미터다.

현재 판정 방식은 다음과 같다.

```cpp
const FVector robotLocation = ActiveRuntimeContext.RobotActor->GetActorLocation();
const double goalDistanceCm = FVector::Dist2D(robotLocation, ActiveRuntimeContext.GoalLocation);

if (goalDistanceCm <= acceptanceRadiusCm)
{
    FinishEpisode(... GoalReached);
}
```

따라서 `BuildRuntimeContext()`에서 다음 값이 정상적으로 들어가야 한다.

```cpp
runtimeContext.RobotActor = runtimeActor;
runtimeContext.GoalLocation = placeableSpec.DeliveryBot.SetupInfo.LocationSetupInfo.GoalLocationCm;
runtimeContext.bHasGoalLocation = true;
```

## 5. DeliveryBotSetup JSON 예시

로봇 성능, 라이다, 정책 주행 튜닝은 별도 `DeliveryBotSetup` 파일에 둔다.

```json
{
  "schema": "delivery_bot_setup",
  "version": 1,
  "robot": {
    "drive": {
      "max_speed_kmh": 7.0,
      "slowdown_speed_range_kmh": 3.0,
      "stop_brake_input": 1.0,
      "brake_input_rate_per_second": 5.0,
      "use_handbrake_when_brake": true
    },
    "path_follow": {
      "target_speed_kmh": 5.0,
      "look_ahead_distance_m": 1.2,
      "goal_acceptance_distance_m": 0.8,
      "obstacle_slow_speed_kmh": 2.0
    },
    "lidar": {
      "scan_range_m": 6.0,
      "angle_step_degree": 5.0,
      "stop_distance_m": 1.1,
      "slow_down_distance_m": 3.5
    }
  }
}
```

`path_follow.goal_acceptance_distance_m`은 Python 정책의 보조적인 goal 판단값이다. 최종 도착 판정은 `EpisodeSetup.evaluation.goal_acceptance_radius_m`이 담당한다.

권장 관계는 다음과 같다.

```text
DeliveryBotSetup.robot.path_follow.goal_acceptance_distance_m
<= EpisodeSetup.evaluation.goal_acceptance_radius_m
```

예를 들어 Evaluation 반경이 `1.0m`이면 Python 정책 보조 반경은 `0.8m` 정도로 둘 수 있다.

## 6. EpisodeRunQueue JSON 예시

실행할 때는 `EpisodeSetup`과 `DeliveryBotSetup`을 pair로 묶는다.

```json
{
  "schema": "episode_run_queue",
  "version": 1,
  "runs": [
    {
      "pair_id": "delivery_policy_test_001",
      "episode_setup": "Json/Input/EpisodeSetupDeliveryPolicyTest.json",
      "delivery_bot_setup": "Json/Input/DeliveryBotSetupPlayable.json",
      "policy_spec": "Json/Input/PolicySpecs/PolicySpec_DefaultDelivery.json"
    }
  ]
}
```

실행 단위는 다음과 같다.

```text
EpisodeSetup + DeliveryBotSetup + PolicySpec = 1개 실행 pair
```

## 7. 사용하면 안 되는 필드

다음 필드는 더 이상 사용하지 않는다.

```text
actors.robot.location
actors.robot.goal_xy_m
actors.robot.goal_m
actors.robot.route.goal_m
actors.robot.drive
actors.robot.path_follow
actors.robot.lidar
```

대신 다음 필드를 사용한다.

```text
actors.robot.xy_m
actors.robot.yaw_deg
actors.robot.route.goal_xy_m
actors.robot.route.auto_start
```

## 8. ChaosActor에서 DeliveryBot으로 교체할 때 유지해야 할 것

현재 `EpisodeSimulationSubsystem::SpawnRobotActor()`는 `ADeliveryBot_ChaosActor`를 소환한다. 추후 `ADeliveryBot`으로 교체하더라도 다음 흐름은 유지해야 한다.

```cpp
RegisterRuntimeActor(
    placeableSpec.InstanceId,
    placeableSpec.AssetId,
    placeableSpec.Category,
    EEpisodeMobilityMode::Moving,
    robotActor
);
```

그리고 `BuildRuntimeContext()`에서 소환된 로봇이 `RobotActor`에 들어가야 한다.

```cpp
runtimeContext.RobotActor = runtimeActor;
runtimeContext.GoalLocation = placeableSpec.DeliveryBot.SetupInfo.LocationSetupInfo.GoalLocationCm;
runtimeContext.bHasGoalLocation = true;
```

이 흐름이 유지되면 `EpisodeEvaluationSubsystem`의 도착 판정은 `ADeliveryBot_ChaosActor`에서 `ADeliveryBot`으로 교체해도 그대로 동작한다.

## 9. Episode 담당자에게 전달할 요약

```text
EpisodeSetup에는 시작 위치 xy_m, 시작 yaw_deg, route.goal_xy_m, evaluation.goal_acceptance_radius_m을 넣는다.
DeliveryBotSetup에는 주행/센서/속도 설정만 넣는다.
두 파일은 EpisodeRunQueue에서 pair로 묶어 실행한다.
도착 판정은 EpisodeEvaluationSubsystem이 runtimeContext.RobotActor 위치와 GoalLocation 거리로 판단한다.
ChaosActor를 DeliveryBot으로 교체해도 RuntimeContext에 RobotActor와 GoalLocation만 정상 전달되면 도착 판정은 유지된다.
```
