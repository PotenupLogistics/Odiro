# Project Setting

경로:

```text
<UserProject>/setting.json
```

schema:

```json
"project_setting"
```

## Root

```json
{
  "schema": "project_setting",
  "version": 1,
  "project_id": "pinch_policy_a",
  "sampling": {},
  "runtime": {},
  "evaluation": {}
}
```

| 필드 | 합의 |
| --- | --- |
| `schema` | `project_setting` |
| `version` | schema version |
| `project_id` | project 식별자 |
| `sampling` | episode scenario 생성 조건 |
| `runtime` | 실행 조건 |

## sampling

| 필드 | 합의 |
| --- | --- |
| `base_seed` | episode seed 생성 기준값 |
| `episode_count` | 한 run에서 생성할 episode 수 |
| `generator_version` | episode scenario 생성기 버전 기록 |

## runtime

| 필드 | 합의 |
| --- | --- |
| `fixed_fps` | project 실행 FPS |
| `time_scale` | 필요 시 실행 시간 배율 |

## evaluation

| 필드 | 합의 |
| --- | --- |
| `goal_acceptance_radius_m` | 목표 도달 판정 반경 |
| `tip_over_angle_deg` | 전복 판정 각도 |
| `near_miss_distance_m` | 보행자 near-miss 거리 |

## 위치 결정

- scenario의 랜덤 range/choices는 `scenario.json`에 남기고, seed와 episode 수는 `setting.sampling` 소유다.
- episode seed는 `base_seed`와 `episode_id`에서 deterministic하게 파생한다.
