# EpisodeSetup JSON Guide

이 문서는 EpisodeSetup JSON을 작성하기 위한 현재 MVP 기준 양식 설명서이다. 언리얼 엔진 프로젝트 디렉토리 기준으로 샘플은 `Json/EpisodeSetupSample.json`이며, 컴파일러는 `UEpisodeCompiler`가 JSON을 읽어 `FEpisodeWorldSpec`으로 변환한다.

현재 MVP의 목적은 다음 요소를 JSON으로 배치하고 단일 실행을 시작하는 것이다.

- 지면 영역: 이동 가능, 패널티, 차단 영역
- 정적 장애물: catalog에 등록된 prop mesh
- 보행자: spline 경로를 따라 직선 이동
- 로봇: 임시로 `BP_DeliveryBot_SimpleMesh`를 스폰하고 출발지/목적지를 주입
- 평가 설정: 종료 판정 반경, near-miss 거리, 감점 기본값

## 출력 원칙

LLM 출력은 반드시 순수 JSON이어야 한다. 주석, trailing comma, Markdown 코드블록, 설명 문장을 섞지 않는다.

좌표 입력 단위는 meter이고, Unreal 내부에서는 centimeter로 변환된다. 예를 들어 `location_m: [1.0, 0.0, 0.0]`은 내부적으로 `[100.0, 0.0, 0.0] cm`가 된다.

회전 입력 단위는 degree이다. `rotation_deg`는 object 형식 `{ "pitch": 0, "yaw": 90, "roll": 0 }`을 권장한다. 배열 형식도 일부 지원하지만 순서가 `[roll, pitch, yaw]`라 혼동 가능성이 있으므로 LLM 출력에는 쓰지 않는다.

모든 ID는 사람이 읽을 수 있는 semantic string이어야 한다. 예: `sidewalk_main`, `ped_01_path`, `cone_01`.

`actors` 아래의 모든 `instance_id`는 서로 중복되면 안 된다. 정적 장애물, 보행자, 로봇 사이에서도 같은 ID를 재사용하지 않는다.

`properties`는 확장용 필드이다. 현재 지원 타입은 boolean, number, string, 숫자 3개 배열(vector)이다. nested object나 일반 배열은 컴파일러가 무시할 수 있으므로 MVP 출력에는 쓰지 않는다.

## 좌표계와 단위

| 항목 | JSON 필드 | 입력 단위 | 내부 단위 | 설명 |
| --- | --- | --- | --- | --- |
| 위치 | `location_m`, `center_m`, `points_m`, `goal_m` | meter | centimeter | Unreal 월드 좌표 `X, Y, Z` |
| 평면 크기 | `size_m` | meter | centimeter | `[width_x, width_y]` |
| 회전 | `rotation_deg`, `yaw_deg` | degree | degree | `yaw`는 바닥 평면에서의 회전 |
| 속도 | `speed_mps` | meter/second | centimeter/second | 보행자 이동 속도 |
| 시간 | `time_limit_s`, `spawn_time_s`, `violation_after_s` | second | second | MVP에서 `spawn_time_s`는 기록되지만 지연 스폰은 아직 적용되지 않음 |
| 평가 거리 | `goal_acceptance_radius_m`, `near_miss.distance_m` | meter | centimeter | `FEpisodeEvaluationConfig`에 보존 |
| scale | `scale` | 배율 | 배율 | `[1, 1, 1]` 기본 |

## 루트 구조

```json
{
  "schema": "episode_actor_spawn_mvp",
  "version": 1,
  "scenario_id": "sidewalk_actor_spawn_001",
  "map_id": "EpisodeSandbox",
  "units": {
    "distance": "m",
    "angle": "deg"
  },
  "run": {},
  "evaluation": {},
  "ground_model": {},
  "paths": [],
  "actors": {}
}
```

| 필드 | 필수 | 타입 | 설명 |
| --- | --- | --- | --- |
| `schema` | 권장 | string | 사람이 읽는 스키마 이름. 현재 컴파일러 검증 대상은 아니지만 반드시 유지 권장 |
| `version` | 권장 | number | EpisodeSetup template version. 없으면 `1`로 처리 |
| `scenario_id` | 권장 | string | 실행 템플릿/시나리오 ID |
| `map_id` | 권장 | string | 대상 맵 ID. 현재 컴파일러가 맵 로드를 직접 하지는 않음 |
| `units` | 권장 | object | LLM 출력 단위 명시. 현재 입력은 meter/degree 기준 |
| `run` | 권장 | object | seed와 반복 실행 설정 |
| `evaluation` | 권장 | object | 종료 조건과 평가 지표 파라미터 |
| `ground_model` | 권장 | object | 지면 영역 목록 |
| `paths` | 보행자 사용 시 필수 | array | 보행자 경로 정의 |
| `actors` | 권장 | object | 정적 장애물, 보행자, 로봇 정의 |

