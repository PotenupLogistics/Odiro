# Environment Catalog

상태: LLM이 `scenario.json`을 작성할 때 참고하는 환경 어휘 가이드다.

LLM은 여기 있는 어휘를 Project Scenario의 `corridor`, `obstacles`, `pedestrians`에 사용한다.

## 작성 원칙

- surface, prop, persona, encounter type 이름은 이 문서의 어휘만 사용한다.
- 모르는 surface/prop/persona 이름을 임의로 만들지 않는다.
- catalog 정보를 scenario 안에 복사하지 않는다.
- Scenario에는 “무엇을 배치/생성할지”만 쓴다.
- 전역 거리/크기 단위는 meter다.

## Scenario에서 쓰는 위치

| Scenario field | 사용하는 어휘 |
| --- | --- |
| `corridor.building_side[].surface` | surface id |
| `corridor.curb_side[].surface` | surface id |
| `corridor.segments[].replaced_by` | surface id |
| `obstacles.placements[].prop` | prop id |
| `obstacles.placements[].palette.categories` | prop category |
| `obstacles.placements[].palette.classes` | prop class |
| `pedestrians.encounters[].type` | encounter type |
| `pedestrians.encounters[].persona` | persona id |
| `pedestrians.encounters[].overrides` | behavior field |

## Surface

`surface`는 지면의 의미 id다.

| surface_id | 로봇 기준 | 의미 | 주 사용처 |
| --- | --- | --- | --- |
| `sidewalk` | walkable | 기본 보행로 | 주 통로 |
| `crosswalk_stripe` | walkable | 횡단보도 표시/보행 가능 stripe | `crosswalk` segment |
| `grass` | penalty | 잔디/화단 | 통로를 좁히거나 회피 비용을 만들 때 |
| `road` | penalty | 차도 | 보도-차도 경계, 횡단 상황 |
| `driveway` | penalty | 진출입로 | 건물/연석 쪽 출입 구간 |
| `wall` | blocked | 벽/물리적 경계 | 건물측 경계 |
| `building` | blocked | 건물 영역 | 통과 불가 영역 |

사용 지침:

- 보행 가능한 주 통로는 `sidewalk`로 쓴다.
- 통로 폭을 줄이는 녹지/화단은 `grass`로 쓴다.
- 차도나 진출입로를 위험/비용 영역으로 표현할 때 `road`, `driveway`를 쓴다.
- 통과 불가능한 건물 경계는 `wall` 또는 `building`을 쓴다.
- `crosswalk_stripe`는 횡단보도처럼 보행 가능한 표시가 필요한 경우에만 쓴다.

예시:

```json
"corridor": {
  "walkway_width_m": { "min": 2.4, "max": 3.2 },
  "building_side": [
    { "surface": "wall", "width_m": 0.5 }
  ],
  "curb_side": [
    { "surface": "grass", "width_m": { "min": 0.4, "max": 1.0 } },
    { "surface": "road", "width_m": 5.0 }
  ]
}
```

## Prop

`prop`는 정적 장애물/소품의 의미 id다. LLM은 prop을 직접 world 좌표에 놓지 않고, `fixed`, `pattern`, `scatter` 배치 규칙에서 사용한다.

### Prop Category

| category | 의미 | 예시 상황 |
| --- | --- | --- |
| `street_furniture` | 벤치, 볼라드, 표지판 등 거리 시설물 | 보도 가장자리 clutter |
| `traffic_control` | 콘, 바리케이드 등 교통 통제물 | 임시 협폭, gate |
| `delivery_item` | 박스, 적재물, 배달 관련 물체 | 보도 위 임시 장애물 |
| `utility` | 전봇대, 설비함 등 시설물 | 고정 장애물 |
| `surface_object` | 낮은 턱, 덮개 등 지면 위 물체 | 낮은 profile 장애물 |

### Prop Class

| class | 의미 |
| --- | --- |
| `blocking` | 로봇이 통과할 수 없는 장애물 |
| `traversable_cost` | 통과는 가능하지만 비용/주의가 필요한 물체 |

### Sensor Profile

`sensor_profile`은 LLM이 prop을 고를 때 감지 난이도를 가늠하기 위한 의미 정보다.

| sensor_profile | 의미 |
| --- | --- |
| `solid` | 넓고 안정적으로 감지되는 물체 |
| `thin` | 콘/기둥처럼 얇아 일부 ray만 맞을 수 있는 물체 |
| `low_profile` | 낮아서 감지/회피가 어려울 수 있는 물체 |

### Prop 사용 지침

