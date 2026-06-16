# Episode Events

경로:

```text
<UserProject>/runs/<RunId>/episodes/<EpisodeId>/events.jsonl
```

schema:

```json
"episode_event"
```

## 합의

- JSON Lines 형식이다.
- 한 줄은 episode 중 결과 해석에 의미 있는 사건 하나다.
- 모든 canonical JSON 필드는 `snake_case`를 사용한다.
- 모든 tick이나 모든 action을 기록하지 않는다.
- `severity`는 사용하지 않는다.
- 전역 위치/거리 단위는 meter다. 속도는 km/h, 각도는 degree를 사용한다.
- `result.json`은 terminal result의 source of truth다.
- episode 종료 원인이 된 사건은 `events.jsonl`에 반드시 남긴다.
- `result.json.summary.terminal_event_index`는 가능하면 종료 원인 event를 참조한다.

## Line

```json
{
  "schema": "episode_event",
  "version": 1,
  "event_index": 1,
  "run_time_seconds": 4.12,
  "source": "EvaluationSubsystem",
  "event_type": "PedestrianNearMiss",
  "reason": "distance_below_threshold",
  "message": "Robot passed too close to a pedestrian.",
  "action_sequence": 75,
  "properties": {
    "target_id": "ped_enc_main_conflict",
    "target_actor": "BP_Pedestrian_C_01",
    "robot_along_m": 13.0,
    "robot_offset_m": 0.1,
    "target_along_m": 13.1,
    "target_offset_m": 0.0,
    "start_time_s": 3.92,
    "end_time_s": 4.12,
    "duration_s": 0.2,
    "min_distance_m": 0.32,
    "threshold_m": 0.5,
    "speed_kmh": 2.8
  }
}
```

## Root

| 필드 | 타입 | 합의 |
| --- | --- | --- |
| `schema` | string | 고정값 `episode_event` |
| `version` | number | schema version. v1은 `1` |
| `event_index` | number | episode 내 event 순번 |
| `run_time_seconds` | number | episode 실행 시간. 단위 s |
| `source` | string | 이벤트를 만든 주체 |
| `event_type` | string | 이벤트 종류 |
| `reason` | string | 기계가 읽기 좋은 원인 코드 |
| `message` | string | 사람이 읽기 좋은 짧은 설명 |
| `action_sequence` | number or null | event time 이전의 마지막 `actions.jsonl.sequence`. 연결할 action이 없으면 `null` |
| `properties` | object | 이벤트별 상세 요약. 필요한 필드만 기록한다 |

## Source Types

| 값 | 합의 |
| --- | --- |
| `EvaluationSubsystem` | Unreal evaluation system이 판단한 event |
| `PythonPolicy` | Python policy 또는 pathfinder가 의도적으로 보고한 policy-level event |
| `PolicyRuntime` | policy 호출/통신/응답 처리 계층이 관측한 runtime event |

## Event Types

| 값 | 주 source | 합의 |
| --- | --- | --- |
| `Timeout` | `EvaluationSubsystem` | 제한 시간 초과 |
| `RobotTipOver` | `EvaluationSubsystem` | 로봇 전복 |
| `StaticObstacleCollision` | `EvaluationSubsystem` | 정적 장애물 충돌 |
| `BlockedRegionCollision` | `EvaluationSubsystem` | blocked region 충돌 |
| `PenaltyRegionViolation` | `EvaluationSubsystem` | penalty region 위반 |
| `PedestrianNearMiss` | `EvaluationSubsystem` | 보행자 near-miss |
| `PedestrianCollision` | `EvaluationSubsystem` | 보행자 충돌 |
| `DeliveryBotSimulationFailure` | `EvaluationSubsystem` | 조정 가능한 DeliveryBot simulation failure |
| `GoalReached` | `EvaluationSubsystem` | 목표 지점 도착 |
| `Stuck` | `EvaluationSubsystem` | 이동 불능 또는 정체 상태 |
| `Repath` | `PythonPolicy` | Python policy가 경로를 다시 계산함 |
| `PathfindFail` | `PythonPolicy` | Python policy 또는 pathfinder가 유효 경로를 찾지 못함 |
| `PolicyDecisionError` | `PolicyRuntime` | policy 호출 실패, 통신 실패, invalid action 등으로 episode를 종료함 |