## run

```json
"run": {
  "base_seed": 42,
  "iteration_index": 0,
  "time_limit_s": 30.0
}
```

| 필드 | 필수 | 타입/단위 | 기본값 | 설명 |
| --- | --- | --- | --- | --- |
| `base_seed` | 권장 | integer | `0` | 전체 episode seed. 내부 seed ledger의 기준값 |
| `iteration_index` | 권장 | integer | `0` | 같은 scenario의 반복 index |
| `time_limit_s` | 권장 | number, second | 없음 | 실행 제한 시간. EvaluationSubsystem이 timeout 실패 조건으로 사용 |

`base_seed`가 `42`이면 내부 seed는 `WorldSeed=42`, `LayoutSeed=143`, `StaticObstacleSeed=244`, `DynamicActorSeed=345`처럼 고정 offset으로 파생된다.

## evaluation

`evaluation`은 episode에서의 로봇 정책을 어떤 기준으로 평가할지 정의하는 레이어이다. 컴파일러는 이 블록을 `FEpisodeEvaluationConfig`로 변환하며, 생략된 값은 코드 기본값을 사용한다.

```json
"evaluation": {
  "goal_acceptance_radius_m": 0.5,
  "fall_angle_deg": 60.0,
  "near_miss": {
    "distance_m": 0.5
  },
  "scoring": {
    "static_obstacle_collision": -1.0,
    "blocked_region_collision": -1.0,
    "penalty_region_violation": -3.0,
    "pedestrian_near_miss": -3.0,
    "pedestrian_collision": -10.0
  }
}
```

| 필드 | 필수 | 타입/단위 | 기본값 | 설명 |
| --- | --- | --- | --- | --- |
| `goal_acceptance_radius_m` | 권장 | number, meter | `0.5` | 로봇이 목표 지점에 도착했다고 보는 반경 |
| `fall_angle_deg` | 권장 | number, degree | `60.0` | 로봇 넘어짐 실패 판정에 사용할 기울기 기준 |
| `near_miss` | 권장 | object | 기본값 사용 | 보행자 near-miss 판정 설정 |
| `scoring` | 권장 | object | 기본값 사용 | 평가 이벤트별 점수 변화량 |

### near_miss fields

| 필드 | 필수 | 타입/단위 | 기본값 | 설명 |
| --- | --- | --- | --- | --- |
| `distance_m` | 권장 | number, meter | `0.5` | 로봇과 보행자의 2D 거리가 이 값 이하이면 near-miss 구간으로 본다 |

EvaluationSubsystem은 `distance_m` 안에 들어온 시점을 near-miss 구간 시작으로 보고, 구간이 끝났을 때 `pedestrian_near_miss` 이벤트 하나를 요약 기록한다. 이벤트 properties에는 `start_time_s`, `end_time_s`, `duration_s`, `min_distance_m`, `pedestrian_id`가 들어간다.

### scoring fields

| 필드 | 필수 | 타입/단위 | 기본값 | 설명 |
| --- | --- | --- | --- | --- |
| `static_obstacle_collision` | 권장 | number | `-1.0` | 정적 장애물과 충돌했을 때 점수 변화량 |
| `blocked_region_collision` | 권장 | number | `-1.0` | blocked region collision과 충돌했을 때 점수 변화량 |
| `penalty_region_violation` | 권장 | number | `-3.0` | penalty region 위반 이벤트의 점수 변화량 |
| `pedestrian_near_miss` | 권장 | number | `-3.0` | near-miss 구간 1회당 점수 변화량 |
| `pedestrian_collision` | 권장 | number | `-10.0` | 보행자와 충돌했을 때 점수 변화량 |

`ground_model.regions[].penalty.cost`는 경로/환경 비용을 표현하는 값이고, `evaluation.scoring.penalty_region_violation`은 평가 결과에 반영되는 감점 값이다. 두 값은 같은 숫자를 쓸 필요가 없다.

## ground_model.regions

맵에 이미 큰 floor가 있음을 전제하고, JSON 영역을 의미 정보와 시각화 및 collision layer로 사용한다.

