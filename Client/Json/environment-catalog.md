# Environment Catalog

LLM이 project `scenario.json`을 작성할 때 사용할 수 있는 현재 구현된 환경 어휘다.
이 문서에 없는 id는 LLM authoring surface로 사용하지 않는다.

## 사용 위치

| Scenario field | 사용하는 어휘 |
| --- | --- |
| `corridor.building_side[].surface` | Surface id |
| `corridor.curb_side[].surface` | Surface id |
| `corridor.segments[].replaced_by` | Surface id |
| `obstacles.placements[].prop` | Prop id |
| `obstacles.placements[].at.lane` | Lane hint |

## 작성 규칙

- 이 문서에 있는 surface, prop, lane hint만 사용한다.
- 모르는 id를 임의로 만들지 않는다.
- Catalog 정의를 `scenario.json` 안에 복사하지 않는다.
- CityBuildings block catalog는 `scenario.json`에서 직접 참조하지 않는다.
- `scenario.json`에는 corridor, fixed obstacle, robot anchor만 기록한다.
- 거리와 크기는 meter 단위다.

## Surface

Surface는 corridor lane이나 segment surface의 의미 id다. 현재 LLM authoring에서는 `walkway`, `road`, `building`만 사용한다.

| id | Semantic role | 사용 기준 |
| --- | --- | --- |
| `walkway` | walkable | 중앙 corridor walkway와 기본 보행 가능 영역. |
| `road` | penalty | road-side generated city와 차도 성격의 segment override. |
| `building` | blocked | building-side generated city를 요청하는 측면 surface. |

## Props

Prop id는 `obstacles.placements[].prop`에서 fixed obstacle 하나를 배치할 때 사용하는 정적 장애물 id다.
현재 LLM authoring에서는 prop의 class, sensor profile, bounding box를 직접 작성하지 않는다.

| prop_id | 표시명 | 사용 기준 |
| --- | --- | --- |
| `obstacle.billboard` | Billboard | 디지털 입간판이나 넓은 표지판형 고정 장애물. |
| `obstacle.bin` | Bin | 보도 가장자리 휴지통. |
| `obstacle.box_01` | Box 01 | 배달 박스나 적재물. |
| `obstacle.box_02` | Box 02 | 배달 박스나 적재물 변형. |
| `obstacle.box_03` | Box 03 | 배달 박스나 적재물 변형. |
| `obstacle.bus_shelter` | Bus Shelter | 버스 정류장 쉘터처럼 큰 보행로 가장자리 시설물. |
| `obstacle.fire_hydrant` | Fire Hydrant | 보도 가장자리 고정 설비물. |
| `obstacle.mailbox` | Mailbox | 우편함형 고정 설비물 또는 보도 가장자리 장애물. |
| `obstacle.manhole_01` | Manhole 01 | 낮은 지면 물체 시각 자산. |
| `obstacle.manhole_02` | Manhole 02 | 낮은 지면 물체 시각 자산 변형. |
| `obstacle.manhole_03` | Manhole 03 | 낮은 지면 물체 시각 자산 변형. |
| `obstacle.manhole_04` | Manhole 04 | 낮은 지면 물체 시각 자산 변형. |
| `obstacle.road_barrier_01` | Road Barrier 01 | 공사 구간, 차단 구간, 협폭 유도용 단독 차단물. |
| `obstacle.road_barrier_02` | Road Barrier 02 | 공사 구간, 차단 구간, 협폭 유도용 단독 차단물 변형. |
| `obstacle.road_cone_01` | Road Cone 01 | 단독 임시 통제물. |
| `obstacle.road_cone_02` | Road Cone 02 | 단독 임시 통제물 변형. |
| `obstacle.street_bank` | Street Bank | 보도 가장자리 거리 시설물. |
| `obstacle.street_light` | Street Light | 얇고 높은 기둥형 시설물. |
| `obstacle.trash_bin` | Trash Bin | 보도 가장자리 휴지통. |
| `obstacle.tree` | Tree | 가로수나 보도 가장자리 자연물. |

## Placement 사용 기준

| placement kind | 사용 기준 | 주요 field |
| --- | --- | --- |
| `fixed` | 특정 prop 하나를 corridor-local anchor에 둔다. | `prop`, `at.segment`, `at.along_m`, `at.offset_m`, `at.lane`, `yaw_deg`, `allow_blocking` |

## Lane Hints

| lane | 의미 |
| --- | --- |
| `walkway` | 중앙 walkway lane 기준 배치. |
| `center` | corridor axis 기준 배치. |
| `building_edge` | 중앙 walkway의 building-side edge 기준 배치. |
| `curb_edge` | 중앙 walkway의 road-side edge 기준 배치. |

## 작성 전 확인

- surface 값이 Surface 목록에 있는가.
- prop 값이 Props 목록에 있는가.
- `at.lane` 값이 Lane Hints 목록에 있는가.
- fixed obstacle이 참조하는 segment가 `scenario.json`의 `corridor.segments[].id`에 있는가.
- Catalog 정의 자체를 `scenario.json`에 복사하지 않았는가.
- 이 문서에 없는 scenario field를 추가하지 않았는가.
