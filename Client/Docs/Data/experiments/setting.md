# Experiment Setting

경로:

```text
experiments/<Experiment>/setting.json
```

schema:

```json
"experiment_setting"
```

## Root

```json
{
  "schema": "experiment_setting",
  "version": 1,
  "experiment_id": "exp_pinch_policy_a",
  "display_name": "Pinch Policy A",
  "sampling": {},
  "runtime": {},
  "evaluation": {}
}
```

| 필드 | 합의 |
| --- | --- |
| `schema` | `experiment_setting` |
| `version` | schema version. validator/compiler는 최신 1개 버전만 지원하므로 값이 다르면 컴파일 에러 |
| `experiment_id` | 실험 식별자 |
| `display_name` | 선택 UI에 표시할 이름 |
| `sampling` | sample 생성 조건 |
| `runtime` | 실행 조건 |
| `evaluation` | 평가 threshold |

## sampling

| 필드 | 합의 |
| --- | --- |
| `scenario_template_ref` | 사용할 `templates/scenarios/*.template.json` |
| `profile_template_ref` | 복사할 `templates/profiles/*.json` |
| `base_seed` | sample seed 생성 기준값 |
| `sample_count` | 생성할 scenario sample 수 |
| `generator_version` | 현재 sampler version과 일치해야 하는 생성기 버전 |

## runtime

| 필드 | 합의 |
| --- | --- |
| `map_id` | 실행할 UE level identifier |
| `fixed_fps` | 실험 실행 FPS |
| `time_scale` | 필요 시 실행 시간 배율 |
| `max_duration_s` | episode 최대 실행 시간. `0`이면 runtime 기본값 사용 |

## evaluation

| 필드 | 합의 |
| --- | --- |
| `goal_acceptance_radius_m` | 목표 도달 판정 반경 |
| `tip_over_angle_deg` | 전복 판정 각도 |
| `near_miss_distance_m` | 보행자 near-miss 거리 |

## 위치 결정

- sample 생성 수와 seed는 `setting.sampling` 소유다.
- 실행 요청의 `simulation_setup`은 `experiment_ref`로 experiment folder를 지정한다.
- simulator는 실행 전에 `experiments/<Experiment>/scenarios/<SampleId>.json`이 없으면 deterministic하게 materialize한다.
- `experiments/<Experiment>/profile.json`은 선택된 profile template을 복사하고 override한 실행 고정 입력이다.
- run 결과는 `experiments/<Experiment>/runs/<RunId>/` 아래에 저장한다.