```json
"ground_model": {
  "default_region_type": "walkable",
  "regions": [
    {
      "region_id": "sidewalk_main",
      "region_type": "walkable",
      "shape": {
        "type": "rectangle",
        "center_m": [0.0, 0.0, 0.0],
        "size_m": [12.0, 6.0],
        "yaw_deg": 0.0
      },
      "traversability_score": 1.0
    }
  ]
}
```

### region fields

| 필드 | 필수 | 타입/단위 | 허용값/기본값 | 설명 |
| --- | --- | --- | --- | --- |
| `region_id` | 필수 | string | unique within regions | 지면 영역 ID |
| `region_type` | 필수 | string | `walkable`, `penalty`, `blocked` | 영역 의미 |
| `type` | 대체 가능 | string | `region_type` 대체 이름 | `region_type`이 없을 때만 사용 |
| `shape` | 필수 | object |  | 영역 형상 |
| `traversability_score` | 선택 | number | `1.0` | 이동 가능성 점수. 현재 기록/시각화용 |
| `penalty` | penalty 권장 | object | 없음 | 패널티 영역의 비용 정보 |
| `collision_tag` | blocked 권장 | string | 없음 | blocked collision actor tag |

### shape fields

| 필드 | 필수 | 타입/단위 | 허용값/기본값 | 설명 |
| --- | --- | --- | --- | --- |
| `type` | 필수 | string | `rectangle` | MVP에서 실제 지원되는 형상은 rectangle뿐 |
| `shape_type` | 대체 가능 | string | `type` 대체 이름 | `type`이 없을 때만 사용 |
| `center_m` | 필수 | `[x, y, z]`, meter |  | rectangle 중심 |
| `size_m` | 필수 | `[x_size, y_size]`, meter |  | rectangle 가로/세로 |
| `yaw_deg` | 선택 | number, degree | `0.0` | rectangle 평면 회전 |

`convex_polygon`은 enum에는 있지만 현재 MVP 컴파일 단계에서 unsupported error로 처리된다. LLM은 반드시 `rectangle`만 출력해야 한다.

### region_type 의미

| 값 | 의미 | 런타임 효과 |
| --- | --- | --- |
| `walkable` | 기본 이동 가능 영역 | 초록 decal로 표시, collision 없음 |
| `penalty` | 이동은 가능하지만 비용이 있는 영역 | 주황 decal로 표시, collision 없음 |
| `blocked` | 진입 불가/장애 영역 | 빨강 decal로 표시, box collision 활성화 |

### penalty fields

| 필드 | 필수 | 타입/단위 | 기본값 | 설명 |
| --- | --- | --- | --- | --- |
| `kind` | 권장 | string | empty | 패널티 종류. 예: `sidewalk_departure` |
| `cost` | 권장 | number | `0.0` | 비용 크기 |
| `violation_after_s` | 선택 | number, second | `0.0` | 몇 초 이상 머무르면 위반으로 볼지 |

## paths

보행자는 `path_id`로 `paths`에 정의된 경로를 참조한다. 현재 보행자 MVP는 spline path를 따라 이동한다.

```json
"paths": [
  {
    "path_id": "ped_01_path",
    "role": "pedestrian_baseline",
    "type": "spline",
    "points_m": [
      [-1.5, -1.1, 0.0],
      [-1.5, 1.1, 0.0]
    ],
    "closed_loop": false
  }
]
```

| 필드 | 필수 | 타입/단위 | 허용값/기본값 | 설명 |
| --- | --- | --- | --- | --- |
| `path_id` | 필수 | string | unique within paths | 경로 ID |
| `role` | 선택 | string | 컴파일러 미사용 | 사람이 읽는 경로 역할 |
| `type` | 선택 | string | `spline` 기본 | `spline` 권장. `waypoints`는 파싱되지만 MVP 스폰은 spline 중심 |
| `points_m` | 필수 | array of `[x,y,z]`, meter | 최소 2개 | 경로 점 목록 |
| `closed_loop` | 선택 | boolean | `false` | true이면 loop spline |

보행자 경로는 최소 2개 point가 필요하다. 직선 이동은 point 2개로 표현한다.

## actors.static_obstacles

정적 장애물은 코드에 등록된 catalog의 `prop_id`만 사용할 수 있다. LLM은 임의 mesh path를 출력하지 말고 아래 catalog 중 하나를 골라야 한다.

