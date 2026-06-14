# Robot Actions

경로:

```text
experiments/<Experiment>/runs/<RunId>/episodes/<SampleId>/actions.jsonl
```

schema:

```json
"robot_action_v1"
```

## 합의

- JSON Lines 형식이다.
- Python `/scenario/decide` 요청/응답 1회당 1줄을 기록한다.
- 모든 canonical JSON 필드는 `snake_case`를 사용한다.
- `sequence`는 policy 호출 시도 전에 할당한다. 호출이 실패해도 같은 `sequence`로 error line을 남긴다.
- `sequence`는 `events.jsonl.action_sequence`와의 action/event 조인 전용 key다.
- `run_time_seconds`는 episode 실행 시간이며, `trace.jsonl`과의 시간 조인 key다.
- 전역 위치/거리 단위는 meter다. 속도는 km/h, 각도는 degree를 사용한다.
- `target_id`는 Scenario Sample의 `scenario.semantic.static_obstacles[].id` 또는 `scenario.semantic.pedestrians[].id`와 조인되는 semantic id다.

## Line

```json
{
  "schema": "robot_action_v1",
  "version": 1,
  "sequence": 75,
  "run_time_seconds": 12.5,
  "status": "ok",
  "front_half_angle_degree": 20.0,
  "lidar_rays": [
    {
      "hit": true,
      "distance_m": 1.37,
      "ray_index": 61,
      "ray_yaw_degree": 12.0,
      "actor_name": "barrier_01",
      "target_id": "obs_pinch_marker",
      "actor_tags": ["StaticObstacle"]
    }
  ],
  "observed_objects": [
    {
      "actor_name": "barrier_01",
      "target_id": "obs_pinch_marker",
      "actor_tags": ["StaticObstacle"],
      "closest_distance_m": 1.37,
      "closest_ray_yaw_degree": 12.0,
      "total_hit_ray_count": 4,
      "front_hit_ray_count": 2,
      "in_front": true
    }
  ],
  "robot_state": {
    "x": -1.2,
    "y": 0.15,
    "z": 0.115,
    "yaw_degree": 2.0,
    "speed_kmh": 3.2
  },
  "action": {
    "steering": -0.12,
    "target_speed_kmh": 3.8,
    "brake": 0.0,
    "direction": "Forward",
    "selected_policy": "PathFollower",
    "reason": "follow_path"
  },
  "path": {
    "path_status": "valid",
    "path_index": 4,
    "path_length": 22,
    "target_path_index": 5,
    "target_world_point": {
      "x": -2.5,
      "y": 0.5,
      "z": 0.115
    },
    "path_world_points": [
      {
        "x": -5.0,
        "y": 0.0,
        "z": 0.115
      },
      {
        "x": -4.5,
        "y": 0.0,
        "z": 0.115
      }
    ]
  }
}
```

## Root

| 필드 | 타입 | 합의 |
| --- | --- | --- |
| `schema` | string | 고정값 `robot_action_v1` |
| `version` | number | schema version. v1은 `1` |
| `sequence` | number | episode 내 decide/action 순번. 실패 line도 sequence를 유지한다 |
| `run_time_seconds` | number | episode 실행 시간. 단위 s |
| `status` | string | `ok` 또는 `error` |
| `front_half_angle_degree` | number | 전방 관측 영역 판정 half angle. `front_hit_ray_count`, `in_front` 계산 기준. 단위 degree |
| `lidar_rays` | array | 해당 decide 시점의 LiDAR ray 목록 |
| `observed_objects` | array | LiDAR ray를 actor 단위로 묶은 관측 요약 |
| `robot_state` | object | decide 시점의 로봇 상태 |
| `action` | object or null | policy가 반환하고 Unreal이 적용할 행동. 실패 시 `null` |
| `error` | object | `status: "error"`일 때 실패 정보 |
| `path` | object | 해당 decide 시점의 경로 추적 상태 |

## lidar_rays

