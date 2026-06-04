# EpisodeSetup JSON Guide

이 문서는 LLM이 EpisodeSetup JSON을 안정적으로 생성하기 위한 축약 양식을 설명한다. 샘플은 `Json/Input/EpisodeSetupSample.json` 및 `Json/Input/EpisodeSetupSample_*.json`을 기준으로 한다.

EpisodeSetup JSON은 에피소드 실행 정보, 지면 영역, 보행자 경로, 정적 장애물, 보행자, 로봇 배치와 로봇 목적지를 정의한다. DeliveryBot의 주행/센서/정책 튜닝값은 같은 실행 pair의 DeliveryBotSetup JSON에서 정의한다.

## 출력 원칙

- JSON만 출력한다. Markdown 코드블록, 주석, trailing comma는 사용하지 않는다.
- 좌표 단위는 meter로 고정한다. 컴파일러는 Unreal centimeter로 변환한다.
- 각도 단위는 degree로 고정한다.
- actor 배치는 2D 평면 기준이다. `xy_m`만 입력하고 Z는 `0`으로 고정한다.
- actor 회전은 `yaw_deg`만 입력한다. pitch/roll은 `0`으로 고정한다.
- actor scale은 JSON에서 입력하지 않는다. 내부에서 `[1, 1, 1]`로 고정한다.
- `paths`는 spline으로 고정한다. `role`, `type`은 출력하지 않는다.
- `units`는 출력하지 않는다. `distance=m`, `angle=deg`로 고정 해석한다.

## Root

```json
{
  "schema": "episode_actor_spawn_mvp",
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
| `version` | 권장 | EpisodeSetup 버전. 없으면 `1`로 처리 |
| `scenario_id` | 권장 | 에피소드/시나리오 ID |
| `map_id` | 권장 | 대상 맵 ID |
| `run` | 권장 | seed와 시간 제한 |
| `evaluation` | 권장 | 종료 조건과 평가 파라미터 |
| `ground_model` | 권장 | 지면 영역 목록 |
| `paths` | 보행자 사용 시 필수 | 보행자 spline 경로 |
| `actors` | 권장 | 정적 장애물, 보행자, 로봇 |

## Coordinates

| 의미 | 필드 | 입력 | 내부 |
| --- | --- | --- | --- |
| actor 위치 | `xy_m` | `[x, y]` meter | `[x, y, 0]` cm |
| actor yaw | `yaw_deg` | degree | degree |
| region 중심 | `center_xy_m` | `[x, y]` meter | `[x, y, 0]` cm |
| region 크기 | `size_m` | `[x_size, y_size]` meter | cm |
| path point | `points_xy_m` | array of `[x, y]` meter | `[x, y, 0]` cm |
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

```json
"pedestrians": [
  {
    "instance_id": "ped_01",
    "archetype_id": "adult_pedestrian",
    "path_id": "ped_01_path",
    "spawn_time_s": 0,
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
| `path_id` | 필수 | `paths[].path_id` 중 하나 |
| `spawn_time_s` | 선택 | 현재는 기록용, 기본 `0` |
| `xy_m` | 권장 | 초기 위치 |
| `yaw_deg` | 선택 | 초기 yaw |
| `movement.speed_mps` | 선택 | m/s |
| `movement.curve_offset_m` / `movement.curve_offset_cm` | 선택 | `planned_trajectory` baseline curve offset. 기본 `0` |
| `movement.curve_sample_spacing_m` / `movement.curve_sample_spacing_cm` | 선택 | curve sample 간격. 기본 `0.5m` |
| `movement.initial_distance_m` | 선택 | path 위 초기 진행 거리 |
| `movement.auto_start` | 선택 | 기본 `true` |

`movement.model`이 `planned_trajectory`이면 `path_id` 대신 `start_xy_m`과 `goal_xy_m`을 제공한다. `curve_offset_m`가 `0`보다 크면 setup 단계에서 deterministic curve sample point가 생성된다.

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

## LLM Checklist

- `units`를 출력하지 않았는가
- actor마다 `xy_m`은 숫자 2개인가
- actor마다 `yaw_deg`는 숫자인가
- `scale`, `transform`, `location_m`, `rotation_deg`를 출력하지 않았는가
- region 중심은 `center_xy_m`인가
- path point는 `points_xy_m`이고 point마다 숫자 2개인가
- `paths.role`, `paths.type`을 출력하지 않았는가
- `actors.robot.drive`, `actors.robot.path_follow`, `actors.robot.lidar`를 출력하지 않았는가
- 이동 로봇은 `route.goal_xy_m`을 갖는가
- 모든 `instance_id`, `path_id`, `region_id`가 unique한가
- 모든 `prop_id`가 catalog에 존재하는가
