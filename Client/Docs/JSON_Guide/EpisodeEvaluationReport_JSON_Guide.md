# Episode Evaluation Report JSON Guide

이 문서는 한 번의 Episode 실행 결과를 담은 `episode_evaluation_report` JSON 양식을 설명한다.

이 report를 통해 Episode에서 어떤 사건이 있었는지, 평가 결과가 왜 그렇게 나왔는지 해석한 뒤 EpisodeSetup JSON, DeliveryBotSetup JSON, 로봇 정책 파라미터, 환경 구성 변수를 조정할 근거로 삼을 수 있다.

정책 서버 통신 실패처럼 LLM/사용자가 직접 조정할 수 없는 인프라 오류는 이 report의 핵심 평가 데이터에서 제외한다. 이런 오류는 추후 사용자 에러 팝업이나 operator log에서 따로 다룬다.

## Root Example

```json
{
  "schema": "episode_evaluation_report",
  "version": 1,
  "units": {
    "time": "s",
    "distance": "m",
    "angle": "deg",
    "location": "xy_m",
    "speed": "km/h",
    "score": "episode_score"
  },
  "run": {
    "run_id": "episode_run_0004",
    "run_index": 4,
    "episode_id": "sensor_route_layout_004",
    "pair_id": "sample_4",
    "episode_setup": {
      "path": "Json/Input/EpisodeSetupSample_4.json",
      "hash": "252738887"
    },
    "delivery_bot_setup": {
      "path": "Json/Input/DeliveryBotSetupSample_4.json",
      "hash": "109204312"
    },
    "policy_spec": {
      "path": "Json/Input/PolicySpecs/PolicySpec_DefaultDelivery.json"
    },
    "pair_hash": "83029122"
  },
  "summary": {
    "completed": true,
    "success": false,
    "outcome": "Failure",
    "terminal_reason": "DeliveryBotSimulationFailed",
    "duration_s": 38.6,
    "score": -7.0,
    "usable_for_llm_tuning": true
  },
  "pipeline": {
    "episode_setup_compiled": true,
    "delivery_bot_setup_compiled": true,
    "world_setup_succeeded": true,
    "evaluation_completed": true,
    "diagnostics": []
  },
  "metrics": {
    "goal_reached": 0,
    "score": -7.0,
    "duration_s": 38.6,
    "goal_distance_m": 2.8,
    "static_obstacle_collision_count": 1,
    "blocked_region_collision_count": 0,
    "penalty_region_violation_count": 0,
    "pedestrian_collision_count": 0,
    "near_miss_count": 1,
    "near_miss_total_duration_s": 0.8,
    "near_miss_min_distance_m": 0.42,
    "robot_tip_over_count": 0,
    "delivery_bot_failure_type": "Stuck",
    "delivery_bot_failure_message": "DeliveryBot remained below movement threshold.",
    "delivery_bot_failure_xy_m": [3.8, 1.4],
    "delivery_bot_failure_time_s": 38.6,
    "delivery_bot_failure_speed_kmh": 0.1,
    "delivery_bot_failure_target_actor_name": ""
  },
  "event_summary": {
    "total": 3,
    "by_type": {
      "PedestrianNearMiss": 1,
      "StaticObstacleCollision": 1,
      "DeliveryBotSimulationFailure": 1
    },
    "by_severity": {
      "Info": 0,
      "Warning": 2,
      "Failure": 1
    },
    "first_failure_event_index": 2
  },
  "events": [
    {
      "i": 0,
      "t_s": 12.4,
      "type": "PedestrianNearMiss",
      "severity": "Warning",
      "subject": "robot_01",
      "target": "ped_01",
      "xy_m": [1.2, 0.7],
      "message": "Pedestrian near miss",
      "properties": {
        "start_time_s": 11.6,
        "end_time_s": 12.4,
        "duration_s": 0.8,
        "min_distance_m": 0.42,
        "pedestrian_id": "ped_01",
        "score_delta": -3.0
      }
    },
    {
      "i": 1,
      "t_s": 31.9,
      "type": "StaticObstacleCollision",
      "severity": "Warning",
      "subject": "robot_01",
      "target": "cone_01",
      "xy_m": [3.4, 0.9],
      "message": "Static obstacle collision",
      "properties": {
        "target_id": "cone_01",
        "target_actor": "BP_Cone_C_2",
        "score_delta": -4.0
      }
    },
    {
      "i": 2,
      "t_s": 38.6,
      "type": "DeliveryBotSimulationFailure",
      "severity": "Failure",
      "subject": "robot_01",
      "target": "",
      "xy_m": [3.8, 1.4],
      "message": "DeliveryBot remained below movement threshold.",
      "properties": {
        "failure_type": "Stuck",
        "speed_kmh": 0.1,
        "target_actor_name": ""
      }
    }
  ]
}
```

