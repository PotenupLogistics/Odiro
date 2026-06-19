# events.jsonl

Episode 중 결과 해석에 필요한 사건을 기록하는 JSON Lines 파일이다. 모든 tick이나 모든 action을 기록하지 않는다.

## 경로

```text
runs/<RunId>/episodes/<EpisodeId>/events.jsonl
```

## line schema

```json
"episode_event"
```

## Line Root Fields

| 필드 | 타입 | 필수 | 설명 |
| --- | --- | --- | --- |
| `schema` | string | 예 | 고정값 `episode_event`. |
| `version` | number | 예 | 고정값 `1`. |
| `event_index` | number | 예 | Episode 안의 event 순번. |
| `run_time_seconds` | number | 예 | Episode 실행 시간. 단위 s. |
| `source` | string | 예 | Event 생성 주체. |
| `event_type` | string | 예 | Event 종류. |
| `reason` | string | 예 | 기계가 읽기 좋은 원인 코드. |
| `message` | string | 예 | 사람이 읽는 짧은 설명. |
| `action_sequence` | number or null | 예 | Event time 이전 마지막 `actions.jsonl.sequence`. 연결할 action이 없으면 `null`. |
| `properties` | object | 예 | Event별 상세 요약. 필요한 필드만 기록한다. |

## source

| 값 | 설명 |
| --- | --- |
| `EvaluationSubsystem` | Unreal evaluation system이 판단한 event. |
| `PythonPolicy` | Python policy 또는 pathfinder가 보고한 policy-level event. |
| `PolicyRuntime` | Policy 호출, 통신, 응답 처리 계층이 관측한 runtime event. |

## event_type

| 값 | 주 source | 설명 |
| --- | --- | --- |
| `Timeout` | `EvaluationSubsystem` | 제한 시간 초과. |
| `RobotTipOver` | `EvaluationSubsystem` | Robot 전복. |
| `StaticObstacleCollision` | `EvaluationSubsystem` | 정적 장애물 충돌. |
| `BlockedRegionCollision` | `EvaluationSubsystem` | Blocked region 충돌. |
| `PenaltyRegionViolation` | `EvaluationSubsystem` | Penalty region 위반. |
| `PedestrianNearMiss` | `EvaluationSubsystem` | 보행자 near-miss. |
| `PedestrianCollision` | `EvaluationSubsystem` | 보행자 충돌. |
| `DeliveryBotSimulationFailure` | `EvaluationSubsystem` | 조정 가능한 DeliveryBot simulation failure. |
| `GoalReached` | `EvaluationSubsystem` | 목표 지점 도착. |
| `Stuck` | `EvaluationSubsystem` | 이동 불능 또는 정체 상태. |
| `Repath` | `PythonPolicy` | Python policy가 경로를 다시 계산함. |
| `PathfindFail` | `PythonPolicy` | Python policy 또는 pathfinder가 유효 경로를 찾지 못함. |
| `PolicyDecisionError` | `PolicyRuntime` | Policy 호출 실패, 통신 실패, invalid action 등으로 episode를 종료함. |

## properties

| 필드 | 사용처 |
| --- | --- |
| `target_id` | `scenario.semantic.static_obstacles[].id` 또는 `scenario.semantic.pedestrians[].id`와 조인. |
| `target_actor` | Unreal actor name. |
| `target_tags` | Target actor tag 목록. |
| `region_id` | Ground region 관련 event. |
| `robot_along_m`, `robot_offset_m` | Event 시점 robot의 corridor-relative 위치. |
| `target_along_m`, `target_offset_m` | Event target의 corridor-relative 위치. |
| `start_time_s`, `end_time_s`, `duration_s` | Near-miss, penalty region, stuck 등 구간 event. |
| `min_distance_m` | Near-miss 최단 거리. |
| `threshold_m` | 거리 기반 판단 기준. |
| `distance_to_goal_m` | Goal 접근 판단 시 목표까지 남은 거리. |
| `goal_threshold_m` | Goal 도착 인정 거리. |
| `failure_type` | DeliveryBot simulation failure 또는 pathfind fail 세부 유형. |
| `speed_kmh` | Event 시점 robot 속도. |
| `roll_degree`, `pitch_degree`, `threshold_degree` | 전복 판단 각도. |
| `path_index`, `path_length` | Repath 또는 PathfindFail 시점 path 상태. |
| `blocked_corridor_cell_count` | Repath 판단 시 막힌 corridor cell 수. |
| `dynamic_blocked_cell_count` | Repath 판단 시 동적으로 막힌 cell 수. |
| `error_code`, `error_message` | `PolicyDecisionError` 세부 오류. |

## Join Rules

- `action_sequence`로 `actions.jsonl.sequence`를 참조한다.
- Setup/evaluation event처럼 연결할 action이 없으면 `action_sequence: null`을 사용한다.
- `trace.jsonl`과의 기본 join key는 `run_time_seconds`다.
- Action 원본, LiDAR ray 원본, 전체 world state는 복사하지 않는다.

## Terminal Event

- Episode 종료 원인이 된 사건은 반드시 기록한다.
- 가능하면 `result.json.summary.terminal_event_index`가 종료 원인 event를 참조한다.
- Terminal result 판단은 `result.json.summary`를 기준으로 한다.