## Properties

`properties`는 이벤트별로 필요한 필드만 기록한다. 사용하지 않는 필드는 넣지 않는다.

| 필드 | 사용처 |
| --- | --- |
| `target_id` | target semantic id. `scenario.semantic.static_obstacles[].id` 또는 `scenario.semantic.pedestrians[].id`와 조인 |
| `target_actor` | Unreal actor name |
| `target_tags` | target actor tag 목록 |
| `region_id` | ground region 관련 event |
| `robot_along_m`, `robot_offset_m` | event 시점 로봇의 corridor-relative 위치 |
| `target_along_m`, `target_offset_m` | event target의 corridor-relative 위치 |
| `start_time_s`, `end_time_s`, `duration_s` | near-miss, penalty region, stuck 등 구간 event |
| `min_distance_m` | near-miss 최단 거리 |
| `threshold_m` | 거리 기반 판단 기준 |
| `distance_to_goal_m` | goal 접근 판단 시 목표까지 남은 거리 |
| `goal_threshold_m` | goal 도착 인정 거리 |
| `failure_type` | DeliveryBot simulation failure 또는 pathfind fail 세부 유형 |
| `speed_kmh` | event 시점 로봇 속도 |
| `roll_degree`, `pitch_degree`, `threshold_degree` | 전복 판단 각도 |
| `path_index`, `path_length` | Repath 또는 PathfindFail 시점 path 상태 |
| `blocked_corridor_cell_count` | Repath 판단 시 막힌 corridor cell 수 |
| `dynamic_blocked_cell_count` | Repath 판단 시 동적으로 막힌 cell 수 |
| `error_code`, `error_message` | `PolicyDecisionError` 세부 오류 |

## Action Join

`events.jsonl`은 action 원본을 복사하지 않고 `action_sequence`로 `actions.jsonl`을 참조한다.

```text
events.jsonl.action_sequence == actions.jsonl.sequence
```

기본 규칙:

- `action_sequence`는 event time 이전의 마지막 `actions.jsonl.sequence`다.
- `PolicyDecisionError`의 `action_sequence`는 실패한 `actions.jsonl` error line의 `sequence`다.
- action이 존재하지 않는 setup/evaluation event는 `action_sequence: null`을 사용한다.
- `trace.jsonl`과의 조인은 `run_time_seconds`로 한다.

## PolicyDecisionError

정책 실패나 통신 연결 실패가 발생하면 다음 순서로 기록한다.

1. `actions.jsonl`에 실패한 decision attempt를 `status: "error"`로 기록한다.
2. `events.jsonl`에 `PolicyDecisionError`를 1회 기록한다.
3. episode를 종료한다.
4. terminal result는 `result.json`에 기록한다.

```json
{
  "schema": "episode_event",
  "version": 1,
  "event_index": 4,
  "run_time_seconds": 13.0,
  "source": "PolicyRuntime",
  "event_type": "PolicyDecisionError",
  "reason": "policy_connection_failed",
  "message": "Policy runtime failed to return a valid action.",
  "action_sequence": 80,
  "properties": {
    "error_code": "SERVER_UNREACHABLE",
    "error_message": "Connection refused.",
    "robot_along_m": 12.4,
    "robot_offset_m": 0.2
  }
}
```

## 기록하지 않는 것

| 항목 | 이유 |
| --- | --- |
| 전체 `lidar_rays` | `actions.jsonl`에서 `action_sequence`로 조회한다 |
| 전체 `observed_objects` | `actions.jsonl`에서 `action_sequence`로 조회한다 |
| 전체 `action` | `actions.jsonl`에서 `action_sequence`로 조회한다 |
| 전체 `path_world_points` | `actions.jsonl`에서 `action_sequence`로 조회한다 |
| 로봇이 보지 못한 전체 월드 상태 | `trace.jsonl`에서 `run_time_seconds`로 조회한다 |
