# ScenarioSetup JSON Guide

이 문서는 LLM이 ScenarioSetup JSON을 안정적으로 생성하기 위한 축약 양식을 설명한다. 기본 샘플은 `Json/Input/EpisodeSetupSample.json` 및 `Json/Input/EpisodeSetupSample_*.json`을 기준으로 한다. Planned pedestrian 샘플은 `Json/Input/EpisodeSetupSample_PlannedPedestrianObstacle.json`, `Json/Input/EpisodeSetupSample_PlannedPedestrianRobotOverlap.json`을 참고한다.

ScenarioSetup JSON은 시나리오 실행 정보, 지면 영역, 보행자 경로, 정적 장애물, 보행자, 로봇 배치와 로봇 목적지를 정의한다. DeliveryBot의 주행/센서/정책 튜닝값은 같은 실행 pair의 DeliveryBotSetup JSON에서 정의한다.

## 출력 원칙

- JSON만 출력한다. Markdown 코드블록, 주석, trailing comma는 사용하지 않는다.
- 좌표 단위는 meter로 고정한다. 컴파일러는 Unreal centimeter로 변환한다.
- 각도 단위는 degree로 고정한다.
- actor 배치는 2D 평면 기준이다. `xy_m`만 입력하고 Z는 `0`으로 고정한다.
- actor 회전은 `yaw_deg`만 입력한다. pitch/roll은 `0`으로 고정한다.
- actor scale은 JSON에서 입력하지 않는다. 내부에서 `[1, 1, 1]`로 고정한다.
- legacy 보행자는 `paths[]`의 spline path를 사용한다. `role`, `type`은 출력하지 않는다.
- `planned_trajectory` 보행자는 `paths[]` 대신 `start_xy_m` / `goal_xy_m`을 사용한다.
- `units`는 출력하지 않는다. `distance=m`, `angle=deg`로 고정 해석한다.

## Root

```json
{
  "schema": "scenario_actor_spawn_mvp",
  "version": 1,
  "scenario_id": "sidewalk_actor_spawn_001",
  "map_id": "EpisodeSandbox",
  "run": {},
  "evaluation": {},
  "ground_model": {},
  "paths": [],
  "actors": {}
}
```

| 필드 | 필수 | 설명 |
| --- | --- | --- |
| `schema` | 권장 | 사람이 읽는 스키마 이름 |
| `version` | 권장 | ScenarioSetup 버전. 없으면 `1`로 처리 |
| `scenario_id` | 권장 | 시나리오 ID |
| `map_id` | 권장 | 대상 맵 ID |
| `run` | 권장 | seed와 시간 제한 |
| `evaluation` | 권장 | 종료 조건과 평가 파라미터 |
| `ground_model` | 권장 | 지면 영역 목록 |
| `paths` | legacy path 보행자 사용 시 필수 | 보행자 spline 경로. `planned_trajectory`만 쓰면 빈 배열 가능 |
| `actors` | 권장 | 정적 장애물, 보행자, 로봇 |

## Coordinates

| 의미 | 필드 | 입력 | 내부 |
| --- | --- | --- | --- |
| actor 위치 | `xy_m` | `[x, y]` meter | `[x, y, 0]` cm |
| actor yaw | `yaw_deg` | degree | degree |
| region 중심 | `center_xy_m` | `[x, y]` meter | `[x, y, 0]` cm |
| region 크기 | `size_m` | `[x_size, y_size]` meter | cm |
| path point | `points_xy_m` | array of `[x, y]` meter | `[x, y, 0]` cm |
| planned pedestrian start | `start_xy_m` | `[x, y]` meter | `[x, y, 0]` cm |
| planned pedestrian goal | `goal_xy_m` | `[x, y]` meter | `[x, y, 0]` cm |
| robot goal | `goal_xy_m` | `[x, y]` meter | `[x, y, 0]` cm |

## Run

```json
"run": {
  "base_seed": 42,
  "iteration_index": 0,
  "time_limit_s": 60
}
```

## Evaluation

```json
"evaluation": {
  "goal_acceptance_radius_m": 1,
  "tip_over_angle_deg": 60,
  "near_miss": {
    "distance_m": 0.5
  },
  "scoring": {
    "static_obstacle_collision": -1,
    "blocked_region_collision": -1,
    "penalty_region_violation": -3,
    "pedestrian_near_miss": -3,
    "pedestrian_collision": -10
  }
}
```

## Ground Regions

```json
"ground_model": {
  "default_region_type": "walkable",
  "regions": [
    {
      "region_id": "sidewalk_main",
      "region_type": "walkable",
      "shape": {
        "type": "rectangle",
        "center_xy_m": [0, 0],
        "size_m": [12, 8],
        "yaw_deg": 0
      },
      "traversability_score": 1
    }
  ]
}
```