## Top-Level Fields

| Field | Type | Description |
| --- | --- | --- |
| `schema` | string | 고정값. `episode_evaluation_report`를 사용한다. |
| `version` | integer | report 양식 버전. 초기 버전은 `1`이다. |
| `units` | object | report 전체에서 사용하는 단위 계약. |
| `run` | object | 실행 ID, EpisodeSetup/DeliveryBotSetup/PolicySpec 조합, hash 정보. |
| `summary` | object | Episode 최종 평가 요약. |
| `pipeline` | object | compile, setup, evaluation pipeline 성공 여부와 진단 정보. |
| `metrics` | object | Episode 전체에 대한 누적 수치와 최종 상태. |
| `event_summary` | object | event 개수, type/severity별 집계, 첫 failure 위치. |
| `events` | array | LLM이 해석해야 할 주요 사건 목록. 시간순 또는 event index순으로 정렬한다. |

## Units

| Field | Value | Description |
| --- | --- | --- |
| `time` | `s` | 모든 시간 값은 초 단위이다. |
| `distance` | `m` | 모든 거리 값은 meter 단위이다. |
| `angle` | `deg` | 모든 각도 값은 degree 단위이다. |
| `location` | `xy_m` | 위치는 `[x, y]` 2D meter 좌표로 표현한다. Z는 LLM report에서 생략한다. |
| `speed` | `km/h` | 로봇 속도는 km/h 단위이다. |
| `score` | `episode_score` | Episode evaluation score. 절대 단위가 아니라 평가 규칙에 따른 누적 점수이다. |

내부 UE runtime이나 `FEpisodeEvaluationResult`에는 centimeter 기반 위치가 남을 수 있다. LLM용 report exporter는 `LocationCm` 또는 `*_location_cm` 값을 `xy_m`으로 변환해 출력한다.

## Run

| Field | Type | Description |
| --- | --- | --- |
| `run.run_id` | string | Runner가 부여한 단일 실행 ID. |
| `run.run_index` | integer | batch queue 안에서의 실행 순서. 단일 실행이면 `0`을 권장한다. |
| `run.episode_id` | string | EpisodeSetup JSON의 scenario/episode ID. |
| `run.pair_id` | string | EpisodeSetup JSON과 DeliveryBotSetup JSON을 묶는 pair ID. |
| `run.episode_setup.path` | string | 실행에 사용한 EpisodeSetup JSON 경로. |
| `run.episode_setup.hash` | string | EpisodeSetup JSON 또는 compile spec hash. |
| `run.delivery_bot_setup.path` | string | 실행에 사용한 DeliveryBotSetup JSON 경로. |
| `run.delivery_bot_setup.hash` | string | DeliveryBotSetup JSON 또는 compile spec hash. |
| `run.policy_spec.path` | string | 실행에 사용한 PolicySpec JSON 경로. |
| `run.pair_hash` | string | EpisodeSetup hash, DeliveryBotSetup hash, PolicySpec 경로를 조합한 pair hash. |

