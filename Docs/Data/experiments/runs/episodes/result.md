# Episode Result

경로:

```text
experiments/<Experiment>/runs/<RunId>/episodes/<SampleId>/result.json
```

schema:

```json
"episode_result_v1"
```

## 합의

- episode 하나의 최종 결과다.
- terminal result의 source of truth다.
- 기존 evaluation report의 summary, pipeline, metrics, event summary 성격을 계승한다.
- 기존 `events` 배열은 여기 넣지 않고 `events.jsonl`로 분리한다.
- episode 종료 원인이 된 사건은 `events.jsonl`에 반드시 남긴다.
- `severity`는 사용하지 않는다.

## Root

```json
{
  "schema": "episode_result_v1",
  "version": 1,
  "sample": {},
  "run": {},
  "summary": {},
  "metrics": {},
  "event_summary": {}
}
```

## sample

| 필드 | 합의 |
| --- | --- |
| `sample_id` | 실행한 sample id |
| `scenario_id` | scenario 표시 식별자 |
| `template_id` | 원본 template id |
| `template_hash` | 원본 template hash |
| `profile_hash` | profile hash |
| `setting_hash` | setting hash |
| `seed` | sample seed |

## run

| 필드 | 합의 |
| --- | --- |
| `run_id` | run 식별자 |
| `episode_id` | episode 식별자 |
| `policy_snapshot_hash` | opaque policy snapshot hash |

## summary

| 필드 | 합의 |
| --- | --- |
| `completed` | evaluation 종료 여부 |
| `success` | 목표 달성 여부 |
| `outcome` | `Success`, `Failure`, `Cancelled` 등 최종 결과 분류 |
| `terminal_reason` | 종료 직접 원인. terminal result의 source of truth |
| `terminal_event_index` | 종료 원인 event의 `events.jsonl.event_index`. 없으면 `null` |
| `duration_s` | episode 실행 시간 |

## metrics

| 필드 | 합의 |
| --- | --- |
| `goal_reached` | 보통 `0` 또는 `1` |
| `duration_s` | 최종 duration |
| `goal_distance_m` | 종료 시점 목표 거리 |
| `static_obstacle_collision_count` | 정적 장애물 충돌 횟수 |
| `blocked_region_collision_count` | blocked region 충돌 횟수 |
| `penalty_region_violation_count` | penalty region 위반 횟수 |
| `pedestrian_collision_count` | 보행자 충돌 횟수 |
| `near_miss_count` | near-miss 구간 수 |
| `near_miss_total_duration_s` | near-miss 누적 시간 |
| `near_miss_min_distance_m` | near-miss 최단 거리 |
| `robot_tip_over_count` | 전복 감지 횟수 |
| `delivery_bot_failure_type` | `RobotTipOver`, `PathFindingFailed`, `Stuck` 등 |
| `delivery_bot_failure_message` | 실패 메시지 |
| `delivery_bot_failure_xy_m` | 실패 위치. 단위 m |
| `delivery_bot_failure_time_s` | 실패 시간 |
| `delivery_bot_failure_speed_kmh` | 실패 시점 속도 |
| `policy_decision_error_count` | `PolicyDecisionError` 발생 횟수. v1에서는 `0` 또는 `1` |

## event_summary

| 필드 | 합의 |
| --- | --- |
| `total` | event 수 |
| `by_type` | `event_type`별 count |
| `by_source` | `source`별 count |
| `terminal_event_index` | `summary.terminal_event_index`와 같은 값. 없으면 `null` |

`event_summary`는 `events.jsonl`에서 집계한다. event 원본은 `result.json`에 복사하지 않는다.
