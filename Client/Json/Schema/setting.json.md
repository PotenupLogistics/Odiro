# setting.json

Project 단위 simulation 설정 파일이다. Run 시작 시 snapshot으로 복사된다.

## 경로

```text
<UserProject>/setting.json
runs/<RunId>/snapshot/setting.json
```

## schema

```json
"project_setting"
```

## Root Fields

| 필드 | 타입 | 필수 | 설명 |
| --- | --- | --- | --- |
| `schema` | string | 예 | 고정값 `project_setting`. |
| `version` | number | 예 | 고정값 `1`. |
| `project_id` | string | 예 | Project 식별자. |
| `sampling` | object | 예 | Episode 수, seed 기준값, generator version. |
| `runtime` | object | 예 | Simulator 실행 조건. |
| `evaluation` | object | 예 | Episode result/event 판정 기준. |

## sampling

| 필드 | 타입 | 필수 | 설명 |
| --- | --- | --- | --- |
| `base_seed` | number | 예 | Episode index `0`의 seed 기준값. |
| `episode_count` | number | 예 | 한 run에서 생성할 episode 수. |
| `generator_version` | string | 예 | Scenario sample generator 구현/설정 버전. |

Seed 계산:

```text
episode_seed = sampling.base_seed + episode_index
```

`episode_index`는 0-based이고 `EpisodeId`는 1-based 6자리 decimal string이다.

## runtime

| 필드 | 타입 | 필수 | 설명 |
| --- | --- | --- | --- |
| `map_id` | string | 예 | Simulator가 열 map 이름 또는 package id. |
| `fixed_fps` | number | 예 | Simulation FPS. |
| `time_scale` | number | 예 | 실행 시간 배율. 기본 실시간 배율은 `1`. |
| `max_duration_s` | number | 예 | Episode timeout. `0`이면 timeout 없음. |

## evaluation

| 필드 | 타입 | 필수 | 설명 |
| --- | --- | --- | --- |
| `goal_acceptance_radius_m` | number | 예 | 목표 도달 판정 반경. 단위 m. |
| `tip_over_angle_deg` | number | 예 | Robot 전복 판정 각도. 단위 degree. |
| `near_miss_distance_m` | number | 예 | 보행자 near-miss 거리. 단위 m. |

## 소유 경계

- Random range/choices는 `scenario.json`이 소유한다.
- Seed 기준값, episode 수, generator version은 `setting.sampling`이 소유한다.
- Robot 크기와 capability 값은 `profile.json`이 소유한다.
- Policy code/config는 `<UserProject>/policy/`가 소유한다.
