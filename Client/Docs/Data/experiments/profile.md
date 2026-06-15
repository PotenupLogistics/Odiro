# Experiment Profile

경로:

```text
experiments/<Experiment>/profile.json
```

schema:

```json
"simulation_profile"
```

상태: v1 합의. template profile에서 복사된 experiment-local fixed input이다.

## 합의

- `templates/profiles/<Profile>.json`에서 복사된다.
- 실험 생성 후에는 해당 experiment의 고정 입력으로 취급한다.
- 실행마다 값이 달라지면 같은 실험의 반복 실행이 아니라 다른 profile을 쓰는 별도 실험으로 본다.
- Scenario Sample의 `sample.source.profile_ref`와 `sample.source.profile_hash`가 이 파일을 참조한다.
- robot policy 코드와 policy config는 `experiments/<Experiment>/policy/`가 소유한다.
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
  "source": {
    "template_ref": "templates/profiles/deliverybot_default.json",
    "template_hash": "sha256:profiletemplatehash0001",
    "copied_at": "2026-06-14T00:00:00Z"
  },
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
| `source` | object | template profile에서 복사된 계보 정보 |
| `robot` | object | robot capability/setup snapshot |

## source

| 필드 | 타입 | 합의 |
| --- | --- | --- |
| `template_ref` | string | 복사 원본 `templates/profiles/<Profile>.json` 경로 |
| `template_hash` | string | 복사 원본 profile hash |
| `copied_at` | string | 복사 시각. ISO 8601 |

## robot

`robot.body`, `robot.drive`, `robot.drive.physics`, `robot.lidar`의 필드 계약은 [Profile Template](../templates/profile-template.md)과 같다.

## sample source와의 관계

Scenario Sample은 이 파일을 다음 필드로 참조한다.

| sample field | 의미 |
| --- | --- |
| `sample.source.profile_ref` | `experiments/<Experiment>/profile.json` |
| `sample.source.profile_hash` | 이 파일의 canonical hash |

## 제외

| 항목 | 이유 |
| --- | --- |
| policy 파일명/config/tuning | `policy/` package가 소유 |
| scenario start/goal | Scenario Template/Sample의 `robot`이 소유 |
| surface/prop/pedestrian catalog | [Environment Catalog](../environment-catalog.md) 또는 시스템 프롬프트 입력으로 분리 |
| LiDAR 전방 판정 snapshot | `actions.jsonl.front_half_angle_degree`가 소유 |
| experiment-local randomization | profile 값은 실험 고정 입력 |

## 추후 확정

| 항목 | 메모 |
| --- | --- |
| profile hash 산정 규칙 | 재현성 검증에 사용할 canonical serialization 규칙 필요 |
| source 필드 필수 여부 | template에서 복사되지 않고 직접 생성된 profile을 허용할지 결정 필요 |
