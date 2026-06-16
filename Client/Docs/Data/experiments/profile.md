# Project Profile

경로:

```text
<UserProject>/profile.json
```

schema:

```json
"simulation_profile"
```

상태: v1 합의. 사용자 project에 직접 들어가는 fixed input이다.

## 합의

- 사용자가 project 생성 시 직접 작성하거나 기본값에서 복사한다.
- 생성 후에는 해당 project의 고정 입력으로 취급한다.
- 실행마다 값이 달라지면 같은 project의 반복 실행이 아니라 다른 profile을 쓰는 별도 project로 본다.
- Episode scenario의 `source.profile_ref`와 `source.profile_hash`가 이 파일의 run snapshot을 참조한다.
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

## robot

`robot.body`, `robot.drive`, `robot.drive.physics`, `robot.lidar`의 필드 계약은 [Profile Template](../templates/profile-template.md)과 같다.

## episode scenario source와의 관계

Episode scenario는 run snapshot의 profile을 다음 필드로 참조한다.

| field | 의미 |
| --- | --- |
| `source.profile_ref` | `runs/<RunId>/snapshot/profile.json` |
| `source.profile_hash` | snapshot profile의 canonical hash |

## 제외

| 항목 | 이유 |
| --- | --- |
| policy 파일명/config/tuning | `policy/` package가 소유 |
| scenario start/goal | `scenario.json`의 `robot`이 소유 |
| surface/prop/pedestrian catalog | [Environment Catalog](../environment-catalog.md) 또는 시스템 프롬프트 입력으로 분리 |
| LiDAR 전방 판정 snapshot | `actions.jsonl.front_half_angle_degree`가 소유 |
| project-local randomization | profile 값은 project 고정 입력 |

## 추후 확정

| 항목 | 메모 |
| --- | --- |
| profile hash 산정 규칙 | 재현성 검증에 사용할 canonical serialization 규칙 필요 |
