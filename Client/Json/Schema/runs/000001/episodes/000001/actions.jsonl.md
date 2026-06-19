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

## 공통 규칙

- JSON Lines 형식이다.
- Python `/scenario/decide` 요청/응답 1회당 1줄을 기록한다.
- 모든 canonical JSON 필드는 `snake_case`를 사용한다.
- `sequence`는 policy 호출 시도 전에 할당한다. 호출이 실패해도 같은 `sequence`로 error line을 남긴다.
- `sequence`는 `events.jsonl.action_sequence`와의 action/event 조인 전용 key다.
- `run_time_seconds`는 episode 실행 시간이며 `trace.jsonl`과의 시간 조인 key다.
- 위치와 거리는 meter를 기본 단위로 사용한다. `_cm` suffix가 붙은 필드만 centimeter 단위다.
- 속도는 km/h, 각도는 degree를 사용한다.
- `target_id`는 `scenario.semantic.static_obstacles[].id` 또는 `scenario.semantic.pedestrians[].id`와 조인되는 semantic id다. 조인할 수 없으면 `null`을 사용한다.

## Line Root Fields

| 필드 | 타입 | 필수 | 설명 |
| --- | --- | --- | --- |
| `schema` | string | 예 | 고정값 `robot_action`. |
| `version` | number | 예 | 고정값 `1`. |
| `sequence` | number | 예 | Episode 안의 policy 판단 순번. |
| `run_time_seconds` | number | 예 | Episode 실행 시간. 단위 s. |
| `sensor_sequence` | number | 예 | LiDAR snapshot 순번. |
| `sensor_time_seconds` | number | 예 | LiDAR snapshot 생성 시간. 단위 s. |
| `status` | string | 예 | `ok` 또는 `error`. |
| `front_half_angle_degree` | number | 예 | 전방 관측 영역 판정 half angle. `front_hit_ray_count`, `in_front` 계산 기준. |
| `lidar` | object | 예 | 1D/2D/3D로 분리된 LiDAR 입력. |
| `observed_objects` | array | 예 | LiDAR ray를 target 단위로 묶은 관측 요약. |
| `robot_state` | object | 예 | Decide 시점의 robot state. |
| `action` | object or null | 예 | Policy가 반환하고 Unreal이 적용할 action. 실패 시 `null`. |
| `decision` | object or null | 예 | Action을 선택한 policy metadata. 실패 시 `null`. |
| `error` | object | `status: "error"`일 때 예 | Policy 호출/응답 실패 정보. |
| `path` | object | 예 | Decide 시점의 path 추적 상태. |

## lidar

| 필드 | 타입 | 필수 | 설명 |
| --- | --- | --- | --- |
| `mode` | string | 예 | LiDAR mode. `OneD`, `TwoD`, `ThreeD`, `OneDAndTwoD`, `TwoDAndThreeD`, `All`. |
| `rays_1d` | array | 예 | 정면 1개 ray 기반 입력. |
| `rays_2d` | array | 예 | 수평 scan 입력. |
| `rays_3d` | array | 예 | Yaw/pitch 격자 scan 입력. |
| `policy_ray_selection` | object | 예 | Python policy가 실제로 사용한 LiDAR 입력 요약. |

## rays_1d[]

| 필드 | 타입 | 필수 | 설명 |
| --- | --- | --- | --- |
| `hit` | boolean | 예 | 해당 ray가 target을 감지했는지 여부. |
| `distance_m` | number | 예 | Hit 지점 또는 ray 끝까지의 거리. |
| `ray_index` | number or null | 예 | Ray index. 단일 ray에 index가 없으면 `null`. |
| `target_id` | string or null | 예 | 감지된 semantic target id. Miss 또는 조인 실패면 `null`. |
| `target_tags` | array | 예 | 감지된 target tag 목록. |

## rays_2d[]

| 필드 | 타입 | 필수 | 설명 |
| --- | --- | --- | --- |
| `hit` | boolean | 예 | 해당 ray가 target을 감지했는지 여부. |
| `distance_m` | number | 예 | Hit 지점 또는 ray 끝까지의 거리. |
| `ray_index` | number or null | 예 | Ray index. |
| `yaw_degree` | number | 예 | Robot 기준 signed local yaw. |
| `target_id` | string or null | 예 | 감지된 semantic target id. Miss 또는 조인 실패면 `null`. |
| `target_tags` | array | 예 | 감지된 target tag 목록. |

