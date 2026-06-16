# Profile Template

상태: 폐기.

profile template을 별도 template root에서 자동 설치하는 구조는 사용하지 않는다.
사용자가 만든 project는 [Project Profile](../experiments/profile.md)의 `<UserProject>/profile.json`을 직접 가진다.

이전 경로:

```text
templates/profiles/<Profile>.json
```

schema:

```json
"simulation_profile"
```

이전 schema:

```json
"simulation_profile"
```

## 합의

- 최종 입력은 `<UserProject>/profile.json`이다.
- Episode scenario의 `source.profile_ref`와 `source.profile_hash`가 run snapshot 계보를 기록한다.
- profile 값은 project의 고정 입력이다. 실행마다 randomize하지 않는다.
- robot policy 코드와 policy config는 `<UserProject>/policy/`가 소유한다.
- 환경 해석 catalog(surface/prop/pedestrian catalog 등)는 profile에 넣지 않는다. [Environment Catalog](../environment-catalog.md) 또는 시스템 프롬프트 입력으로 분리한다.
- 전역 위치/거리/크기 단위는 meter다.

## Root

```json
{
  "schema": "simulation_profile",
  "version": 1,
  "profile_id": "deliverybot_default",
  "display_name": "Default DeliveryBot",
  "description": "Default DeliveryBot profile.",
  "robot": {
    "body": {},
    "drive": {},
    "lidar": {}
  }
}
```

| 필드 | 타입 | 합의 |
| --- | --- | --- |
| `schema` | string | 고정값 `simulation_profile` |
| `version` | number | schema version. v1은 `1` |
| `profile_id` | string | 사람이 읽고 참조할 수 있는 profile id |
| `display_name` | string | UI 표시명 |
| `description` | string | 선택 설명 |
| `robot` | object | robot capability/setup snapshot |

## robot.body

| 필드 | 타입 | 합의 |
| --- | --- | --- |
| `length_m` | number | robot 전체 길이. 단위 m |
| `width_m` | number | robot 전체 폭. 단위 m |
| `height_m` | number | robot 전체 높이. 단위 m |
| `wheel_base_m` | number | wheel base. 단위 m |
| `turning_radius_m` | number | 최소 회전 반경. 단위 m |

## robot.drive

| 필드 | 타입 | 합의 |
| --- | --- | --- |
| `max_speed_kmh` | number | 전진 최대 속도. 단위 km/h |
| `max_reverse_kmh` | number | 후진 최대 속도. 단위 km/h |
| `accel_kmh_per_s` | number | 전진 가속 기준값 |
| `decel_kmh_per_s` | number | 감속 기준값 |
| `reverse_accel_kmh_per_s` | number | 후진 가속 기준값 |
| `steering_rate_per_s` | number | 조향 입력 변화율 |
| `throttle_rate_per_s` | number | throttle 입력 변화율 |
| `brake_rate_per_s` | number | brake 입력 변화율 |
| `stop_brake` | number | 정지 시 brake 입력 |
| `gear_switch_stop_kmh` | number | 기어 전환을 허용할 정지 판정 속도 |
| `gear_switch_brake` | number | 기어 전환 시 brake 입력 |
| `slowdown_range_kmh` | number | 감속 제어 범위 |
| `speed_tolerance_kmh` | number | 목표 속도 허용 오차 |
| `speed_limit_brake` | number | 속도 제한 초과 시 brake 입력 |
| `use_handbrake` | boolean | handbrake 사용 여부 |
| `physics` | object | low-level engine/vehicle physics 값 |

## robot.drive.physics

| 필드 | 타입 | 합의 |
| --- | --- | --- |
| `max_torque` | number | engine torque 설정 |
| `max_rpm` | number | engine max rpm |
| `idle_rpm` | number | engine idle rpm |
| `engine_brake` | number | engine brake 설정 |
| `rev_up_moi` | number | engine rev up moment of inertia |
| `rev_down_rate` | number | engine rev down rate |

## robot.lidar

| 필드 | 타입 | 합의 |
| --- | --- | --- |
| `mode` | string | LiDAR mode |
| `range_m` | number | LiDAR 감지 거리. 단위 m |
| `angle_step_degree` | number | ray 간 yaw 간격. 단위 degree |
| `height_m` | number | ray 시작 높이. 단위 m |
| `store_missed_rays` | boolean | miss ray도 저장할지 여부 |

## 제외

| 항목 | 이유 |
| --- | --- |
| policy 파일명/config/tuning | `policy/` package가 소유 |
| scenario start/goal | `<UserProject>/scenario.json`의 `robot`이 소유 |
| surface/prop/pedestrian catalog | [Environment Catalog](../environment-catalog.md) 또는 시스템 프롬프트 입력으로 분리 |
| LiDAR 전방 판정 snapshot | `actions.jsonl.front_half_angle_degree`가 소유 |
| randomization range | profile 값은 실험 고정 입력 |

## 추후 확정

| 항목 | 메모 |
| --- | --- |
| profile hash 산정 규칙 | 재현성 검증에 사용할 canonical serialization 규칙 필요 |
