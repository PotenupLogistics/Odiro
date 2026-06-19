# Scenario Template

경로:

```text
templates/scenarios/<Scenario>.template.json
```

schema:

```json
"scenario_template"
```

## 역할

Scenario Template은 시나리오 샘플을 만들기 위한 소스이며 world XY 좌표 목록으로 작성하지 않는다.
사용자 에디터와 LLM은 같은 schema를 편집한다.

작성 기준:

- 수치 필드는 고정값 또는 `{ "min": a, "max": b }` 범위를 쓸 수 있다.
- 문자열 선택지는 고정값 또는 `{ "choices": [...] }`를 쓸 수 있다.
- 시드 관련 setup, policy, robot setup 값은 이 schema에 넣지 않는다.

## Root

```json
{
  "schema": "scenario_template",
  "version": 1,
  "template_id": "pinch_oncoming_low_coop",
  "intent": "협폭 구간에서 대향 보행자와 조우할 때 로봇이 안전하게 통과하는지 검증한다.",
  "corridor": {},
  "obstacles": {},
  "pedestrians": {},
  "robot": {}
}
```

| 필드 | 필수 | 합의 |
| --- | --- | --- |
| `schema` | 필수 | 고정값 `scenario_template` |
| `version` | 필수 | schema version. v1은 `1` |
| `template_id` | 필수 | 사람이 읽을 수 있는 snake_case 식별자 |
| `intent` | 필수 | 이 template이 검증하려는 상황/가설 |
| `corridor` | 필수 | 맵의 공간 skeleton과 lane/surface 구성 |
| `obstacles` | 권장 | 정적 장애물 배치 규칙 |
| `pedestrians` | 권장 | 배경 보행자 수와 설계된 encounter |
| `robot` | 필수 | 로봇 시작/목적지 anchor |

## 다른 Schema와의 경계

| 필드/영역 | 합의 |
| --- | --- |
| `generation` | sample 생성 수와 seed는 `experiments/<Experiment>/setting.json` 소유 |
| 사용자 정의 점수 체계 | 사용하지 않음 |
| policy 설정 | `experiments/<Experiment>/policy/` schema에서 확정 |
| DeliveryBotSetup이었던 세부값 | `templates/profiles/*.json`과 `experiments/<Experiment>/profile.json` schema에서 확정 |
| 실행 payload | sample에 저장하지 않고 preview/run 시점에 파생 |

## 값 형태

고정값:

```json
"walkway_width_m": 3.0
```

범위값:

```json
"walkway_width_m": { "min": 2.5, "max": 4.0 }
```

선택값:

```json
"replaced_by": { "choices": ["grass", "road"] }
```

범위 또는 선택값은 Scenario Sample 생성 시 seed로 하나의 값으로 확정되며, 확정 결과는 `scenario.params`에 dotted-key로 기록한다.

## corridor

`corridor`는 시나리오의 공간 골격이다.
에디터에서는 spline/polyline처럼 그리고, LLM은 이를 segment, lane, along/offset 기준으로 읽고 쓴다.

| 필드 | 합의 |
| --- | --- |
| `axis.type` | v1은 `polyline` |
| `axis.points_m` | template-local meter 좌표. corridor 진행 방향의 기준선 |
| `walkway_width_m` | 보행 가능 주 통로 폭. 고정값 또는 범위값 |
| `building_side` | walkway 기준 건물측 lane 목록 |
| `curb_side` | walkway 기준 연석측 lane 목록 |
| `segments[].id` | template 안에서 unique한 segment id |
| `segments[].type` | `straight`, `narrowing`, `crosswalk`, `entrance` |
| `segments[].along_range_m` | corridor axis 위에서 segment가 차지하는 시작/끝 거리 |

lane/surface 규칙:

- `surface`는 UE Material 이름이 아니라 지면 의미다.
- surface 카탈로그가 collision profile, actor tag, region type, 보행자 통행 가능 여부를 연결한다.
- `walkway_width_m`와 양쪽 lane 폭은 Scenario Sample의 `scenario.semantic.layout`로 전개되고, 실행 시 compiler payload로 변환된다.

## obstacles

`obstacles`는 정적 장애물의 배치 규칙이다.
editor가 직접 놓은 장애물도 world XY가 아니라 corridor 기준 anchor로 저장한다.

| 필드 | 합의 |
| --- | --- |
| `min_clear_width_m` | 생성기가 유지해야 하는 최소 유효 통로 폭 |
| `placements[].id` | sample semantic의 `placed_by`와 연결되는 placement id |
| `placements[].kind` | `fixed`, `pattern`, `scatter` |
| `placements[].allow_blocking` | 의도적으로 최소 통로 폭을 깰 때 `true` |

placement kind:

| kind | 의미 | 주요 필드 |
| --- | --- | --- |
| `fixed` | 한 장애물을 특정 의미 위치에 배치 | `prop`, `at.segment`, `at.along_m`, `at.offset_m`, `at.lane`, `yaw_deg` |
| `pattern` | gate, line, cluster 같은 정형 배열 | `pattern`, `prop`, `at`, `count`, `spacing_m` 또는 `gap_width_m` |
| `scatter` | 영역 안에 밀도 기반 배치 | `zone.segments`, `zone.lanes`, `density_per_10m`, `palette` |