`pair_id`와 `pair_hash`는 LLM이 "어떤 환경 구성, 로봇 설정, 정책 스펙의 조합에서 나온 결과인지"를 추적하기 위한 값이다. EpisodeSetup, DeliveryBotSetup, PolicySpec 중 하나만 바꾸는 실험에서도 pair 단위로 결과를 비교한다.

## Summary

| Field | Type | Description |
| --- | --- | --- |
| `summary.completed` | bool | Episode evaluation이 종료 콜백까지 정상적으로 도달했는지 여부. |
| `summary.success` | bool | Episode 목표 달성 여부. `outcome`보다 단순한 boolean 플래그이다. |
| `summary.outcome` | string | `Success`, `Warning`, `Failure`, `Cancelled` 중 하나. |
| `summary.terminal_reason` | string | Episode가 종료된 직접 원인. |
| `summary.duration_s` | number | Episode 시작 후 종료까지 걸린 시간. |
| `summary.score` | number | 최종 누적 점수. |
| `summary.usable_for_llm_tuning` | bool | 이 결과를 LLM이 튜닝 근거로 사용해도 되는지 여부. |

### Outcome

| Value | Description |
| --- | --- |
| `Success` | 목표를 달성했고 심각한 실패 없이 종료했다. |
| `Warning` | 목표는 달성했지만 collision, near-miss, penalty 같은 감점 사건이 있었다. |
| `Failure` | 목표 달성 전 실패 조건으로 종료했다. |
| `Cancelled` | 사용자 또는 runner 흐름에 의해 중단되었다. |

### Terminal Reason

| Value | LLM Tuning | Description |
| --- | --- | --- |
| `GoalReached` | true | 목표 지점에 도달했다. |
| `Timeout` | true | 제한 시간 안에 목표에 도달하지 못했다. |
| `RobotTipOver` | true | 로봇 기울기가 tip-over threshold를 넘었다. |
| `DeliveryBotSimulationFailed` | depends | DeliveryBot actor가 simulation failure를 보고했다. |
| `CompileFailed` | false | EpisodeSetup 또는 DeliveryBotSetup compile 실패. |
| `SetupFailed` | false | 월드 배치 또는 runtime setup 실패. |
| `EvaluationStartFailed` | false | 평가 시작 조건이 충족되지 않았다. |
| `Cancelled` | false | 외부 중단. |

`DeliveryBotSimulationFailed` 중 LLM tuning 대상으로 삼는 failure type은 `RobotTipOver`, `PathFindingFailed`, `Stuck`뿐이다. `PolicyRequestFailed` 같은 통신 오류는 report의 평가 근거에서 제외한다.

## Pipeline

시뮬레이션 흐름에서 문제가 있었는지 여부를 판단하기 위한 값이다.

| Field | Type | Description |
| --- | --- | --- |
| `pipeline.episode_setup_compiled` | bool | EpisodeSetup JSON compile 성공 여부. |
| `pipeline.delivery_bot_setup_compiled` | bool | DeliveryBotSetup JSON compile 성공 여부. |
| `pipeline.world_setup_succeeded` | bool | runtime actor, ground region, path 등 월드 배치 성공 여부. |
| `pipeline.evaluation_completed` | bool | EvaluationSubsystem이 최종 result를 완성했는지 여부. |
| `pipeline.diagnostics` | string array | compile/setup/evaluation 단계의 경고 또는 오류 메시지. |

`pipeline` 오류는 대체로 LLM 튜닝 근거가 아니라 입력 JSON 형식, asset path, setup pipeline 문제를 찾기 위한 정보이다. 

## Metrics

`metrics`는 Episode 전체를 요약하는 key-value object이다. 값은 number, string, bool, array를 사용할 수 있다.