| 필드 | 필수 | 설명 |
| --- | --- | --- |
| `region_id` | 필수 | 지면 영역 ID |
| `region_type` | 필수 | `walkable`, `penalty`, `blocked` |
| `shape.type` | 필수 | 현재는 `rectangle`만 사용 |
| `shape.center_xy_m` | 필수 | rectangle 중심 |
| `shape.size_m` | 필수 | rectangle 크기 |
| `shape.yaw_deg` | 선택 | rectangle yaw. 기본 `0` |
| `traversability_score` | 선택 | 이동 가능성 점수 |
| `penalty` | penalty 권장 | 패널티 종류와 비용 |
| `collision_tag` | blocked 권장 | blocked collision actor tag |

## Paths

```json
"paths": [
  {
    "path_id": "ped_01_path",
    "points_xy_m": [
      [-3, -3],
      [3, 3]
    ],
    "closed_loop": false
  }
]
```

| 필드 | 필수 | 설명 |
| --- | --- | --- |
| `path_id` | 필수 | 경로 ID |
| `points_xy_m` | 필수 | 최소 2개의 `[x, y]` point |
| `closed_loop` | 선택 | 기본 `false` |

`role`과 `type`은 출력하지 않는다. 컴파일러는 path type을 spline 기본값으로 사용한다.

`paths[]`는 legacy spline/path follower 보행자용이다. `movement.model`이 `planned_trajectory`인 보행자는 이 배열을 plan source로 사용하지 않고, `start_xy_m` / `goal_xy_m`에서 setup-time baseline plan을 만든다.

## Actor Placement

정적 장애물, 보행자, 로봇은 공통으로 축약 배치 필드를 쓴다.

```json
"xy_m": [2, 1],
"yaw_deg": 15
```

`xy_m`이 없으면 `[0, 0]`으로 해석된다. `yaw_deg`가 없으면 `0`으로 해석된다. `transform` object는 지원하지 않는다.

## Static Obstacles

```json
"static_obstacles": [
  {
    "instance_id": "cone_01",
    "prop_id": "obstacle.road_cone_01",
    "xy_m": [2, 1],
    "yaw_deg": 0
  }
]
```

| 필드 | 필수 | 설명 |
| --- | --- | --- |
| `instance_id` | 필수 | 전체 actors 안에서 unique |
| `prop_id` | 필수 | static obstacle catalog ID |
| `asset_id` | 대체 가능 | `prop_id` alias |
| `xy_m` | 권장 | 2D 위치 |
| `yaw_deg` | 선택 | yaw |
| `properties` | 선택 | shallow 확장 map |

## Pedestrians

보행자는 두 입력 방식을 지원한다.

| 방식 | `movement.model` | 경로 입력 | 용도 |
| --- | --- | --- | --- |
| legacy path follower | `straight_line`, `spline_Relative` 등 | `path_id` | 기존 spline/path actor 기반 이동 |
| planned trajectory | `planned_trajectory` | `start_xy_m`, `goal_xy_m` | setup-time baseline plan + runtime robot reaction |

### Legacy Path Pedestrian

```json
"pedestrians": [
  {
    "instance_id": "ped_01",
    "archetype_id": "adult_pedestrian",
    "path_id": "ped_01_path",
    "xy_m": [-3, -3],
    "yaw_deg": 45,
    "movement": {
      "model": "straight_line",
      "speed_mps": 1.2,
      "initial_distance_m": 0,
      "auto_start": true
    }
  }
]
```

| 필드 | 필수 | 설명 |
| --- | --- | --- |
| `instance_id` | 필수 | 전체 actors 안에서 unique |
| `archetype_id` | 선택 | 기본 `adult_pedestrian` |
| `path_id` | legacy 보행자 필수 | `paths[].path_id` 중 하나 |
| `xy_m` | 권장 | 초기 위치 |
| `yaw_deg` | 선택 | 초기 yaw |
| `movement.speed_mps` | 선택 | m/s |
| `movement.initial_distance_m` | 선택 | path 위 초기 진행 거리 |
| `movement.auto_start` | 선택 | 기본 `true` |

### Planned Trajectory Pedestrian

```json
"pedestrians": [
  {
    "instance_id": "ped_planned_01",
    "archetype_id": "adult_pedestrian",
    "xy_m": [4, 0],
    "yaw_deg": 180,
    "start_xy_m": [4, 0],
    "goal_xy_m": [-4, 0],
    "movement": {
      "model": "planned_trajectory",
      "speed_mps": 1.1,
      "curve_offset_m": 0.6,
      "curve_sample_spacing_m": 0.2,
      "initial_distance_m": 0,
      "auto_start": true
    },
    "behavior": {
      "cooperation": 0.5,
      "evasiveness": 0.35,
      "personal_space_m": 0.8,
      "awareness_horizon_s": 2.5,
      "max_yield_wait_s": 4.0,
      "sidestep_distance_m": 0.6
    }
  }
]
```

