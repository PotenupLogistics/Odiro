# Environment Catalog

LLM이 project `scenario.json`을 작성할 때 사용할 수 있는 환경 어휘다.

## 사용 위치

| Scenario field | 사용하는 어휘 |
| --- | --- |
| `corridor.building_side[].surface` | Surface id |
| `corridor.curb_side[].surface` | Surface id |
| `corridor.segments[].replaced_by` | Surface id |
| `obstacles.placements[].prop` | Prop id |
| `obstacles.placements[].palette.categories` | Prop category |
| `obstacles.placements[].palette.classes` | Prop class |
| `pedestrians.encounters[].type` | Encounter type |
| `pedestrians.encounters[].persona` | Persona id |
| `pedestrians.encounters[].overrides` | Behavior override field |

## 작성 규칙

- 이 문서에 있는 surface, prop, category, class, persona, encounter type만 사용한다.
- 모르는 id를 임의로 만들지 않는다.
- Catalog 정의를 `scenario.json` 안에 복사하지 않는다.
- `scenario.json`에는 배치, 생성, 선택, 범위만 기록한다.
- 거리와 크기는 meter 단위다.

## Surface

Surface는 corridor lane이나 segment surface의 의미 id다. 현재는 LLM 작성 테스트를 단순화하기 위해 `walkway`, `road`, `building`만 사용한다.

| surface_id | Lane type | 의미 | 사용 기준 |
| --- | --- | --- | --- |
| `walkway` | `walkable` | 기본 보행로 | Robot이 이동할 주 통로. |
| `road` | `penalty` | 차도 | 보도-차도 경계, 횡단 상황, 위험 영역. |
| `building` | `blocked` | 건물 또는 벽 영역 | 통과 불가 영역. |

## Prop Classes

Prop class는 `scenario_sample.scenario.semantic.static_obstacles[].class`와 scatter filter에 쓰는 의미 분류다.

| class | 의미 |
| --- | --- |
| `blocking` | Robot이 통과할 수 없는 장애물. |
| `traversable_cost` | 통과는 가능하지만 비용이나 주의가 필요한 물체. |

현재 static obstacle prop Data Asset은 class를 별도 필드로 저장하지 않는다. Generator가 만든 fixed obstacle은 기본적으로 `blocking` class로 확정된다.

## Sensor Profiles

Sensor profile은 `scenario_sample.scenario.semantic.static_obstacles[].sensor_profile`에 쓰는 감지 특성 주석이다.

| sensor_profile | 의미 |
| --- | --- |
| `solid` | 넓고 안정적으로 감지되는 물체. |
| `thin` | 얇아서 일부 ray만 맞을 수 있는 물체. |
| `low_profile` | 낮아서 감지나 회피가 어려울 수 있는 물체. |

현재 static obstacle prop Data Asset은 sensor profile을 별도 필드로 저장하지 않는다. Generator가 만든 fixed obstacle은 기본적으로 `solid` profile로 확정된다.

## Props

Prop id는 `obstacles.placements[].prop`에서 직접 사용할 수 있는 정적 장애물 id다.

| prop_id | 표시명 | 권장 class | 권장 sensor_profile | 사용 기준 |
| --- | --- | --- | --- | --- |
| `obstacle.bin` | Bin | `blocking` | `solid` | 보도 가장자리 휴지통. |
| `obstacle.box_01` | Box 01 | `blocking` | `solid` | 배달 박스나 적재물. |
| `obstacle.box_02` | Box 02 | `blocking` | `solid` | 배달 박스나 적재물 변형. |
| `obstacle.box_03` | Box 03 | `blocking` | `solid` | 배달 박스나 적재물 변형. |
| `obstacle.fire_hydrant` | Fire Hydrant | `blocking` | `solid` | 고정 설비물. |
| `obstacle.mailbox` | Mailbox | `blocking` | `solid` | 고정 설비물 또는 보도 가장자리 장애물. |
| `obstacle.manhole_01` | Manhole 01 | `traversable_cost` | `low_profile` | 낮은 지면 물체. |
| `obstacle.manhole_02` | Manhole 02 | `traversable_cost` | `low_profile` | 낮은 지면 물체 변형. |
| `obstacle.manhole_03` | Manhole 03 | `traversable_cost` | `low_profile` | 낮은 지면 물체 변형. |
| `obstacle.manhole_04` | Manhole 04 | `traversable_cost` | `low_profile` | 낮은 지면 물체 변형. |
| `obstacle.road_barrier_01` | Road Barrier 01 | `blocking` | `solid` | 공사 구간, gate, 협폭 유도. |
| `obstacle.road_barrier_02` | Road Barrier 02 | `blocking` | `solid` | 공사 구간, gate, 협폭 유도 변형. |
| `obstacle.road_cone_01` | Road Cone 01 | `blocking` | `thin` | 임시 통제, 협폭 표시. |
| `obstacle.road_cone_02` | Road Cone 02 | `blocking` | `thin` | 임시 통제, 협폭 표시 변형. |
| `obstacle.street_bank` | Street Bank | `blocking` | `solid` | 보도 가장자리 거리 시설물. |
| `obstacle.trash_bin` | Trash Bin | `blocking` | `solid` | 보도 가장자리 휴지통. |