| Field | Type | Description |
| --- | --- | --- |
| `goal_reached` | number | 목표 도달 횟수. 보통 `0` 또는 `1`. |
| `score` | number | 최종 누적 점수. `summary.score`와 같은 값을 권장한다. |
| `duration_s` | number | 최종 duration. `summary.duration_s`와 같은 값을 권장한다. |
| `goal_distance_m` | number | 종료 시점 또는 goal reached 시점의 목표 거리. |
| `static_obstacle_collision_count` | number | 정적 장애물 충돌 횟수. |
| `blocked_region_collision_count` | number | blocked region 충돌 횟수. |
| `penalty_region_violation_count` | number | penalty region 체류/위반 횟수. |
| `pedestrian_collision_count` | number | 보행자 충돌 횟수. |
| `near_miss_count` | number | 보행자 near-miss 구간 수. |
| `near_miss_total_duration_s` | number | near-miss 누적 시간. |
| `near_miss_min_distance_m` | number | near-miss 중 최단 거리. |
| `robot_tip_over_count` | number | 로봇 전복 감지 횟수. |
| `delivery_bot_failure_type` | string | DeliveryBot simulation failure type. LLM report에는 `RobotTipOver`, `PathFindingFailed`, `Stuck`만 사용한다. |
| `delivery_bot_failure_message` | string | failure에 대한 사람이 읽는 메시지. |
| `delivery_bot_failure_xy_m` | number array | failure 발생 위치 `[x, y]` meter. |
| `delivery_bot_failure_time_s` | number | DeliveryBot이 보고한 failure time. |
| `delivery_bot_failure_speed_kmh` | number | failure 시점 로봇 속도. |
| `delivery_bot_failure_target_actor_name` | string | failure와 관련된 target actor 이름. 없으면 빈 문자열. |

metric이 발생하지 않은 경우에는 생략하거나 `0`을 사용할 수 있다. LLM 입력 안정성을 우선한다면 count 계열은 `0`으로 채우는 방식을 권장한다.

## Event Summary

| Field | Type | Description |
| --- | --- | --- |
| `event_summary.total` | integer | `events` 배열에 포함된 event 개수. |
| `event_summary.by_type` | object | event `type`별 개수. key는 event type 문자열이다. |
| `event_summary.by_severity` | object | `Info`, `Warning`, `Failure`별 개수. |
| `event_summary.first_failure_event_index` | integer or null | 첫 `Failure` severity event의 `i`. failure event가 없으면 `null`. |

`event_summary`는 LLM이 긴 `events` 배열을 읽기 전에 전체 패턴을 빠르게 파악하기 위한 요약이다.

## Events

각 event는 Episode 중 의미 있는 사건 하나를 나타낸다. 모든 tick을 넣지 않고, LLM이 판단 근거로 사용할 수 있는 incident만 넣는다.

| Field | Type | Description |
| --- | --- | --- |
| `i` | integer | event index. `0`부터 시작한다. |
| `t_s` | number | Episode 시작 기준 event 발생 시간. |
| `type` | string | event type. UE enum 이름을 그대로 쓰는 것을 권장한다. |
| `severity` | string | `Info`, `Warning`, `Failure` 중 하나. |
| `subject` | string | 사건의 주체. 보통 로봇 instance ID. |
| `target` | string | 사건의 대상 actor/region/pedestrian instance ID. 없으면 빈 문자열. |
| `xy_m` | number array | event 발생 위치 `[x, y]` meter. |
| `message` | string | 사람이 읽는 짧은 event 설명. |
| `properties` | object | event type별 추가 정보. |

### Event Types

| Type | Severity | Description |
| --- | --- | --- |
| `Timeout` | `Failure` | 제한 시간 초과. |
| `RobotTipOver` | `Failure` | 로봇 전복. |
| `StaticObstacleCollision` | `Warning` or `Failure` | 정적 장애물 충돌. 현재 평가는 보통 감점 warning으로 다룬다. |
| `BlockedRegionCollision` | `Warning` or `Failure` | blocked region 충돌. |
| `PenaltyRegionViolation` | `Warning` | penalty region 체류/위반. |
| `PedestrianNearMiss` | `Warning` | 보행자와 near-miss 거리 안에 들어간 구간. |
| `PedestrianCollision` | `Warning` or `Failure` | 보행자 충돌. |
| `DeliveryBotSimulationFailure` | `Failure` | DeliveryBot actor가 조정 가능한 simulation failure를 보고했다. |