- 특정 장애물을 하나 놓을 때는 `fixed`를 쓴다.
- 통로를 일부 막는 문/게이트 형태는 `pattern`을 쓴다.
- 보도 가장자리 clutter는 `scatter`를 쓴다.
- `palette.categories`와 `palette.classes`는 scatter에서 후보 prop을 제한할 때 쓴다.
- 장애물의 구체 표현 방식은 scenario에 쓰지 않는다.

예시:

```json
"obstacles": {
  "min_clear_width_m": 0.9,
  "placements": [
    {
      "kind": "scatter",
      "id": "curb_clutter",
      "zone": {
        "segments": ["approach"],
        "lanes": ["curb_edge"]
      },
      "density_per_10m": { "min": 1, "max": 3 },
      "palette": {
        "categories": ["street_furniture"],
        "classes": ["blocking"]
      }
    },
    {
      "kind": "fixed",
      "id": "pinch_marker",
      "prop": "traffic_cone_01",
      "at": {
        "segment": "pinch",
        "along_m": 12.5,
        "offset_m": 0.48,
        "lane": "curb_edge"
      },
      "yaw_deg": 0
    }
  ]
}
```

## Pedestrian

`pedestrians`는 보행자 path 목록을 직접 쓰는 곳이 아니다. LLM은 배경 보행자 규모와 특정 encounter를 의미론적으로 작성한다.

### Persona

| persona_id | 의미 | 쓰기 좋은 상황 |
| --- | --- | --- |
| `passive` | 잘 비켜주는 보행자 | 로봇이 무난히 통과 가능한 baseline |
| `normal` | 자기 경로를 유지하되 적당히 양보 | 일반 보행자 흐름 |
| `assertive` | 잘 비켜주지 않는 보행자 | 협폭/대향 통과 스트레스 테스트 |
| `vulnerable` | 저속, 큰 personal space, 낮은 회피 성향 | 조심스럽게 접근해야 하는 보행자 |

`persona`는 생성 시 구체 `behavior` 값으로 전개된다. Scenario에서는 persona와 필요한 override만 쓴다.

### Behavior Override

| field | 의미 |
| --- | --- |
| `cooperation` | 로봇에게 양보하는 성향. `0`에 가까울수록 비협조적 |
| `evasiveness` | 옆으로 피하려는 성향 |
| `personal_space_m` | 유지하려는 개인 공간 |
| `awareness_horizon_s` | 충돌/접근을 예측하는 시간 범위 |
| `max_yield_wait_s` | 양보하며 기다릴 수 있는 최대 시간 |
| `sidestep_distance_m` | 옆으로 비켜서는 거리 |

### Encounter Type

| type | 의미 | 주요 field |
| --- | --- | --- |
| `oncoming_pass` | 대향 보행자와 통과/양보 판단 | `at`, `meet_offset_m`, `persona`, `overrides` |
| `overtake` | 뒤에서 추월하는 보행자 반응 | `at`, `speed_mps`, `persona` |
| `cross_path` | 전방 끼어듦/횡단 반응 | `at`, `trigger_distance_m`, `from`, `persona` |
| `standing_group` | 정지 군집 앞 통과/대기/우회 판단 | `at`, `size`, `blocked_width_ratio`, `persona` |

사용 지침:

- 좁은 통로에서 양보 판단을 보고 싶으면 `oncoming_pass`와 `assertive`를 우선 사용한다.
- 갑작스러운 진입을 만들고 싶으면 `cross_path`를 사용한다.
- 보행자가 로봇 뒤에서 접근하는 상황은 `overtake`를 사용한다.
- 통로 일부를 점유하는 정지 인파는 `standing_group`을 사용한다.
- `background.persona_mix` 같은 거시 분포 값은 v1에서 쓰지 않는다.

예시:

```json
"pedestrians": {
  "background": {
    "count": { "min": 1, "max": 3 },
    "speed_mps": { "min": 0.8, "max": 1.4 }
  },
  "encounters": [
    {
      "id": "main_conflict",
      "type": "oncoming_pass",
      "at": "pinch",
      "meet_offset_m": 0.0,
      "persona": "assertive",
      "overrides": {
        "cooperation": { "min": 0.15, "max": 0.4 }
      }
    }
  ]
}
```

## 작성 체크리스트

- `surface` 값이 이 문서의 surface 어휘인가
- `prop` 값이 catalog에 존재하는 prop id인가
- `palette.categories`가 정의된 prop category인가
- `palette.classes`가 정의된 prop class인가
- `persona`가 정의된 persona id인가
- `encounters[].type`이 정의된 encounter type인가
- scenario에 catalog 정의 자체를 복사하지 않았는가
- 너무 많은 macro pedestrian 분포 값을 넣지 않았는가