## Prop Bounding Boxes

LLM이 `obstacles.placements[]`의 `at.offset_m`, `spacing_m`, `gap_width_m`, `count`를 잡을 때 참고하는 prop별 대략 크기다. `bbox_m`는 full bounding box 크기이며 `X x Y x Z` meter 순서로 적는다. `footprint_m`는 지면에서 차지하는 대략적인 `X x Y` 크기다.

| prop_id | 표시명 | bbox_m (X x Y x Z) | footprint_m (X x Y) | 배치 참고 |
| --- | --- | --- | --- | --- |
| `obstacle.bin` | Bin | `0.90 x 0.90 x 1.80` | `0.90 x 0.90` | 보행로 가장자리나 건물/연석 측에 단독 배치하기 적합하다. |
| `obstacle.box_01` | Box 01 | `0.90 x 0.90 x 0.90` | `0.90 x 0.90` | 배달 물품, 적재물, 작은 고정 장애물로 쓴다. |
| `obstacle.box_02` | Box 02 | `0.90 x 0.90 x 0.90` | `0.90 x 0.90` | Box 01과 같은 크기의 시각 변형이다. |
| `obstacle.box_03` | Box 03 | `0.90 x 0.90 x 0.90` | `0.90 x 0.90` | Box 01과 같은 크기의 시각 변형이다. |
| `obstacle.fire_hydrant` | Fire Hydrant | `0.70 x 0.70 x 1.60` | `0.70 x 0.70` | 좁은 고정 설비물로, 가장자리 배치에 적합하다. |
| `obstacle.mailbox` | Mailbox | `1.10 x 0.90 x 1.80` | `1.10 x 0.90` | 비교적 큰 고정 설비물이므로 보행로 중앙을 막지 않게 배치한다. |
| `obstacle.manhole_01` | Manhole 01 | `1.10 x 1.10 x 0.10` | `1.10 x 1.10` | 낮은 지면 물체다. 통과 가능 비용이나 주의 요소로 쓰기 좋다. |
| `obstacle.manhole_02` | Manhole 02 | `1.10 x 1.10 x 0.10` | `1.10 x 1.10` | Manhole 01과 같은 크기의 시각 변형이다. |
| `obstacle.manhole_03` | Manhole 03 | `1.10 x 1.10 x 0.10` | `1.10 x 1.10` | Manhole 01과 같은 크기의 시각 변형이다. |
| `obstacle.manhole_04` | Manhole 04 | `1.10 x 1.10 x 0.10` | `1.10 x 1.10` | Manhole 01과 같은 크기의 시각 변형이다. |
| `obstacle.road_barrier_01` | Road Barrier 01 | `2.40 x 0.70 x 1.20` | `2.40 x 0.70` | 협폭, gate, 경로 차단 의도를 표현할 때 우선 사용한다. |
| `obstacle.road_barrier_02` | Road Barrier 02 | `2.40 x 0.70 x 1.20` | `2.40 x 0.70` | Road Barrier 01과 같은 크기의 시각 변형이다. |
| `obstacle.road_cone_01` | Road Cone 01 | `0.70 x 0.70 x 1.40` | `0.70 x 0.70` | 여러 개를 `pattern`으로 배치해 임시 통제선을 만들기 좋다. |
| `obstacle.road_cone_02` | Road Cone 02 | `0.70 x 0.70 x 1.40` | `0.70 x 0.70` | Road Cone 01과 같은 크기의 시각 변형이다. |
| `obstacle.street_bank` | Street Bank | `2.00 x 0.90 x 1.20` | `2.00 x 0.90` | 길쭉한 거리 시설물이다. 보행 흐름을 막지 않도록 가장자리 기준으로 둔다. |
| `obstacle.trash_bin` | Trash Bin | `0.90 x 0.90 x 1.80` | `0.90 x 0.90` | Bin과 같은 크기의 휴지통 계열 장애물이다. |