```json
"static_obstacles": [
  {
    "instance_id": "cone_01",
    "prop_id": "obstacle.road_cone_01",
    "transform": {
      "location_m": [2.0, 0.55, 0.0],
      "rotation_deg": { "pitch": 0.0, "yaw": 0.0, "roll": 0.0 },
      "scale": [1.0, 1.0, 1.0]
    }
  }
]
```

| 필드 | 필수 | 타입/단위 | 기본값 | 설명 |
| --- | --- | --- | --- | --- |
| `instance_id` | 필수 | string |  | actor instance ID. 전체 actors 안에서 unique |
| `prop_id` | 필수 | string |  | static obstacle catalog ID |
| `asset_id` | 대체 가능 | string |  | `prop_id` 대체 이름 |
| `transform` | 권장 | object | identity transform | 배치 위치/회전/스케일 |
| `properties` | 선택 | object | 없음 | 확장용 shallow property map |

### static obstacle prop catalog

| prop_id | semantic type | category | 용도 |
| --- | --- | --- | --- |
| `obstacle.bin` | `bin` | StreetFurniture | 일반 쓰레기통/통 |
| `obstacle.box_01` | `box` | DeliveryItem | 상자 1 |
| `obstacle.box_02` | `box` | DeliveryItem | 상자 2 |
| `obstacle.box_03` | `box` | DeliveryItem | 상자 3 |
| `obstacle.fire_hydrant` | `fire_hydrant` | Utility | 소화전 |
| `obstacle.mailbox` | `mailbox` | StreetFurniture | 우편함 |
| `obstacle.manhole_01` | `manhole` | SurfaceObject | 맨홀 1 |
| `obstacle.manhole_02` | `manhole` | SurfaceObject | 맨홀 2 |
| `obstacle.manhole_03` | `manhole` | SurfaceObject | 맨홀 3 |
| `obstacle.manhole_04` | `manhole` | SurfaceObject | 맨홀 4 |
| `obstacle.road_cone_01` | `road_cone` | TrafficControl | 라바콘 1 |
| `obstacle.road_cone_02` | `road_cone` | TrafficControl | 라바콘 2 |
| `obstacle.road_barrier_01` | `road_barrier` | TrafficControl | 도로 바리케이드 1 |
| `obstacle.road_barrier_02` | `road_barrier` | TrafficControl | 도로 바리케이드 2 |
| `obstacle.street_bank` | `street_bank` | StreetFurniture | 벤치 |
| `obstacle.trash_bin` | `trash_bin` | StreetFurniture | 쓰레기통 |

## actors.pedestrians

보행자는 `path_id`로 참조한 spline을 따라 이동한다. JSON의 보행자 위치는 ground 기준 위치를 넣는다. 런타임 보행자 actor는 내부적으로 Z offset을 적용한다.

```json
"pedestrians": [
  {
    "instance_id": "ped_01",
    "archetype_id": "adult_pedestrian",
    "path_id": "ped_01_path",
    "spawn_time_s": 0.0,
    "transform": {
      "location_m": [-1.5, -1.1, 0.0],
      "rotation_deg": { "pitch": 0.0, "yaw": 90.0, "roll": 0.0 },
      "scale": [1.0, 1.0, 1.0]
    },
    "movement": {
      "model": "straight_line",
      "speed_mps": 1.2,
      "initial_distance_m": 0.0,
      "auto_start": true
    }
  }
]
```

| 필드 | 필수 | 타입/단위 | 기본값 | 설명 |
| --- | --- | --- | --- | --- |
| `instance_id` | 필수 | string |  | actor instance ID. 전체 actors 안에서 unique |
| `archetype_id` | 선택 | string | `adult_pedestrian` | 보행자 archetype. 현재 BP는 하나로 고정 |
| `path_id` | 필수 | string |  | `paths[].path_id` 중 하나와 일치해야 함 |
| `spawn_time_s` | 선택 | number, second | `0.0` | MVP에서는 기록되지만 즉시 스폰됨 |
| `transform` | 권장 | object | identity transform | 초기 위치/회전 |
| `movement` | 선택 | object | 아래 기본값 | 이동 설정 |
| `properties` | 선택 | object | 없음 | 확장용 shallow property map |

### movement fields