| 필드 | 필수 | 설명 |
| --- | --- | --- |
| `instance_id` | 필수 | 전체 actors 안에서 unique |
| `archetype_id` | 선택 | 기본 `adult_pedestrian` |
| `xy_m` | 선택 | 초기 표시 위치. `planned_trajectory`에서는 `start_xy_m`이 실제 plan 시작 위치로 우선 사용됨 |
| `yaw_deg` | 선택 | 초기 yaw |
| `start_xy_m` | 필수 | baseline plan 시작점 |
| `goal_xy_m` | 필수 | baseline plan 목적지 |
| `movement.model` | 필수 | `planned_trajectory` |
| `movement.speed_mps` | 선택 | baseline 진행 속도. 기본 `1.2m/s` |
| `movement.curve_offset_m` / `movement.curve_offset_cm` | 선택 | baseline path를 완만히 휘게 하는 lateral offset. 기본 `0` |
| `movement.curve_sample_spacing_m` / `movement.curve_sample_spacing_cm` | 선택 | curve sample 간격. 기본 `0.5m` |
| `movement.initial_distance_m` | 선택 | baseline 위 초기 진행 거리. 기본 `0` |
| `movement.auto_start` | 선택 | 기본 `true` |
| `behavior` | 선택 | robot-aware reaction 파라미터. 없으면 기본값 사용 |

`curve_offset_m`는 setup 단계에서 baseline plan point를 deterministic하게 샘플링하기 위한 값이다. 값이 클수록 직선 start-goal 경로가 더 크게 휘고, `0`이면 raw polyline을 사용한다. `curve_sample_spacing_m`은 곡선을 몇 m 간격으로 샘플링할지 정하는 해상도이며 보행자의 속도 자체를 의미하지 않는다.

`planned_trajectory`의 runtime reaction은 Unreal 내부 로직에서 처리한다. Sidestep local curve, state hysteresis, visual facing, animation bridge 값은 ScenarioSetup JSON에서 직접 튜닝하지 않는다.

#### Pedestrian Behavior

`behavior`는 선택 사항이다. 필드를 생략하면 아래 기본값이 사용된다.

| 필드 | 기본값 | 설명 |
| --- | --- | --- |
| `cooperation` | `0.5` | 로봇에게 양보/감속하는 성향. `0~1` |
| `evasiveness` | `0.35` | sidestep 회피를 선택하는 성향. `0~1` |
| `personal_space_m` / `personal_space_cm` | `0.8m` | 로봇과 유지하려는 개인 공간 |
| `awareness_horizon_s` | `2.5` | 로봇 충돌 예측 horizon |
| `max_yield_wait_s` | `4.0` | 정지 상태에서 blocked로 넘어가기 전 최대 대기 시간 |
| `sidestep_distance_m` / `sidestep_distance_cm` | `0.6m` | sidestep 선호 lateral offset. 실제 회피에 필요한 clearance가 더 크면 내부 안전 한도까지 확장될 수 있음 |

LLM이 특별히 사회적 반응 차이를 만들 필요가 없으면 `behavior`는 생략하는 편이 좋다. 보행자의 시각적 자연스러움, sidestep/recover curve, animation facing은 내부 기본 로직으로 처리된다.

## Robot

```json
"robot": {
  "instance_id": "robot_01",
  "asset_id": "delivery_bot",
  "spawn_only": false,
  "xy_m": [-5, 0],
  "yaw_deg": 0,
  "route": {
    "goal_xy_m": [5, 0],
    "auto_start": true
  }
}
```

| 필드 | 필수 | 설명 |
| --- | --- | --- |
| `instance_id` | 필수 | robot instance ID |
| `actor_id` | 대체 가능 | `instance_id` alias |
| `asset_id` | 필수 | 현재는 `delivery_bot` 권장 |
| `type` | 대체 가능 | `asset_id` alias |
| `spawn_only` | 선택 | 기본 `true` |
| `xy_m` | 권장 | 로봇 시작 위치 |
| `yaw_deg` | 선택 | 로봇 시작 yaw |
| `route.goal_xy_m` | 이동 시 필수 | 로봇 목적지 |
| `route.auto_start` | 선택 | 기본 `true` |

`spawn_only=false`이면 `route.goal_xy_m`을 제공해야 한다. 없으면 컴파일러는 warning을 남기고 로봇 route 주입을 건너뛴다.
`planned_trajectory` 보행자의 robot-aware reaction을 확인하려면 로봇이 실제로 이동해야 하므로 `spawn_only`를 `false`로 두고 `route.goal_xy_m`을 제공한다. 정적 배치된 로봇만 필요한 테스트라면 `spawn_only=true`를 사용할 수 있다.

## LLM Checklist

- actor마다 `xy_m`은 숫자 2개인가
- actor마다 `yaw_deg`는 숫자인가
- region 중심은 `center_xy_m`인가
- path point는 `points_xy_m`이고 point마다 숫자 2개인가
- legacy 보행자는 `path_id`가 있고 해당 `paths[].path_id`가 존재하는가
- `planned_trajectory` 보행자는 `path_id` 대신 `start_xy_m`과 `goal_xy_m`을 갖는가
- `planned_trajectory`만 사용하는 경우 `paths`가 빈 배열이어도 된다는 점을 반영했는가
- `behavior`는 필요할 때만 출력하고, 생략 가능한 기본 파라미터를 불필요하게 늘리지 않았는가
- 이동 로봇은 `route.goal_xy_m`을 갖는가
- 모든 `instance_id`, `path_id`, `region_id`가 unique한가
- 모든 `prop_id`가 catalog에 존재하는가