## Placement 사용 기준

| placement kind | 사용 기준 | 주요 field |
| --- | --- | --- |
| `fixed` | 특정 prop 하나를 corridor-local anchor에 둔다. | `prop`, `at.segment`, `at.along_m`, `at.offset_m`, `at.lane`, `yaw_deg` |
| `pattern` | Gate, line, cluster 같은 정형 배열을 만든다. | `pattern`, `prop`, `at`, `count`, `spacing_m`, `gap_width_m` |
| `scatter` | Segment/lane 영역 안에 density 기반 clutter를 만든다. | `zone.segments`, `zone.lanes`, `density_per_10m`, `palette` |

## Lane Hints

| lane | 의미 |
| --- | --- |
| `walkway` | 주 보행로. |
| `building_edge` | 보행로 건물측 가장자리. |
| `center` | 보행로 중앙. |
| `curb_edge` | 보행로 연석측 가장자리. |
| `across` | 보행로를 가로지르는 방향. `pattern`에 사용한다. |

## Persona

Persona는 pedestrian encounter의 기본 행동 성향이다.

| persona_id | 의미 | 사용 기준 |
| --- | --- | --- |
| `passive` | 잘 비켜주는 보행자. | Robot이 무난히 통과 가능한 baseline. |
| `normal` | 자기 경로를 유지하되 적당히 양보하는 보행자. | 일반 보행자 흐름. |
| `assertive` | 잘 비켜주지 않는 보행자. | 협폭이나 대향 통과 stress test. |
| `vulnerable` | 저속, 큰 personal space, 낮은 회피 성향. | 조심스럽게 접근해야 하는 보행자. |

## Behavior Overrides

| field | 의미 |
| --- | --- |
| `cooperation` | Robot에게 양보하는 성향. `0`에 가까울수록 비협조적. |
| `evasiveness` | 옆으로 피하려는 성향. |
| `personal_space_m` | 유지하려는 개인 공간. |
| `awareness_horizon_s` | 충돌 또는 접근을 예측하는 시간 범위. |
| `max_yield_wait_s` | 양보하며 기다릴 수 있는 최대 시간. |
| `sidestep_distance_m` | 옆으로 비켜서는 거리. |

## Encounter Types

| type | 의미 | 주요 field |
| --- | --- | --- |
| `oncoming_pass` | 대향 보행자와 통과/양보 판단. | `at`, `meet_offset_m`, `persona`, `overrides` |
| `overtake` | 뒤에서 추월하는 보행자에 대한 반응. | `at`, `speed_mps`, `persona` |
| `cross_path` | 전방 끼어듦 또는 횡단 반응. | `at`, `trigger_distance_m`, `from`, `persona` |
| `standing_group` | 정지 군집 앞 통과/대기/우회 판단. | `at`, `size`, `blocked_width_ratio`, `persona` |

## 작성 전 확인

- `surface` 값이 Surface 목록에 있는가.
- `prop` 값이 Props 목록에 있는가.
- `palette.categories` 값이 Prop Categories 목록에 있는가.
- `palette.classes` 값이 Prop Classes 목록에 있는가.
- `persona` 값이 Persona 목록에 있는가.
- `encounters[].type` 값이 Encounter Types 목록에 있는가.
- Catalog 정의 자체를 `scenario.json`에 복사하지 않았는가.
- `background.persona_mix` 같은 v1 외부 거시 분포 값을 넣지 않았는가.
