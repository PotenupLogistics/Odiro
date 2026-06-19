# actions.jsonl

Policy decide 요청/응답 한 번을 한 줄로 기록하는 JSON Lines 파일이다.

## 경로

```text
runs/<RunId>/episodes/<EpisodeId>/actions.jsonl
```

## line schema

```json
"robot_action"
```

## Line Root Fields

| 필드 | 타입 | 필수 | 설명 |
| --- | --- | --- | --- |
| `schema` | string | 예 | 고정값 `robot_action`. |
| `version` | number | 예 | 고정값 `1`. |
| `sequence` | number | 예 | Episode 안의 policy 판단 순번. |
| `run_time_seconds` | number | 예 | Episode 실행 시간. 단위 s. |
| `status` | string | 예 | `ok` 또는 `error`. |
| `front_half_angle_degree` | number | 예 | 전방 관측 영역 half angle. |
| `lidar_rays` | array | 예 | 해당 decide 시점의 LiDAR ray 목록. |
| `observed_objects` | array | 예 | LiDAR ray를 actor 단위로 묶은 관측 요약. |
| `robot_state` | object | 예 | Decide 시점의 robot state. |
| `action` | object or null | 예 | Policy 반환 action. 실패 시 `null`. |
| `error` | object | `status: "error"`일 때 예 | Policy 호출/응답 실패 정보. |
| `path` | object | 예 | Decide 시점의 path 추적 상태. |

## lidar_rays[]

| 필드 | 타입 | 필수 | 설명 |
| --- | --- | --- | --- |
| `hit` | boolean | 예 | 해당 ray가 actor를 감지했는지 여부. |
| `distance_m` | number | 예 | Hit 지점 또는 ray 끝까지의 거리. |
| `ray_index` | number | 예 | Ray index. |
| `ray_yaw_degree` | number | 예 | Robot 기준 signed local yaw. |
| `actor_name` | string | 예 | Hit actor 이름. Miss면 빈 문자열. |
| `target_id` | string or null | 예 | `scenario.semantic`의 target id. 조인할 수 없으면 `null`. |
| `actor_tags` | array | 예 | Hit actor tag 목록. |

## observed_objects[]

| 필드 | 타입 | 필수 | 설명 |
| --- | --- | --- | --- |
| `actor_name` | string | 예 | 관측된 actor 이름. |
| `target_id` | string or null | 예 | `scenario.semantic`의 target id. 조인할 수 없으면 `null`. |
| `actor_tags` | array | 예 | 관측된 actor tag 목록. |
| `closest_distance_m` | number | 예 | 해당 actor와의 최단 hit 거리. |
| `closest_ray_yaw_degree` | number | 예 | 가장 가까운 hit ray yaw. |
| `total_hit_ray_count` | number | 예 | 해당 actor를 맞춘 전체 ray 수. |
| `front_hit_ray_count` | number | 예 | 전방 영역에서 해당 actor를 맞춘 ray 수. |
| `in_front` | boolean | 예 | 해당 actor가 전방 관측 영역에 있는지 여부. |

`front_hit_ray_count`와 `in_front`는 같은 line의 `front_half_angle_degree`를 기준으로 해석한다.

## robot_state

| 필드 | 타입 | 필수 | 설명 |
| --- | --- | --- | --- |
| `x` | number | 예 | Robot world X 위치. 단위 m. |
| `y` | number | 예 | Robot world Y 위치. 단위 m. |
| `z` | number | 예 | Robot world Z 위치. 단위 m. |
| `yaw_degree` | number | 예 | Robot yaw. 단위 degree. |
| `speed_kmh` | number | 예 | Robot 속도. 단위 km/h. |

## action

| 필드 | 타입 | 필수 | 설명 |
| --- | --- | --- | --- |
| `steering` | number | 예 | 조향 입력. `-1.0`부터 `1.0`. |
| `target_speed_kmh` | number | 예 | 목표 속도. 단위 km/h. |
| `brake` | number | 예 | Brake 입력. `0.0`부터 `1.0`. |
| `direction` | string | 예 | 진행 방향. 예: `Forward`, `Reverse`. |
| `selected_policy` | string | 권장 | Action을 선택한 policy 이름. |
| `reason` | string | 권장 | Action 선택 이유 코드. |

## error

| 필드 | 타입 | 필수 | 설명 |
| --- | --- | --- | --- |
| `code` | string | 권장 | 기계가 읽기 좋은 오류 코드. |
| `message` | string | 예 | 사람이 읽는 오류 설명. |
| `http_status` | number | 통신 오류 시 권장 | Policy server HTTP status. |
| `response_status` | string | 응답이 있으면 권장 | Policy response의 status 값. |

## path

| 필드 | 타입 | 필수 | 설명 |
| --- | --- | --- | --- |
| `path_status` | string | 예 | 경로 상태. 예: `valid`, `empty`, `failed`. |
| `path_index` | number | 예 | 현재 따라가는 path index. |
| `path_length` | number | 예 | 현재 path point 개수. |
| `target_path_index` | number | 예 | 실제 추종 target index. |
| `target_world_point` | object or null | 예 | 실제 추종 target world 좌표. |
| `path_world_points` | array | 예 | 현재 Python policy가 사용하는 path world 좌표 목록. |

## Join Rules

| 대상 | Key | 설명 |
| --- | --- | --- |
| `events.jsonl` | `events.jsonl.action_sequence == actions.jsonl.sequence` | Action/event 조인. |
| `trace.jsonl` | `run_time_seconds` | 시간 기반 조인. |
| `result.json` | Episode directory | 같은 episode의 최종 결과. |

Policy 호출 실패도 같은 `sequence`로 `status: "error"` line을 남긴다. 같은 실패는 `events.jsonl`에 `PolicyDecisionError` event로도 기록한다.
