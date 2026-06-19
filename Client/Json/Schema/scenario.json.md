# scenario.json

사용자, editor, LLM이 함께 편집하는 project scenario 파일이다.

## 경로

```text
<UserProject>/scenario.json
runs/<RunId>/snapshot/scenario.json
```

## schema

```json
"scenario"
```

## Root Fields

| 필드 | 타입 | 필수 | 설명 |
| --- | --- | --- | --- |
| `schema` | string | 예 | 고정값 `scenario`. |
| `version` | number | 예 | 고정값 `1`. |
| `scenario_id` | string | 예 | Scenario 식별자. |
| `intent` | string | 예 | 검증하려는 상황 또는 가설. |
| `corridor` | object | 예 | 공간 skeleton과 lane/surface 규칙. |
| `obstacles` | object | 예 | 정적 장애물 배치 규칙. |
| `pedestrians` | object | 예 | 배경 보행자와 encounter 규칙. |
| `robot` | object | 예 | Robot 시작/목적지 anchor. |

## Random Values

Numeric field는 고정값 또는 range를 쓸 수 있다.

```json
"walkway_width_m": 3.0
```

```json
"walkway_width_m": { "min": 2.5, "max": 4.0 }
```

String field는 고정값 또는 choices를 쓸 수 있다.

```json
"replaced_by": { "choices": ["grass", "road"] }
```

Range와 choices는 `runs/<RunId>/episodes/<EpisodeId>/scenario.json` 생성 시 seed로 확정된다.

## corridor

| 필드 | 타입 | 필수 | 설명 |
| --- | --- | --- | --- |
| `axis` | object | 예 | Corridor route axis. |
| `walkway_width_m` | number or range | 예 | 주 보행로 폭. 단위 m. |
| `building_side` | array | 예 | 보행로 건물측 lane/surface 폭 규칙. |
| `curb_side` | array | 예 | 보행로 연석측 lane/surface 폭 규칙. |
| `segments` | array | 예 | Corridor axis 상의 의미 segment 목록. |

### corridor.axis

| 필드 | 타입 | 필수 | 설명 |
| --- | --- | --- | --- |
| `type` | string | 예 | v1 고정값 `polyline`. |
| `points_m` | array | 예 | 최소 두 개의 local XY point. 단위 m. |

### corridor.building_side[] / corridor.curb_side[]

| 필드 | 타입 | 필수 | 설명 |
| --- | --- | --- | --- |
| `surface` | string | 예 | `environment-catalog.md`의 surface id. |
| `width_m` | number or range | 예 | Lane 폭. 단위 m. |

### corridor.segments[]

| 필드 | 타입 | 필수 | 설명 |
| --- | --- | --- | --- |
| `id` | string | 예 | Obstacle, pedestrian, robot anchor가 참조하는 unique segment id. |
| `type` | string | 예 | `straight`, `narrowing`, `crosswalk`, `entrance`. |
| `along_range_m` | array | 예 | `[start_m, end_m]` 형태의 corridor axis 진행 구간. |
| `replaced_by` | string or choices | 아니오 | 이 segment의 surface를 대체할 surface id. |

## obstacles

| 필드 | 타입 | 필수 | 설명 |
| --- | --- | --- | --- |
| `min_clear_width_m` | number or range | 예 | 생성기가 유지해야 하는 최소 유효 통로 폭. |
| `placements` | array | 예 | 정적 장애물 생성 규칙. |

### obstacles.placements[]

| 필드 | 타입 | 필수 | 설명 |
| --- | --- | --- | --- |
| `id` | string | 예 | Unique placement id. |
| `kind` | string | 예 | `fixed`, `pattern`, `scatter`. |
| `prop` | string | `fixed`, `pattern`이면 예 | `environment-catalog.md`의 prop id. |
| `pattern` | string | `pattern`이면 예 | `gate`, `line`, `cluster` 같은 pattern id. |
| `at` | object | `fixed`, `pattern`이면 예 | Corridor-local placement anchor. |
| `zone` | object | `scatter`이면 예 | Scatter를 허용할 segment/lane filter. |
| `palette` | object | `scatter`이면 예 | Scatter 후보 prop category/class filter. |
| `count` | number or range | 아니오 | Pattern 또는 generated obstacle 수. |
| `spacing_m` | number or range | 아니오 | Pattern 간격. 단위 m. |
| `gap_width_m` | number or range | 아니오 | Pattern gap 폭. 단위 m. |
| `density_per_10m` | number or range | `scatter`이면 예 | 10m당 scatter 밀도. |
| `yaw_deg` | number or range | 아니오 | Obstacle local yaw. 단위 degree. |
| `allow_blocking` | boolean | 아니오 | `min_clear_width_m` 위반을 의도할 때만 `true`. |

### placement.at

| 필드 | 타입 | 필수 | 설명 |
| --- | --- | --- | --- |
| `segment` | string | 예 | Placement anchor가 속한 segment id. |
| `along_m` | number or range | 예 | Corridor axis 진행 거리. 단위 m. |
| `offset_m` | number or range | 예 | Corridor axis 기준 lateral offset. 단위 m. |
| `lane` | string | 아니오 | `walkway`, `building_edge`, `center`, `curb_edge`, `across` 등 lane hint. |

