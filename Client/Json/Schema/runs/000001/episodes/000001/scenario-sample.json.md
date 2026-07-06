# scenario.json

Episode 실행 전에 project `scenario.json`과 seed를 확정한 `scenario_sample` artifact다. 사용자가 직접 편집하는 파일이 아니다.

이 문서는 현재 환경 생성과 검증에서 의미 있는 semantic field만 설명한다. 호환성 field는 JSON에 빈 값으로 기록될 수 있지만 LLM authoring 계약으로 다루지 않는다.

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
| `params` | object | 예 | `scenario.json`의 range/choices 또는 runtime setting이 확정된 값. |
| `semantic` | object | 예 | Editor, simulation, replay 변환이 읽는 의미론 view. |

## scenario.params

- Range 또는 choices였던 항목만 기록한다.
- Key는 가능한 한 `scenario.json` 경로를 유지한다.
- 값은 확정된 scalar 또는 짧은 배열을 우선한다.
- `max_duration_s`는 `setting.runtime.max_duration_s`에서 파생되어 runtime timeout으로 쓰인다.
- `corridor.building_side[].width_m`와 `corridor.curb_side[].width_m`가 `0`이면 해당 side lane은 생성되지 않는다. 양수 값은 generated city rule을 활성화하지만 generated city 폭을 재계산하는 source of truth는 아니다.

## scenario.semantic

| 필드 | 타입 | 필수 | 설명 |
| --- | --- | --- | --- |
| `route_axis` | object | 예 | Corridor-relative along/offset 기준축. |
| `robot` | object | 예 | 확정된 로봇 시작/목적지 pose. |
| `layout` | array | 예 | Segment별 semantic lane 구성. |
| `static_obstacles` | array | 예 | Fixed placement에서 생성된 정적 장애물 의미론. |
| `clear_width_profile` | array | 예 | 중앙 walkway 기준 유효 통로 폭. |
| `summary` | object | 예 | Corridor walkway 중심의 sample 형상 요약. |

`layout`은 최종 world actor 목록이 아니다. Adapter와 CityBuildings materializer는 `layout` lane을 해석해 corridor GroundRegion, building-side expansion, road-side band, road/corner visual, building frontage visual을 파생한다.

CityBuildings BP 선택, bounds, semantic collision proxy는 현재 로드된 CityBlock catalog와 asset 상태에 의존한다.

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
| `lane` | string | 아니오 | 원본 anchor의 lane hint. Pose 계산 source of truth는 `along_m`, `offset_m`, `heading_deg`다. |
| `heading_deg` | number | 예 | Runtime yaw. |
| `source_anchor_type` | string | 예 | Project `scenario.json`의 원래 anchor type. |

### layout[]

| 필드 | 타입 | 필수 | 설명 |
| --- | --- | --- | --- |
| `along_range_m` | array | 예 | 이 layout이 적용되는 진행 구간. |
| `segment` | string | 예 | 원본 scenario segment id. |
| `lanes` | array | 예 | 해당 구간의 semantic lane 목록. |

### layout[].lanes[]

| 필드 | 타입 | 필수 | 설명 |
| --- | --- | --- | --- |
| `lane` | string | 예 | Lane 이름. |
| `offset_range_m` | array | 예 | Axis 기준 좌우 범위. |
| `surface` | string | 예 | Surface id. |
| `type` | string | 예 | `walkable`, `penalty`, or `blocked`. |

현재 생성되는 주요 lane id는 다음과 같다.

| lane | 의미 |
| --- | --- |
| `walkway` | 중앙 corridor walkway. |
| `building_walkway_extension` | Building-side walkable expansion seed. |
| `building_edge` | Building-side frontage/blocking seed. |
| `curb_edge` | Road-side curb seed. |
| `road_2lane` | Road-side 2-lane road seed. |

### static_obstacles[]

| 필드 | 타입 | 필수 | 설명 |
| --- | --- | --- | --- |
| `id` | string | 예 | 생성된 obstacle instance id. |
| `prop` | string | 예 | Prop id. |
| `perception_tag` | string | 예 | 감지/로그 조인용 tag. |
| `class` | string | 예 | `blocking` 또는 `traversable_cost`. |
| `sensor_profile` | string | 아니오 | `solid`, `thin`, `low_profile` 등 감지 특성. |
| `along_m` | number | 예 | Corridor axis 진행 거리. |
| `offset_m` | number | 예 | Corridor axis 기준 lateral offset. |
| `yaw_deg` | number | 예 | Obstacle yaw. |
| `footprint_m` | array | 예 | 분석용 footprint size. |
| `placed_by` | string | 예 | 이 obstacle을 만든 placement id. |
| `clear_width_remaining_m` | number | 예 | 중앙 walkway 기준 남은 유효 통로 폭. |

### clear_width_profile[]

| 필드 | 타입 | 필수 | 설명 |
| --- | --- | --- | --- |
| `along_range_m` | array | 예 | 통로 폭을 요약하는 진행 구간. |
| `clear_width_m` | number | 예 | 중앙 walkway 기준 유효 통로 폭. |
| `limited_by` | string | 예 | 통로 폭을 제한한 fixed obstacle id 또는 `layout`. |

### summary

| 필드 | 타입 | 필수 | 설명 |
| --- | --- | --- | --- |
| `global_min_clear_width_m` | number | 예 | 중앙 walkway 기준 sample 전체 최소 유효 통로 폭. |
| `min_clear_at_along_m` | number | 예 | 최소 통로 폭이 발생한 along 위치. |
| `total_length_m` | number | 예 | Route axis 전체 길이. |
| `encounter_in_min_clear_zone` | boolean | 예 | 현재 생성 경로에서는 보통 `false`. |

## validation

| 필드 | 타입 | 필수 | 설명 |
| --- | --- | --- | --- |
| `edited_by_user` | boolean | 예 | 생성 후 sample이 직접 수정되었는지 여부. |
| `diagnostics` | array | 예 | 생성기 warning, repair, error 기록. |

## 사용 규칙

- 이 파일은 generator output이다. LLM은 직접 작성하지 않는다.
- Error diagnostic이 있는 sample은 run 대상에서 제외한다.
- Runtime actor/world payload는 이 파일에 저장하지 않는다.
- Editor, simulation, replay는 같은 `scenario_sample` semantic을 adapter로 변환해 환경을 그린다.
- Generated city의 최종 visual block 배치는 `layout`과 현재 CityBlock catalog/asset에서 파생된다.
