# scenario.json

사용자, editor, LLM이 함께 편집하는 project scenario 파일이다. 이 문서는 현재 구현된 시나리오 환경 생성 경로에서 LLM이 작성해도 되는 필드만 다룬다.

Parser와 serializer는 호환성을 위해 더 많은 필드를 받을 수 있지만, 이 문서에 없는 필드는 LLM authoring surface로 사용하지 않는다.

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
| `corridor` | object | 예 | 로봇 경로와 generated city 생성을 위한 공간 골격. |
| `obstacles` | object | 아니오 | Fixed 정적 장애물 배치 규칙. 장애물이 없으면 생략할 수 있다. |
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
"replaced_by": { "choices": ["walkway", "road"] }
```

Range와 choices는 episode별 scenario_sample 생성 시 seed로 확정된다. LLM은 구현 의미가 문서화된 필드에만 range 또는 choices를 사용한다.

## corridor

| 필드 | 타입 | 필수 | 설명 |
| --- | --- | --- | --- |
| `axis` | object | 예 | Corridor route axis. |
| `walkway_width_m` | number or range | 예 | 중앙 walkway 폭. 단위 m. |
| `building_side` | array | 예 | Building-side generated city를 켜는 surface intent. |
| `curb_side` | array | 예 | Road-side generated city를 켜는 surface intent. |
| `segments` | array | 예 | Corridor axis 상의 의미 segment 목록. |

### corridor.axis

| 필드 | 타입 | 필수 | 설명 |
| --- | --- | --- | --- |
| `type` | string | 예 | v1 고정값 `polyline`. |
| `points_m` | array | 예 | 최소 두 개의 local XY point. 단위 m. |

`points_m`은 corridor의 골격이다. Editor, simulation, replay는 이 polyline과 segment 구간을 기준으로 walkway, building-side expansion, road-side band, CityBuildings visual block을 파생한다.

### corridor.building_side[] / corridor.curb_side[]

| 필드 | 타입 | 필수 | 설명 |
| --- | --- | --- | --- |
| `surface` | string | 예 | `building_side`는 `building`, `curb_side`는 `road`를 사용한다. |
| `width_m` | number or range | 예 | 호환성 필수 필드. 현재 generated city geometry에서는 이 값으로 폭을 조절하지 않는다. |

`building_side`에 `surface: "building"`을 두면 corridor 한쪽에 walkable building expansion과 building frontage visual이 생성된다.

`curb_side`에 `surface: "road"`를 두면 corridor 한쪽에 fixed curb/2-lane road 영역과 road/corner visual이 생성된다.

`width_m`은 validator가 요구하므로 작성해야 하지만, 실제 building depth, curb width, road width는 코드와 CityBuildings catalog 규칙이 결정한다. LLM은 예시에서 일정한 placeholder 값을 사용한다.

### corridor.segments[]

| 필드 | 타입 | 필수 | 설명 |
| --- | --- | --- | --- |
| `id` | string | 예 | Obstacle과 robot anchor가 참조하는 unique segment id. |
| `type` | string | 예 | 호환성 필수 필드. 현재 LLM authoring에서는 `straight`만 사용한다. |
| `along_range_m` | array | 예 | `[start_m, end_m]` 형태의 corridor axis 진행 구간. |
| `replaced_by` | string or choices | 아니오 | 이 segment의 중앙 walkway surface를 대체할 surface id. |

`along_range_m`은 corridor axis 진행 거리 기준이다. Segment 구간은 서로 겹치지 않고 route axis 길이 안에 있어야 한다.

`replaced_by`는 중앙 walkway surface override다. Road/corner/crossroad visual 배치는 이 필드가 아니라 generated city와 CityBuildings catalog 규칙에서 파생된다.

## obstacles

| 필드 | 타입 | 필수 | 설명 |
| --- | --- | --- | --- |
| `min_clear_width_m` | number or range | 아니오 | 중앙 walkway 기준 fixed obstacle clear-width guard. |
| `placements` | array | 아니오 | Fixed 정적 장애물 배치 목록. |

`obstacles`는 정적 장애물이 필요할 때만 작성한다. 현재 LLM authoring에서는 `kind: "fixed"`만 사용한다.

### obstacles.placements[]

| 필드 | 타입 | 필수 | 설명 |
| --- | --- | --- | --- |
| `id` | string | 예 | Unique placement id. |
| `kind` | string | 예 | 현재 LLM authoring에서는 `fixed`만 사용한다. |
| `prop` | string | 예 | Static obstacle prop id. |
| `at` | object | 예 | Corridor-local placement anchor. |
| `yaw_deg` | number or range | 아니오 | Obstacle local yaw. 단위 degree. |
| `allow_blocking` | boolean | 아니오 | `min_clear_width_m` 위반을 의도할 때만 `true`. |

Fixed obstacle은 sample의 `semantic.static_obstacles[]`로 생성된다. Clear-width 계산은 중앙 walkway와 fixed obstacle semantic footprint를 기준으로 한다.

### placement.at

| 필드 | 타입 | 필수 | 설명 |
| --- | --- | --- | --- |
| `segment` | string | 예 | Placement anchor가 속한 segment id. |
| `along_m` | number or range | 예 | Corridor axis 진행 거리. 단위 m. |
| `offset_m` | number or range | 예 | 기준 lane 또는 axis에서의 lateral offset. 단위 m. |
| `lane` | string | 아니오 | `walkway`, `center`, `building_edge`, `curb_edge`, `across` 중 하나. |

`lane`을 생략하면 `offset_m`은 corridor axis 기준이다. `walkway`는 중앙 walkway lane 기준, `building_edge`와 `curb_edge`는 walkway 양쪽 edge 기준 배치에 사용한다.

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
| `along_m` | number or range | `corridor_pose`이면 예 | Corridor axis 진행 거리. 단위 m. |
| `offset_m` | number or range | `corridor_pose`이면 예 | Corridor axis 기준 lateral offset. 단위 m. |
| `heading` | string | 아니오 | `forward`, `backward`, `auto`. |

`entry`는 첫 segment 시작점, `exit`은 마지막 segment 끝점을 사용한다. `entry`/`exit`에서는 `segment`, `along_m`, `offset_m`을 작성하지 않는다.

`corridor_pose`는 지정한 segment와 along/offset을 사용한다. `heading: "backward"`만 진행 방향에서 180도 돌리고, `auto`와 `forward`는 현재 같은 방향으로 처리된다.

## 작성 규칙

- `corridor.axis.points_m`은 최소 두 점을 가져야 한다.
- `corridor.segments[].id`와 `obstacles.placements[].id`는 각각 unique해야 한다.
- `corridor.segments[].type`은 `straight`로 작성한다.
- `corridor.building_side[].surface`는 `building`, `corridor.curb_side[].surface`는 `road`를 사용한다.
- `corridor.building_side[].width_m`와 `corridor.curb_side[].width_m`는 호환성 필수 필드이며 generated city 폭 조절값이 아니다.
- Obstacle과 robot anchor가 참조하는 segment는 존재해야 한다.
- `corridor_pose.along_m`은 참조 segment의 `along_range_m` 안에 있어야 한다.
- `allow_blocking` 없이 `min_clear_width_m` 계약을 깨면 error다.
- Unreal actor/world payload는 저장하지 않는다. Preview/run 시점에 파생한다.

## 최소 예시

```json
{
  "schema": "scenario",
  "version": 1,
  "scenario_id": "city_corner_001",
  "intent": "Robot follows a right-angle walkway with buildings on the left and a road on the right.",
  "corridor": {
    "axis": {
      "type": "polyline",
      "points_m": [[0, 0], [40, 0], [40, 60]]
    },
    "walkway_width_m": 3.0,
    "building_side": [
      { "surface": "building", "width_m": 10.0 }
    ],
    "curb_side": [
      { "surface": "road", "width_m": 10.0 }
    ],
    "segments": [
      { "id": "seg_001", "type": "straight", "along_range_m": [0, 40] },
      { "id": "seg_002", "type": "straight", "along_range_m": [40, 100] }
    ]
  },
  "robot": {
    "start": { "type": "entry", "heading": "forward" },
    "goal": { "type": "exit", "heading": "forward" }
  }
}
```