| 필드 | 필수 | 타입/단위 | 기본값 | 설명 |
| --- | --- | --- | --- | --- |
| `model` | 권장 | string | 없음 | 현재는 `straight_line`을 사용 |
| `speed_mps` | 선택 | number, m/s | `1.2` 상당 | 보행자 속도. 내부 `speed_cm_per_second`로 변환 |
| `initial_distance_m` | 선택 | number, meter | `0.0` | path 위 초기 진행 거리 |
| `auto_start` | 선택 | boolean | `true` | 스폰 직후 path following 시작 여부 |

## actors.robot

로봇 액터는 AWheeledVehiclePawn을 상속받은 class로서 position-base가 아닌, 방향과 힘을 통해 이동한다.

```json
"robot": {
  "instance_id": "robot_01",
  "asset_id": "delivery_bot",
  "spawn_only": false,
  "transform": {
    "location_m": [-5.0, 0.0, 0.0],
    "rotation_deg": { "pitch": 0.0, "yaw": 0.0, "roll": 0.0 },
    "scale": [1.0, 1.0, 1.0]
  },
  "route": {
    "goal_m": [5.0, 0.0, 0.0],
    "auto_start": true
  }
}
```

| 필드 | 필수 | 타입/단위 | 기본값 | 설명 |
| --- | --- | --- | --- | --- |
| `instance_id` | 필수 | string |  | robot instance ID. 전체 actors 안에서 unique |
| `actor_id` | 대체 가능 | string |  | `instance_id` 대체 이름 |
| `asset_id` | 필수 | string |  | 현재는 `delivery_bot` 권장 |
| `type` | 대체 가능 | string |  | `asset_id` 대체 이름 |
| `spawn_only` | 선택 | boolean | `true` | true이면 스폰만 하고 route를 주입하지 않음 |
| `transform` | 권장 | object | identity transform | 로봇 출발 위치. `location_m`은 ground 기준 시작점 |
| `route.goal_m` | 이동 시 필수 | `[x,y,z]`, meter | 없음 | 목적지. 내부 `goal_cm`으로 변환 |
| `route.auto_start` | 선택 | boolean | `true` | false이면 route가 있어도 자동 시작하지 않음 |
| `goal_m` | 대체 가능 | `[x,y,z]`, meter | 없음 | `route.goal_m`의 root-level fallback |
| `properties` | 선택 | object | 없음 | 확장용 shallow property map |

`spawn_only`가 `false`이면 `route.goal_m` 또는 `goal_m`을 반드시 제공해야 한다. 없으면 컴파일은 warning을 남기고 로봇 경로 주입을 건너뛴다.

로봇 경로 탐색은 DeliveryBot grid를 사용한다. 테스트 맵에는 출발지와 목적지가 grid bounds 안의 walkable cell이어야 한다.

## transform object

`transform`은 actor 공통 배치 형식이다.

```json
"transform": {
  "location_m": [0.0, 0.0, 0.0],
  "rotation_deg": {
    "pitch": 0.0,
    "yaw": 0.0,
    "roll": 0.0
  },
  "scale": [1.0, 1.0, 1.0]
}
```

| 필드 | 필수 | 타입/단위 | 기본값 | 설명 |
| --- | --- | --- | --- | --- |
| `location_m` | 선택 | `[x,y,z]`, meter | `[0,0,0]` | 월드 위치 |
| `rotation_deg` | 선택 | object, degree | zero rotation | `pitch`, `yaw`, `roll` |
| `scale` | 선택 | `[x,y,z]` | `[1,1,1]` | actor scale |

`transform` 자체가 없으면 identity transform이 사용되고 warning이 발생한다. LLM 출력에서는 항상 명시하는 것을 권장한다.

## LLM 출력 검수 체크리스트

- JSON만 출력했는가
- 모든 숫자 배열 길이가 맞는가: 위치는 3개, 크기는 2개, scale은 3개
- 모든 actor `instance_id`가 unique인가
- 모든 `path_id` 참조가 `paths[].path_id`에 존재하는가
- 모든 `prop_id`가 catalog에 존재하는가
- 지면 shape는 `rectangle`만 사용했는가
- `spawn_only=false`인 robot에 `route.goal_m`이 있는가
- `evaluation.near_miss`에는 `distance_m`만 사용했는가
- `evaluation.scoring`의 감점 값은 number인가
- 좌표와 크기는 meter 단위인가
- 회전은 degree 단위인가
- 보행자 `speed_mps`가 현실적인 값인가. 예: `0.8 ~ 1.8`
- actor가 blocked region 안이나 floor 밖에 배치되지 않았는가

## Sample JSON

