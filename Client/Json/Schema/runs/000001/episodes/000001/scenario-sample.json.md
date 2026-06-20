# scenario.json

Episode 실행 전에 project `scenario.json`과 seed를 확정한 `scenario_sample` artifact다. 사용자가 직접 편집하는 파일이 아니다.

## 경로

```text
runs/<RunId>/episodes/<EpisodeId>/scenario.json
```

## schema

```json
"scenario_sample"
```

## Root Fields

| 필드 | 타입 | 필수 | 설명 |
| --- | --- | --- | --- |
| `schema` | string | 예 | 고정값 `scenario_sample`. |
| `version` | number | 예 | 고정값 `1`. |
| `sample` | object | 예 | Episode/sample 식별자와 source 계보. |
| `scenario` | object | 예 | Seed로 확정된 parameter와 semantic view. |
| `validation` | object | 예 | 생성기 진단과 수동 수정 여부. |

## sample

| 필드 | 타입 | 필수 | 설명 |
| --- | --- | --- | --- |
| `sample_id` | string | 예 | Episode id와 같은 6자리 decimal string. |
| `scenario_id` | string | 예 | 표시/로그 조인용 scenario id. |
| `source` | object | 예 | 입력 snapshot과 seed 계보. |

## sample.source

| 필드 | 타입 | 필수 | 설명 |
| --- | --- | --- | --- |
| `template_ref` | string | 예 | Run snapshot `scenario.json` 경로. v1 필드명은 호환성 때문에 `template_ref`를 유지한다. |
| `template_hash` | string | 예 | Run snapshot `scenario.json` hash. |
| `profile_ref` | string | 예 | Run snapshot `profile.json` 경로. |
| `profile_hash` | string | 예 | Run snapshot `profile.json` hash. |
| `setting_ref` | string | 예 | Run snapshot `setting.json` 경로. |
| `setting_hash` | string | 예 | Run snapshot `setting.json` hash. |
| `seed` | number | 예 | 이 episode를 확정한 seed. |
| `generator_version` | string | 예 | Scenario sample generator 구현/설정 버전. |

## scenario

| 필드 | 타입 | 필수 | 설명 |
| --- | --- | --- | --- |
| `params` | object | 예 | `scenario.json`의 range/choices가 seed로 확정된 값. |
| `semantic` | object | 예 | LLM, 분석, preview, runtime 변환이 읽는 의미론 view. |

## scenario.params

- Range 또는 choices였던 항목만 기록한다.
- Key는 가능한 한 `scenario.json` 경로를 유지한다.
- 값은 확정된 scalar 또는 짧은 배열을 우선한다.
- `max_duration_s`는 `setting.runtime.max_duration_s`에서 파생되어 runtime timeout으로 쓰인다.

## scenario.semantic

| 필드 | 타입 | 필수 | 설명 |
| --- | --- | --- | --- |
| `route_axis` | object | 예 | Corridor-relative along/offset 기준축. |
| `robot` | object | 예 | 확정된 로봇 시작/목적지 pose. |
| `layout` | array | 예 | Segment별 lane/surface 구성. |
| `static_obstacles` | array | 예 | 생성된 정적 장애물 의미론. |
| `pedestrians` | array | 예 | 생성된 보행자 의미론. |
| `clear_width_profile` | array | 예 | 진행 구간별 유효 통로 폭. |
| `summary` | object | 예 | Sample 형상 요약. |

### route_axis

| 필드 | 타입 | 필수 | 설명 |
| --- | --- | --- | --- |
| `type` | string | 예 | 고정값 `polyline` in v1. |
| `origin_xy_m` | array | 예 | Local/world 변환 기준점. |
| `heading_deg` | number | 예 | 기준 heading. |
| `points_m` | array | 예 | Corridor axis polyline points. |
| `length_m` | number | 예 | Axis 전체 길이. |

### robot.start / robot.goal

| 필드 | 타입 | 필수 | 설명 |
| --- | --- | --- | --- |
| `segment` | string | 예 | 확정 pose가 위치한 segment id. |
| `along_m` | number | 예 | Corridor axis 진행 거리. |
| `offset_m` | number | 예 | Corridor axis 기준 lateral offset. |
| `lane` | string | 예 | Pose를 확정할 때 사용한 lane hint. |
| `heading_deg` | number | start 권장 | Runtime yaw. |
| `source_anchor_type` | string | 예 | Project `scenario.json`의 원래 anchor type. |

### layout[]

| 필드 | 타입 | 필수 | 설명 |
| --- | --- | --- | --- |
| `along_range_m` | array | 예 | 이 layout이 적용되는 진행 구간. |
| `segment` | string | 예 | 원본 scenario segment id. |
| `lanes` | array | 예 | 해당 구간의 lane 목록. |

### layout[].lanes[]

