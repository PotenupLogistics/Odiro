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
| `lidar` | object | 예 | LiDAR 관측 mode, ray, scan, point cloud 설정. |

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
| `mass_kg` | number | 예 | Chaos Vehicle chassis mass. 단위 kg. |
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
| `mode` | string | 예 | LiDAR mode. `OneD`, `TwoD`, `ThreeD`, `OneDAndTwoD`, `TwoDAndThreeD`, `All`. |
| `scan_range_m` | number | 예 | LiDAR 감지 거리. 단위 m. |
| `range_m` | number | 예 | LiDAR 감지 거리. 단위 m. |
| `angle_step_degree` | number | 예 | Ray 간 yaw 간격. 단위 degree. |
| `height_m` | number | 예 | Ray 시작 높이. 단위 m. |
| `front_half_angle_degree` | number | 예 | 전방 장애물 판정 half angle. 단위 degree. |
| `vertical_min_degree` | number | 3D mode면 예 | 3D LiDAR 최소 pitch. 단위 degree. |
| `vertical_max_degree` | number | 3D mode면 예 | 3D LiDAR 최대 pitch. 단위 degree. |
| `vertical_step_degree` | number | 3D mode면 예 | 3D LiDAR pitch 간격. 단위 degree. |
| `scan_rate_hz` | number | 예 | LiDAR scan rate. 단위 Hz. |
| `store_missed_rays` | boolean | 예 | Miss ray 저장 여부. |
| `observation_profile` | string | 예 | Python observation profile. 예: `basic`, `realtime_point_cloud`, `quality_point_cloud`. |
| `point_cloud` | object | 예 | 3D LiDAR point cloud capture 설정. |

## robot.lidar.mode

| 값 | 설명 |
| --- | --- |
| `OneD` | 정면 1개 ray. |
| `TwoD` | 수평 yaw scan. |
| `ThreeD` | Yaw/pitch 격자 scan. |
| `OneDAndTwoD` | 1D와 2D scan 함께 사용. |
| `TwoDAndThreeD` | 2D와 3D scan 함께 사용. |
| `All` | 1D, 2D, 3D scan 함께 사용. |

## robot.lidar.point_cloud

| 필드 | 타입 | 필수 | 설명 |
| --- | --- | --- | --- |
| `capture_enabled` | boolean | 예 | Point cloud 파일 저장 여부. |
| `capture_every_n_sensor_frames` | number | `capture_enabled: true`면 예 | 몇 번째 sensor frame마다 저장할지. |
| `range_limit_m` | number | `capture_enabled: true`면 예 | Point cloud 저장 거리 제한. 단위 m. |
| `include_ground_points` | boolean | `capture_enabled: true`면 예 | Ground point 포함 여부. |
| `max_points` | number | `capture_enabled: true`면 예 | Frame당 최대 point 수. |

## 소유 경계

- Profile 값은 project 고정 입력이다.
- Profile 값이 달라지면 별도 project configuration으로 간주한다.
- Robot policy code/config는 `<UserProject>/policy/`가 소유한다.
- Surface, prop, persona, encounter catalog는 `profile.json`에 넣지 않는다.
- Scenario start/goal anchor는 `scenario.json`이 소유한다.