### Common Event Properties

| Field | Type | Description |
| --- | --- | --- |
| `score_delta` | number | 이 event가 score에 준 변화량. 감점은 음수. |
| `target_id` | string | target instance ID. |
| `target_actor` | string | Unreal actor name. |
| `region_id` | string | 관련 ground region ID. |

### PedestrianNearMiss Properties

| Field | Type | Description |
| --- | --- | --- |
| `start_time_s` | number | near-miss 시작 시간. |
| `end_time_s` | number | near-miss 종료 시간. |
| `duration_s` | number | near-miss 지속 시간. |
| `min_distance_m` | number | 구간 중 로봇과 보행자 사이 최단 거리. |
| `pedestrian_id` | string | 관련 pedestrian instance ID. |
| `score_delta` | number | near-miss 감점. |

### Collision Properties

| Field | Type | Description |
| --- | --- | --- |
| `target_id` | string | 충돌 대상 instance ID. |
| `target_actor` | string | 충돌 대상 Unreal actor name. |
| `region_id` | string | ground region 충돌일 때 region ID. |
| `score_delta` | number | 충돌 감점. |

### DeliveryBotSimulationFailure Properties

| Field | Type | Description |
| --- | --- | --- |
| `failure_type` | string | `RobotTipOver`, `PathFindingFailed`, `Stuck` 중 하나. |
| `speed_kmh` | number | failure 시점 로봇 속도. |
| `target_actor_name` | string | 관련 target actor 이름. 없으면 빈 문자열. |

`PolicyRequestFailed`, HTTP response code, request ID, server URL, request elapsed time 같은 통신 세부 정보는 이 report에 넣지 않는다.

## LLM Tuning Usability

`summary.usable_for_llm_tuning`은 LLM이 이 report를 근거로 다음 JSON 또는 policy setting을 조정해도 되는지 알려준다.

| Case | Value | Reason |
| --- | --- | --- |
| Goal reached with warning events | `true` | 성공했지만 더 안전하거나 효율적인 구성이 가능하다. |
| Timeout | `true` | route, speed, obstacle layout, policy setting 조정 근거가 된다. |
| Collision or near-miss | `true` | 환경 배치와 정책 반응을 조정할 근거가 된다. |
| `RobotTipOver` | `true` | 속도, 회전, 경로, 물리 세팅 조정 근거가 된다. |
| `PathFindingFailed` | `true` | route, obstacle, blocked region, path planning setting 조정 근거가 된다. |
| `Stuck` | `true` | local policy, obstacle avoidance, speed, route setting 조정 근거가 된다. |
| Compile/setup/evaluation start failure | `false` | JSON 형식, asset, pipeline 문제이지 episode behavior 평가가 아니다. |
| Policy server communication failure | `false` | LLM이 EpisodeSetup/DeliveryBotSetup을 바꿔도 해결할 수 없는 인프라 문제이다. |
| User cancelled | `false` | 의도적인 중단이라 평가 근거로 쓰지 않는다. |

## Output Rules

- report는 valid JSON object 하나로 출력한다.
- 위치는 항상 `xy_m`으로 출력하고 centimeter raw location은 노출하지 않는다.
- policy 통신 failure 상세는 출력하지 않는다.
- `events`는 중요한 incident만 포함한다. every tick data는 measurement JSONL이나 별도 incident window export로 분리한다.
- `events`는 `i` 오름차순으로 정렬한다.
- count metric은 없으면 `0`으로 채우는 것을 권장한다.
- 빈 target은 `null`보다 빈 문자열 `""`을 권장한다. LLM prompt에서 조건 분기가 단순해진다.