| 필드 | 타입 | 필수 | 설명 |
| --- | --- | --- | --- |
| `lane` | string | 예 | Lane 이름. |
| `offset_range_m` | array | 예 | Axis 기준 좌우 범위. |
| `surface` | string | 예 | Surface id. |
| `type` | string | 예 | `walkable`, `penalty`, or `blocked`. |

### static_obstacles[]

| 필드 | 타입 | 필수 | 설명 |
| --- | --- | --- | --- |
| `id` | string | 예 | 생성된 obstacle instance id. |
| `prop` | string | 예 | Prop catalog id. |
| `perception_tag` | string | 예 | 감지/로그 조인용 tag. |
| `class` | string | 예 | `blocking` 또는 `traversable_cost`. |
| `sensor_profile` | string | 아니오 | `solid`, `thin`, `low_profile` 등 감지 특성. |
| `along_m` | number | 예 | Corridor axis 진행 거리. |
| `offset_m` | number | 예 | Corridor axis 기준 lateral offset. |
| `yaw_deg` | number | 예 | Obstacle yaw. |
| `footprint_m` | array | 예 | 분석용 footprint size. |
| `placed_by` | string | 예 | 이 obstacle을 만든 placement id. |
| `clear_width_remaining_m` | number | 예 | 해당 위치의 남은 유효 통로 폭. |

### pedestrians[]

| 필드 | 타입 | 필수 | 설명 |
| --- | --- | --- | --- |
| `id` | string | 예 | 생성된 pedestrian instance id. |
| `role` | string | 예 | `encounter` 또는 `background`. |
| `placed_by` | string | encounter면 예 | 원본 encounter id. |
| `type` | string | encounter면 예 | Encounter type. |
| `persona` | string | 예 | 원본 persona id. |
| `behavior` | object | 예 | Persona와 overrides가 확정된 행동 벡터. |
| `speed_mps` | number | 예 | 확정 보행 속도. |
| `baseline` | object | 예 | 로봇이 없을 때의 기준 보행 동선. |
| `pedestrian_scenario_hash` | string | 예 | Plan과 behavior 지문. |

### behavior

| 필드 | 타입 | 필수 | 설명 |
| --- | --- | --- | --- |
| `cooperation` | number | 예 | 로봇에게 양보하는 성향. |
| `evasiveness` | number | 예 | 옆으로 피하려는 성향. |
| `personal_space_m` | number | 예 | 유지하려는 개인 공간. |
| `awareness_horizon_s` | number | 예 | 충돌/접근 예측 시간 범위. |
| `max_yield_wait_s` | number | 예 | 양보하며 기다릴 수 있는 최대 시간. |
| `sidestep_distance_m` | number | 예 | 옆으로 비켜서는 거리. |

### baseline

| 필드 | 타입 | 필수 | 설명 |
| --- | --- | --- | --- |
| `start_segment` | string | 예 | 기준 동선 시작 segment. |
| `goal_segment` | string | 예 | 기준 동선 목적지 segment. |
| `start_along_m` | number | 예 | 시작 along 위치. |
| `start_offset_m` | number | 예 | 시작 lateral offset. |
| `goal_along_m` | number | 예 | 목적지 along 위치. |
| `goal_offset_m` | number | 예 | 목적지 lateral offset. |
| `points_m` | array | 아니오 | Preview용 baseline polyline. |

### clear_width_profile[]

| 필드 | 타입 | 필수 | 설명 |
| --- | --- | --- | --- |
| `along_range_m` | array | 예 | 통로 폭을 요약하는 진행 구간. |
| `clear_width_m` | number | 예 | 해당 구간의 유효 통로 폭. |
| `limited_by` | string | 예 | 통로 폭을 제한한 obstacle 또는 region id. |

### summary

| 필드 | 타입 | 필수 | 설명 |
| --- | --- | --- | --- |
| `global_min_clear_width_m` | number | 예 | Sample 전체 최소 유효 통로 폭. |
| `min_clear_at_along_m` | number | 예 | 최소 통로 폭이 발생한 along 위치. |
| `total_length_m` | number | 예 | Route axis 전체 길이. |
| `encounter_in_min_clear_zone` | boolean | 예 | 주요 encounter가 최소 통로 폭 구간과 겹치는지 여부. |

## validation

| 필드 | 타입 | 필수 | 설명 |
| --- | --- | --- | --- |
| `edited_by_user` | boolean | 예 | 생성 후 sample이 직접 수정되었는지 여부. |
| `diagnostics` | array | 예 | 생성기 warning, repair, error 기록. |

## 사용 규칙

- 재현성 기준은 `sample.source`, `scenario.params`, `scenario.semantic`이다.
- `edited_by_user: true`인 sample은 순수 seed 생성물과 구분한다.
- Error diagnostic이 있는 sample은 run 대상에서 제외한다.
- Runtime actor/world payload는 이 파일에 저장하지 않는다.
