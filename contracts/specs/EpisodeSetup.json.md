# EpisodeSetup JSON Guide

상태: legacy Client input guide.

- 최종 사용자 project 계약 아님
- 새 scenario 기준: `contracts/specs/user-project-data.md`의 `scenario.json`
- 새 episode 입력 기준: `scenario_sample`
- 새 writer는 `EpisodeSetup` JSON을 만들지 않음
- 이 문서는 기존 compiler와 field 출처 확인용

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
  "robot_profile": {
    "profile_id": "delivery_bot_alpha",
    "width_m": 0.44,
    "depth_m": 1.0,
    "height_m": 0.64,
    "footprint_shape": "box",
    "safety_margin_m": 0.2,
    "min_passable_width_m": 0.84
  },
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
| `robot_profile` | 선택/additive | 서버 기본 로봇 실측 크기와 통과 폭 제약 |
| `ground_model` | 권장 | 지면 영역 목록 |
| `paths` | 보행자 사용 시 필수 | 보행자 spline 경로 |
| `actors` | 권장 | 정적 장애물, 보행자, 로봇 |

## RobotProfile

`robot_profile`은 서버 기본값으로 주입되는 additive root field다. public scenario generation API request에서 사용자가 로봇 크기를 직접 넘기지 않는다. 현재 기본 profile은 `delivery_bot_alpha`이며 실측 W/D/H는 `0.44m / 1.00m / 0.64m`로 해석한다.

```json
"robot_profile": {
  "profile_id": "delivery_bot_alpha",
  "width_m": 0.44,
  "depth_m": 1.0,
  "height_m": 0.64,
  "footprint_shape": "box",
  "safety_margin_m": 0.2,
  "min_passable_width_m": 0.84
}
```

`min_passable_width_m`은 `width_m + safety_margin_m * 2`로 계산한 `0.84m`이다. backend는 이 값을 시나리오 생성/검증 단계에서 보도 폭, 장애물 배치 후 남은 gap, robot spawn/goal과 blocked region 사이의 여유 판단에 사용한다. UE collision box가 실제 충돌 판정을 담당하더라도, AI 생성 단계에서는 이 profile로 비현실적인 passable 경로 생성을 줄인다.

UE 파서가 아직 `robot_profile`을 소비하지 않아도 기존 실행에는 영향을 주지 않아야 한다. 이 필드는 기존 `actors.robot`, `ground_model`, `paths`, `actors.static_obstacles`, `actors.pedestrians` 구조를 변경하지 않는다. 다만 UE 쪽 collision box와 실제 크기 W/D/H가 일치하는지는 별도 확인이 필요하다.

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
  "default_region_type": "blocked",
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
    "yaw_deg": 0,
    "properties": {
      "passability": "passable"
    }
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

`properties`는 shallow 확장 map이다. backend는 장애물의 `blocking_ratio`와 보도 폭을 기준으로 `properties.passability`를 `"passable"` 또는 `"blocked_path"`로 추가할 수 있다. UE가 이 값을 사용하지 않으면 무시해도 되며, 기존 장애물 배치 필드인 `instance_id`, `prop_id`, `xy_m`, `yaw_deg`는 그대로 유지된다.

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
| `movement.initial_distance_m` | 선택 | path 위 초기 진행 거리 |
| `movement.auto_start` | 선택 | 기본 `true` |

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