### placement.zone

| 필드 | 타입 | 필수 | 설명 |
| --- | --- | --- | --- |
| `segments` | array | 예 | Scatter placement가 사용할 segment id 목록. |
| `lanes` | array | 예 | Scatter placement가 사용할 lane id 목록. |

### placement.palette

| 필드 | 타입 | 필수 | 설명 |
| --- | --- | --- | --- |
| `categories` | array | 아니오 | `environment-catalog.md`의 prop category id 목록. |
| `classes` | array | 아니오 | `environment-catalog.md`의 prop class id 목록. |

`scatter`는 `palette.categories` 또는 `palette.classes` 중 하나 이상을 가져야 한다.

## pedestrians

| 필드 | 타입 | 필수 | 설명 |
| --- | --- | --- | --- |
| `background` | object | 예 | 배경 보행자 생성 규칙. |
| `encounters` | array | 예 | 설계된 pedestrian encounter 목록. |

### pedestrians.background

| 필드 | 타입 | 필수 | 설명 |
| --- | --- | --- | --- |
| `count` | number or range | 예 | 배경 보행자 수. |
| `speed_mps` | number or range | 아니오 | 배경 보행자 속도. 단위 m/s. |
| `spawn_zone.segments` | array | 아니오 | 배경 보행자 spawn segment 제한. |

### pedestrians.encounters[]

| 필드 | 타입 | 필수 | 설명 |
| --- | --- | --- | --- |
| `id` | string | 예 | Unique encounter id. |
| `type` | string | 예 | `environment-catalog.md`의 encounter type. |
| `at` | string | 예 | Encounter가 발생할 segment id. |
| `persona` | string | 예 | `environment-catalog.md`의 persona id. |
| `meet_offset_m` | number or range | `oncoming_pass`이면 권장 | 의도한 만남 지점에서의 offset. 단위 m. |
| `speed_mps` | number or range | `overtake`이면 권장 | Encounter pedestrian 속도. 단위 m/s. |
| `trigger_distance_m` | number or range | `cross_path`이면 권장 | Crossing behavior를 trigger할 robot 거리. 단위 m. |
| `from` | string | `cross_path`이면 권장 | Pedestrian이 진입하는 방향 또는 lane side. |
| `size` | number or range | `standing_group`이면 권장 | 정지 group 크기. |
| `blocked_width_ratio` | number or range | `standing_group`이면 권장 | Group이 점유하는 walkway 폭 비율. |
| `overrides` | object | 아니오 | Persona behavior 일부 override. |

### encounter.overrides

| 필드 | 타입 | 필수 | 설명 |
| --- | --- | --- | --- |
| `cooperation` | number or range | 아니오 | Robot에게 양보하는 성향. |
| `evasiveness` | number or range | 아니오 | 옆으로 피하려는 성향. |
| `personal_space_m` | number or range | 아니오 | 유지하려는 개인 공간. 단위 m. |
| `awareness_horizon_s` | number or range | 아니오 | 충돌/접근 예측 시간 범위. 단위 s. |
| `max_yield_wait_s` | number or range | 아니오 | 양보하며 기다릴 수 있는 최대 시간. 단위 s. |
| `sidestep_distance_m` | number or range | 아니오 | 옆으로 비켜서는 거리. 단위 m. |

## robot

| 필드 | 타입 | 필수 | 설명 |
| --- | --- | --- | --- |
| `start` | object | 예 | Robot 시작 anchor. |
| `goal` | object | 예 | Robot 목적지 anchor. |

### robot.start / robot.goal

| 필드 | 타입 | 필수 | 설명 |
| --- | --- | --- | --- |
| `type` | string | 예 | `entry`, `exit`, `corridor_pose`. |
| `segment` | string | `corridor_pose`이면 예 | Pose가 위치한 segment id. |
| `along_m` | number | `corridor_pose`이면 예 | Corridor axis 진행 거리. 단위 m. |
| `offset_m` | number | `corridor_pose`이면 예 | Corridor axis 기준 lateral offset. 단위 m. |
| `lane` | string | 아니오 | Editor/generator lane hint. |
| `heading` | string | 아니오 | `forward`, `backward`, `auto`. |

## 검증 규칙

- 한 project에는 편집 가능한 `scenario.json` 하나만 둔다.
- `corridor.axis.points_m`은 최소 두 점을 가져야 한다.
- `corridor.segments[].id`, `obstacles.placements[].id`, `pedestrians.encounters[].id`는 각각 unique해야 한다.
- Placement, encounter, robot anchor가 참조하는 segment는 존재해야 한다.
- `corridor_pose.along_m`은 참조 segment의 `along_range_m` 안에 있어야 한다.
- Surface, prop, persona, encounter type, prop category, prop class는 `environment-catalog.md`의 값만 사용한다.
- `allow_blocking` 없이 `min_clear_width_m` 계약을 깨면 error다.
- Unreal actor/world payload는 저장하지 않는다. Preview/run 시점에 파생한다.
