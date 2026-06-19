# result.json

Episode 하나의 최종 결과 파일이다. Terminal result의 source of truth다.

## 경로

```text
runs/<RunId>/episodes/<EpisodeId>/result.json
```

## schema

```json
"episode_result"
```

## Root Fields

| 필드 | 타입 | 필수 | 설명 |
| --- | --- | --- | --- |
| `schema` | string | 예 | 고정값 `episode_result`. |
| `version` | number | 예 | 고정값 `1`. |
| `episode` | object | 예 | Episode id, hashes, seed. |
| `run` | object | 예 | Run id와 policy snapshot hash. |
| `summary` | object | 예 | 최종 결과 기준. |
| `artifacts` | object | 예 | 같은 episode의 주요 근거 파일 경로. |
| `metrics` | object | 예 | Evaluation metrics. |
| `event_summary` | object | 예 | `events.jsonl` 집계. |

## episode

| 필드 | 타입 | 필수 | 설명 |
| --- | --- | --- | --- |
| `episode_id` | string | 예 | 6자리 decimal episode id. |
| `scenario_id` | string | 예 | Scenario 표시/조인 식별자. |
| `scenario_hash` | string | 예 | Episode `scenario_sample` hash. |
| `scenario_source_hash` | string | 예 | Run snapshot `scenario.json` hash. |
| `profile_hash` | string | 예 | Run snapshot `profile.json` hash. |
| `setting_hash` | string | 예 | Run snapshot `setting.json` hash. |
| `seed` | number | 예 | Episode seed. |

## run

| 필드 | 타입 | 필수 | 설명 |
| --- | --- | --- | --- |
| `run_id` | string | 예 | 6자리 decimal run id. |
| `policy_snapshot_hash` | string | 예 | 실행 시점 policy snapshot hash. |

## artifacts

경로는 run directory 기준 상대 경로다.

| 필드 | 타입 | 필수 | 설명 |
| --- | --- | --- | --- |
| `scenario_path` | string | 예 | Episode `scenario.json` 경로. |
| `result_path` | string | 예 | 이 `result.json` 경로. |
| `events_path` | string | 예 | Episode `events.jsonl` 경로. |

## summary

| 필드 | 타입 | 필수 | 설명 |
| --- | --- | --- | --- |
| `completed` | boolean | 예 | Evaluation 종료 여부. |
| `evaluation_completed` | boolean | 호환 | Evaluation 종료 여부를 나타내는 runtime writer field. |
| `success` | boolean | 예 | 목표 달성 여부. |
| `outcome` | string | 예 | `Success`, `Failure`, `Cancelled` 등 최종 결과 분류. |
| `terminal_reason` | string | 예 | Episode 종료 직접 원인. |
| `terminal_event_index` | number or null | 권장 | 종료 원인 event의 `events.jsonl.event_index`. |
| `duration_s` | number | 예 | Episode 실행 시간. |

## metrics

| 필드 | 타입 | 필수 | 설명 |
| --- | --- | --- | --- |
| `goal_reached` | number | 권장 | 목표 도달 여부. 보통 `0` 또는 `1`. |
| `duration_s` | number | 권장 | 최종 duration. |
| `goal_distance_m` | number | 권장 | 종료 시점 목표 거리. |
| `static_obstacle_collision_count` | number | 권장 | 정적 장애물 충돌 횟수. |
| `blocked_region_collision_count` | number | 권장 | Blocked region 충돌 횟수. |
| `penalty_region_violation_count` | number | 권장 | Penalty region 위반 횟수. |
| `pedestrian_collision_count` | number | 권장 | 보행자 충돌 횟수. |
| `near_miss_count` | number | 권장 | Near-miss 구간 수. |
| `near_miss_total_duration_s` | number | 권장 | Near-miss 누적 시간. |
| `near_miss_min_distance_m` | number | 권장 | Near-miss 최단 거리. |
| `robot_tip_over_count` | number | 권장 | 전복 감지 횟수. |
| `delivery_bot_failure_type` | string | 실패 시 권장 | DeliveryBot simulation failure 유형. |
| `delivery_bot_failure_message` | string | 실패 시 권장 | Failure message. |
| `delivery_bot_failure_xy_m` | array | 실패 시 권장 | Failure 위치. |
| `delivery_bot_failure_time_s` | number | 실패 시 권장 | Failure 시간. |
| `delivery_bot_failure_speed_kmh` | number | 실패 시 권장 | Failure 시점 속도. |
| `policy_decision_error_count` | number | 권장 | `PolicyDecisionError` 발생 횟수. |

## event_summary

| 필드 | 타입 | 필수 | 설명 |
| --- | --- | --- | --- |
| `total` | number | 권장 | Event 수. |
| `event_count` | number | 호환 | Event 수를 나타내는 runtime writer field. |
| `by_type` | object | 예 | `event_type`별 count. |
| `by_source` | object | 권장 | `source`별 count. |
| `terminal_event_index` | number or null | 권장 | `summary.terminal_event_index`와 같은 값. |

## 사용 규칙

- Terminal result 판단은 `result.json.summary`를 기준으로 한다.
- 종료 원인 event는 `events.jsonl`에도 기록한다.
- Event 원본 배열은 `result.json`에 복사하지 않는다.
- `preview.png`와 `captures/`는 이 result artifact 계약에 포함하지 않는다.