## rays_3d[]

| 필드 | 타입 | 필수 | 설명 |
| --- | --- | --- | --- |
| `hit` | boolean | 예 | 해당 ray가 target을 감지했는지 여부. |
| `distance_m` | number | 예 | Hit 지점 또는 ray 끝까지의 거리. |
| `ray_index` | number or null | 예 | Ray index. |
| `yaw_degree` | number | 예 | Robot 기준 signed local yaw. |
| `pitch_degree` | number | 예 | Robot 기준 local pitch. |
| `hit_location_cm` | object or null | 예 | Unreal world hit 위치. Miss면 `null`. |
| `target_id` | string or null | 예 | 감지된 semantic target id. Miss 또는 조인 실패면 `null`. |
| `target_tags` | array | 예 | 감지된 target tag 목록. |

## policy_ray_selection

| 필드 | 타입 | 필수 | 설명 |
| --- | --- | --- | --- |
| `mode` | string | 예 | Policy 입력으로 선택된 family. `1d`, `2d`, `3d`, `legacy2d`, `none`. |
| `source` | string | 예 | 선택 source. 예: `lidar.rays_2d`, `lidar.rays_3d.nearest_vertical_by_yaw`. |
| `ray_count` | number | 예 | Policy에 전달된 ray 개수. |
| `horizontal_pitch_degree` | number or null | 예 | 3D를 2D policy ray로 투영할 때 기준이 된 pitch. 해당 없으면 `null`. |

## observed_objects[]

| 필드 | 타입 | 필수 | 설명 |
| --- | --- | --- | --- |
| `target_id` | string or null | 예 | 관측된 semantic target id. 조인할 수 없으면 `null`. |
| `target_tags` | array | 예 | 관측된 target tag 목록. |
| `has_bounds` | boolean | 예 | Target bounds가 유효한지 여부. |
| `bounds_origin_cm` | object or null | 예 | Target bounds origin. 유효하지 않으면 `null`. |
| `bounds_extent_cm` | object or null | 예 | Target bounds extent. 유효하지 않으면 `null`. |
| `closest_hit_location_cm` | object or null | 예 | 해당 target과 가장 가까운 hit 위치. 없으면 `null`. |
| `closest_distance_m` | number | 예 | 해당 target과의 최단 hit 거리. |
| `closest_ray_yaw_degree` | number | 예 | 가장 가까운 hit ray yaw. |
| `total_hit_ray_count` | number | 예 | 해당 target을 맞춘 전체 ray 수. |
| `front_hit_ray_count` | number | 예 | 전방 영역에서 해당 target을 맞춘 ray 수. |
| `in_front` | boolean | 예 | 해당 target이 전방 관측 영역에 있는지 여부. |

`front_hit_ray_count`와 `in_front`는 같은 line의 `front_half_angle_degree`를 기준으로 해석한다.

## robot_state

| 필드 | 타입 | 필수 | 설명 |
| --- | --- | --- | --- |
| `x` | number | 예 | Robot world X 위치. 단위 m. |
| `y` | number | 예 | Robot world Y 위치. 단위 m. |
| `z` | number | 예 | Robot world Z 위치. 단위 m. |
| `yaw_degree` | number | 예 | Robot yaw. 단위 degree. |
| `speed_kmh` | number | 예 | Robot 속도. 단위 km/h. |
| `colliding` | boolean | 예 | Collision stop 상태 여부. |
| `collision_target_id` | string or null | 예 | Collision stop을 만든 semantic target id. 없으면 `null`. |
| `collision_target_tags` | array | 예 | Collision target tag 목록. |

## action

| 필드 | 타입 | 필수 | 설명 |
| --- | --- | --- | --- |
| `steering` | number | 예 | 조향 입력. `-1.0`부터 `1.0`. |
| `target_speed_kmh` | number | 예 | 목표 속도. 단위 km/h. |
| `brake` | number | 예 | Brake 입력. `0.0`부터 `1.0`. |
| `direction` | string | 예 | 진행 방향. 예: `Forward`, `Reverse`. |

## decision

| 필드 | 타입 | 필수 | 설명 |
| --- | --- | --- | --- |
| `selected_policy` | string | 예 | Action을 선택한 Python policy 이름. |
| `reason` | string | 예 | Policy가 action을 선택한 이유 코드. |

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
