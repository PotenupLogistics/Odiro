# profile.json

Project 단위 robot capability/setup snapshot이다. Run 시작 시 snapshot으로 복사된다.

## 경로

```text
<UserProject>/profile.json
runs/<RunId>/snapshot/profile.json
```

## schema

```json
"simulation_profile"
```

## Root Fields

| 필드 | 타입 | 필수 | 설명 |
| --- | --- | --- | --- |
| `schema` | string | 예 | 고정값 `simulation_profile`. |
| `version` | number | 예 | 고정값 `1`. |
| `profile_id` | string | 예 | Profile 식별자. |
| `display_name` | string | 예 | UI 표시명. |
| `description` | string | 예 | Profile 설명. |
| `robot` | object | 예 | Robot body, drive, LiDAR 설정. |

## robot

| 필드 | 타입 | 필수 | 설명 |
| --- | --- | --- | --- |
| `body` | object | 예 | Robot 물리 크기. |
| `drive` | object | 예 | 속도, 가속, 조향, brake, engine 설정. |
| `lidar` | object | 예 | LiDAR 관측 mode와 ray 설정. |

## robot.body

| 필드 | 타입 | 필수 | 설명 |
| --- | --- | --- | --- |
| `length_m` | number | 예 | Robot 전체 길이. 단위 m. |
| `width_m` | number | 예 | Robot 전체 폭. 단위 m. |
| `height_m` | number | 예 | Robot 전체 높이. 단위 m. |
| `wheel_base_m` | number | 예 | Wheel base. 단위 m. |
| `turning_radius_m` | number | 예 | 최소 회전 반경. 단위 m. |

## robot.drive

| 필드 | 타입 | 필수 | 설명 |
| --- | --- | --- | --- |
| `max_speed_kmh` | number | 예 | 전진 최대 속도. 단위 km/h. |
| `max_reverse_kmh` | number | 예 | 후진 최대 속도. 단위 km/h. |
| `accel_kmh_per_s` | number | 예 | 전진 가속 기준값. |
| `decel_kmh_per_s` | number | 예 | 감속 기준값. |
| `reverse_accel_kmh_per_s` | number | 예 | 후진 가속 기준값. |
| `steering_rate_per_s` | number | 예 | 조향 입력 변화율. |
| `throttle_rate_per_s` | number | 예 | Throttle 입력 변화율. |
| `brake_rate_per_s` | number | 예 | Brake 입력 변화율. |
| `stop_brake` | number | 예 | 정지 시 brake 입력. |
| `gear_switch_stop_kmh` | number | 예 | 기어 전환을 허용할 정지 판정 속도. |
| `gear_switch_brake` | number | 예 | 기어 전환 시 brake 입력. |
| `slowdown_range_kmh` | number | 예 | 감속 제어 범위. |
| `speed_tolerance_kmh` | number | 예 | 목표 속도 허용 오차. |
| `speed_limit_brake` | number | 예 | 속도 제한 초과 시 brake 입력. |
| `use_handbrake` | boolean | 예 | Handbrake 사용 여부. |
| `max_torque` | number | 예 | Engine torque 설정. |
| `max_rpm` | number | 예 | Engine max RPM. |
| `idle_rpm` | number | 예 | Engine idle RPM. |
| `engine_brake` | number | 예 | Engine brake 설정. |
| `rev_up_moi` | number | 예 | Engine rev-up moment of inertia. |
| `rev_down_rate` | number | 예 | Engine rev-down rate. |

`robot.drive.physics`는 v1 field가 아니다. Engine 값은 `robot.drive` 바로 아래에 둔다.

## robot.lidar

| 필드 | 타입 | 필수 | 설명 |
| --- | --- | --- | --- |
| `mode` | string | 예 | LiDAR mode. |
| `range_m` | number | 예 | LiDAR 감지 거리. 단위 m. |
| `angle_step_degree` | number | 예 | Ray 간 yaw 간격. 단위 degree. |
| `height_m` | number | 예 | Ray 시작 높이. 단위 m. |
| `store_missed_rays` | boolean | 예 | Miss ray 저장 여부. |

## 소유 경계

- Profile 값은 project 고정 입력이다.
- Profile 값이 달라지면 별도 project configuration으로 간주한다.
- Robot policy code/config는 `<UserProject>/policy/`가 소유한다.
- Surface, prop, persona, encounter catalog는 `profile.json`에 넣지 않는다.
- Scenario start/goal anchor는 `scenario.json`이 소유한다.