```json
{
  "schema": "episode_actor_spawn_mvp",
  "version": 1,
  "scenario_id": "sidewalk_actor_spawn_001",
  "map_id": "EpisodeSandbox",
  "units": {
    "distance": "m",
    "angle": "deg"
  },
  "run": {
    "base_seed": 42,
    "iteration_index": 0,
    "time_limit_s": 30.0
  },
  "evaluation": {
    "goal_acceptance_radius_m": 0.5,
    "fall_angle_deg": 60.0,
    "near_miss": {
      "distance_m": 0.5
    },
    "scoring": {
      "static_obstacle_collision": -1.0,
      "blocked_region_collision": -1.0,
      "penalty_region_violation": -3.0,
      "pedestrian_near_miss": -3.0,
      "pedestrian_collision": -10.0
    }
  },
  "ground_model": {
    "default_region_type": "walkable",
    "regions": [
      {
        "region_id": "sidewalk_main",
        "region_type": "walkable",
        "shape": {
          "type": "rectangle",
          "center_m": [0.0, 0.0, 0.0],
          "size_m": [6.0, 4.0],
          "yaw_deg": 0.0
        },
        "traversability_score": 1.0
      },
      {
        "region_id": "road_penalty",
        "region_type": "penalty",
        "shape": {
          "type": "rectangle",
          "center_m": [0.0, 6.0, 0.0],
          "size_m": [6.0, 2.0],
          "yaw_deg": 0.0
        },
        "penalty": {
          "kind": "sidewalk_departure",
          "cost": 5.0,
          "violation_after_s": 0.2
        },
        "traversability_score": 0.6
      },
      {
        "region_id": "building_wall",
        "region_type": "blocked",
        "shape": {
          "type": "rectangle",
          "center_m": [0.0, -5.0, 0.0],
          "size_m": [6.0, 1.0],
          "yaw_deg": 0.0
        },
        "collision_tag": "wall"
      }
    ]
  },
  "paths": [
    {
      "path_id": "ped_01_path",
      "role": "pedestrian_baseline",
      "type": "spline",
      "points_m": [
        [-3.0, -3.0, 0.0],
        [3.0, 3.0, 0.0]
      ],
      "closed_loop": false
    }
  ],
  "actors": {
    "static_obstacles": [
      {
        "instance_id": "cone_01",
        "prop_id": "obstacle.road_cone_01",
        "transform": {
          "location_m": [2.0, 0.55, 0.0],
          "rotation_deg": {
            "pitch": 0.0,
            "yaw": 0.0,
            "roll": 0.0
          },
          "scale": [1.0, 1.0, 1.0]
        }
      },
      {
        "instance_id": "bin_01",
        "prop_id": "obstacle.bin",
        "transform": {
          "location_m": [3.8, 0.65, 0.0],
          "rotation_deg": {
            "pitch": 0.0,
            "yaw": 15.0,
            "roll": 0.0
          },
          "scale": [1.0, 1.0, 1.0]
        }
      },
      {
        "instance_id": "barrier_01",
        "prop_id": "obstacle.road_barrier_01",
        "transform": {
          "location_m": [-1.5, -0.45, 0.0],
          "rotation_deg": {
            "pitch": 0.0,
            "yaw": 90.0,
            "roll": 0.0
          },
          "scale": [1.0, 1.0, 1.0]
        }
      }
    ],
    "pedestrians": [
      {
        "instance_id": "ped_01",
        "archetype_id": "adult_pedestrian",
        "path_id": "ped_01_path",
        "spawn_time_s": 0.0,
        "transform": {
          "location_m": [-1.5, -1.1, 0.0],
          "rotation_deg": {
            "pitch": 0.0,
            "yaw": 90.0,
            "roll": 0.0
          },
          "scale": [1.0, 1.0, 1.0]
        },
        "movement": {
          "model": "straight_line",
          "speed_mps": 1.2,
          "initial_distance_m": 0.0,
          "auto_start": true
        }
      }
    ],
    "robot": {
      "instance_id": "robot_01",
      "asset_id": "delivery_bot",
      "spawn_only": false,
      "transform": {
        "location_m": [-5.0, 0.0, 0.0],
        "rotation_deg": {
          "pitch": 0.0,
          "yaw": 0.0,
          "roll": 0.0
        },
        "scale": [1.0, 1.0, 1.0]
      },
      "route": {
        "goal_m": [5.0, 0.0, 0.0],
        "auto_start": true
      }
    }
  }
}
```
