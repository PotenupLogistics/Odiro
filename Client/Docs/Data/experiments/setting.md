# Experiment Setting

경로:

```text
experiments/<Experiment>/setting.json
```

schema:

```json
"experiment_setting_v1"
```

## Root

```json
{
  "schema": "experiment_setting_v1",
  "version": 1,
  "experiment_id": "exp_pinch_policy_a",
  "sampling": {},
  "runtime": {},
  "evaluation": {}
}
```

| 필드 | 합의 |
| --- | --- |
| `schema` | `experiment_setting_v1` |
| `version` | schema version |
| `experiment_id` | 실험 식별자 |
| `sampling` | sample 생성 조건 |
| `runtime` | 실행 조건 |

## sampling

| 필드 | 합의 |
| --- | --- |
| `scenario_template_ref` | 사용할 `templates/scenarios/*.template.json` |
| `profile_template_ref` | 복사할 `templates/profiles/*.json` |
| `base_seed` | sample seed 생성 기준값 |
| `sample_count` | 생성할 scenario sample 수 |
| `generator_version` | sample 생성기 버전 기록 |

## runtime

| 필드 | 합의 |
| --- | --- |
| `fixed_fps` | 실험 실행 FPS |
| `time_scale` | 필요 시 실행 시간 배율 |

## evaluation

| 필드 | 합의 |
| --- | --- |
| `goal_acceptance_radius_m` | 목표 도달 판정 반경 |
| `tip_over_angle_deg` | 전복 판정 각도 |
| `near_miss_distance_m` | 보행자 near-miss 거리 |

## 위치 결정

- sample 생성 수와 seed는 `setting.sampling` 소유다.