| 필드 | 타입 | 합의 |
| --- | --- | --- |
| `hit` | boolean | 해당 ray가 actor를 감지했는지 여부 |
| `distance_m` | number | hit 지점 또는 ray 끝까지의 거리. 단위 m |
| `ray_index` | number | ray index |
| `ray_yaw_degree` | number | 로봇 기준 signed local yaw. 단위 degree |
| `actor_name` | string | hit actor 이름. miss면 빈 문자열 |
| `target_id` | string or null | sample semantic id. 조인할 수 없으면 `null` |
| `actor_tags` | string[] | hit actor tag 목록 |

## observed_objects

| 필드 | 타입 | 합의 |
| --- | --- | --- |
| `actor_name` | string | 관측된 actor 이름 |
| `target_id` | string or null | sample semantic id. 조인할 수 없으면 `null` |
| `actor_tags` | string[] | 관측된 actor tag 목록 |
| `closest_distance_m` | number | 해당 actor와의 최단 hit 거리. 단위 m |
| `closest_ray_yaw_degree` | number | 가장 가까운 hit ray yaw. 단위 degree |
| `total_hit_ray_count` | number | 해당 actor를 맞춘 전체 ray 수 |
| `front_hit_ray_count` | number | 전방 영역에서 해당 actor를 맞춘 ray 수 |
| `in_front` | boolean | 해당 actor가 전방 관측 영역에 있는지 여부 |

`front_hit_ray_count`와 `in_front`는 같은 line의 `front_half_angle_degree`를 기준으로 해석한다.

## robot_state

| 필드 | 타입 | 합의 |
| --- | --- | --- |
| `x`, `y`, `z` | number | 로봇 world 위치. 단위 m |
| `yaw_degree` | number | 로봇 yaw. 단위 degree |
| `speed_kmh` | number | 로봇 속도. 단위 km/h |

## action

| 필드 | 타입 | 합의 |
| --- | --- | --- |
| `steering` | number | 조향 입력. `-1.0`부터 `1.0` |
| `target_speed_kmh` | number | 목표 속도. 단위 km/h |
| `brake` | number | 브레이크 입력. `0.0`부터 `1.0` |
| `direction` | string | 진행 방향. 예: `Forward`, `Reverse` |
| `selected_policy` | string | action을 선택한 policy 이름 |
| `reason` | string | policy가 action을 선택한 이유 코드 |

## path

| 필드 | 타입 | 합의 |
| --- | --- | --- |
| `path_status` | string | 경로 상태. 예: `valid`, `empty`, `failed` |
| `path_index` | number | 현재 따라가는 path index |
| `path_length` | number | 현재 path point 개수 |
| `target_path_index` | number | 실제 추종 target index |
| `target_world_point` | object or null | 실제 추종 target world 좌표. 단위 m |
| `path_world_points` | array | 현재 Python policy가 사용하는 전체 path world 좌표 목록. 단위 m |

## Failed Decide Line

Python `/scenario/decide`가 action을 반환하지 못했거나 통신/처리 오류가 발생해도 `actions.jsonl`에는 1줄을 기록한다.

```json
{
  "schema": "robot_action_v1",
  "version": 1,
  "sequence": 80,
  "run_time_seconds": 13.0,
  "status": "error",
  "front_half_angle_degree": 20.0,
  "lidar_rays": [],
  "observed_objects": [],
  "robot_state": {
    "x": -0.9,
    "y": 0.18,
    "z": 0.115,
    "yaw_degree": 3.0,
    "speed_kmh": 2.8
  },
  "action": null,
  "error": {
    "code": "SERVER_UNREACHABLE",
    "message": "Connection refused."
  },
  "path": {
    "path_status": "empty",
    "path_index": 0,
    "path_length": 0,
    "target_path_index": 0,
    "target_world_point": null,
    "path_world_points": []
  }
}
```

정책 실패나 통신 실패는 `actions.jsonl.status: "error"` 원본 line을 남기고, `events.jsonl`에는 `PolicyDecisionError` event를 1회 기록한 뒤 episode를 종료한다.

## Join

| 대상 | key | 합의 |
| --- | --- | --- |
| `events.jsonl` | `events.jsonl.action_sequence == actions.jsonl.sequence` | action/event 조인 |
| `trace.jsonl` | `run_time_seconds` | 시간 기반 조인 |
| `result.json` | episode folder + `sample_id` | episode 최종 결과와 같은 폴더에서 조인 |