lane 위치 어휘:

| 값 | 의미 |
| --- | --- |
| `building_edge` | walkway의 건물측 가장자리 |
| `center` | walkway 중앙 |
| `curb_edge` | walkway의 연석측 가장자리 |
| `across` | walkway를 가로지르는 방향. pattern 전용 |

## pedestrians

`pedestrians`는 보행자 path 목록을 직접 쓰지 않는다.
생성기와 plan builder가 corridor 의미론에서 기준 동선을 만든다.

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

background:

| 필드 | 합의 |
| --- | --- |
| `background.count` | 배경 보행자 수. 고정값 또는 범위값 |
| `background.speed_mps` | 배경 보행자 속도. 고정값 또는 범위값 |
| `background.spawn_zone.segments` | 선택. 배경 보행자가 등장할 segment 제한 |

encounter:

| 필드 | 합의 |
| --- | --- |
| `encounters[].id` | template 안에서 unique한 encounter id |
| `encounters[].type` | `oncoming_pass`, `overtake`, `cross_path`, `standing_group` |
| `encounters[].at` | encounter가 발생할 segment id |
| `encounters[].persona` | 보행자 행동 프리셋 |
| `encounters[].overrides` | persona에서 일부 행동 벡터만 덮어쓸 때 사용 |

encounter type:

| type | 검증 표적 |
| --- | --- |
| `oncoming_pass` | 대향 보행자와 통과/양보 판단 |
| `overtake` | 뒤에서 추월하는 보행자에 대한 반응 |
| `cross_path` | 전방 끼어듦 반응 |
| `standing_group` | 정지 군집 앞 통과/대기/우회 판단 |

persona:

| persona | 의미 |
| --- | --- |
| `passive` | 잘 비켜주는 보행자 |
| `normal` | 자기 경로를 유지하되 적당히 양보 |
| `assertive` | 잘 비켜주지 않는 보행자 |
| `vulnerable` | 저속, 큰 personal space, 낮은 회피 성향 |

`persona`는 생성기에서 구체 행동 벡터로 전개된다.
엔진 실행 payload는 persona 이름 자체에 의존하지 않고, 전개된 behavior 값을 사용한다.

## robot

`robot`은 시작/목적지를 의미론적 anchor로 작성한다.
policy, 센서, 로컬 planner 튜닝, 시간 제한은 이 schema에 쓰지 않는다.
사용자 에디터가 viewport에서 지정한 위치도 world XY가 아니라 corridor-local pose로 저장한다.

```json
"robot": {
  "start": {
    "type": "entry"
  },
  "goal": {
    "type": "corridor_pose",
    "segment": "exit",
    "along_m": 24.5,
    "offset_m": 0.0,
    "lane": "walkway",
    "heading": "forward"
  }
}
```

| 필드 | 합의 |
| --- | --- |
| `start` | 로봇 시작 anchor object. v1 기본값은 `{ "type": "entry" }` |
| `goal` | 로봇 목적지 anchor object. v1 기본값은 `{ "type": "exit" }` |

anchor:

| 필드 | 합의 |
| --- | --- |
| `type` | `entry`, `exit`, `corridor_pose` |
| `segment` | `corridor_pose`일 때 기준 segment id |
| `along_m` | `corridor_pose`일 때 corridor axis 위 진행 거리. segment의 `along_range_m` 안에 있어야 함 |
| `offset_m` | `corridor_pose`일 때 corridor axis 기준 좌우 오프셋 |
| `lane` | 선택. `walkway`, `building_edge`, `center`, `curb_edge` 등 editor hint |
| `heading` | 선택. `forward`, `backward`, `auto` |

`entry`와 `exit`는 LLM이 간단히 사용할 수 있는 추상 anchor다.
`corridor_pose`는 editor가 사용자의 구체 지정 위치를 저장하기 위한 anchor다.

생성기는 `start`/`goal`을 Scenario Sample의 semantic route 정보로 확정하고, 실행 시 `actors.robot.xy_m`과 `route.goal_xy_m` 형태의 compiler payload로 변환한다.

## 검증

Template은 sample 생성 전에 검증한다.

| 등급 | 대응 |
| --- | --- |
| `error` | sample 생성 중단 |
| `warning` | 생성 계속, Scenario Sample의 `validation.diagnostics`에 기록 |
| `repair` | 보정 후 생성, 보정 사실을 `validation.diagnostics`에 기록 |

주요 검증 규칙:

- `segments[].id`, `placements[].id`, `encounters[].id`는 각각 unique해야 한다.
- placement, encounter, robot이 참조하는 segment는 존재해야 한다.
- `corridor_pose.along_m`은 참조 segment의 `along_range_m` 안에 있어야 한다.
- `allow_blocking` 없이 `min_clear_width_m` 계약을 깨면 error다.
