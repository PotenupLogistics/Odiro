# Project Scenario

경로:

```text
<UserProject>/scenario.json
```

schema:

```json
"scenario"
```

## 역할

Project Scenario는 사용자가 project에서 편집하는 단일 시나리오 입력이다.
이전 `Scenario Template`과 `Scenario Sample`을 별도 파일로 나누지 않는다.

합의:

- 한 project에는 `scenario.json` 하나만 둔다.
- 고정값과 랜덤 range/choices를 같은 파일에 기록한다.
- seed, episode 수, generator version은 `setting.json`의 `sampling`이 소유한다.
- run 시작 시 `runs/<RunId>/snapshot/scenario.json`으로 복사한다.
- 각 episode 실행 전 snapshot scenario와 seed로 `episodes/<EpisodeId>/scenario.json`을 확정한다.
- episode scenario는 실행 입력/재현성 artifact이며 사용자가 편집하는 입력이 아니다.
- Unreal 실행용 actor/world payload는 저장하지 않고 preview/run 시점에 파생한다.

## Root

```json
{
  "schema": "scenario",
  "version": 1,
  "scenario_id": "pinch_oncoming_low_coop",
  "intent": "협폭 구간에서 대향 보행자와 조우할 때 로봇이 안전하게 통과하는지 검증한다.",
  "corridor": {},
  "obstacles": {},
  "pedestrians": {},
  "robot": {}
}
```

| 필드 | 필수 | 합의 |
| --- | --- | --- |
| `schema` | 필수 | 고정값 `scenario` |
| `version` | 필수 | schema version. v1은 `1` |
| `scenario_id` | 필수 | 사람이 읽을 수 있는 snake_case 식별자 |
| `intent` | 필수 | 이 scenario가 검증하려는 상황/가설 |
| `corridor` | 필수 | 맵의 공간 skeleton과 lane/surface 구성 |
| `obstacles` | 권장 | 정적 장애물 배치 규칙 |
| `pedestrians` | 권장 | 배경 보행자 수와 설계된 encounter |
| `robot` | 필수 | 로봇 시작/목적지 anchor |

## 랜덤 값

고정값:

```json
"walkway_width_m": 3.0
```

범위값:

```json
"walkway_width_m": { "min": 2.5, "max": 4.0 }
```

선택값:

```json
"replaced_by": { "choices": ["grass", "road"] }
```

범위 또는 선택값은 episode scenario 생성 시 seed로 하나의 값으로 확정한다.
확정 결과는 `episodes/<EpisodeId>/scenario.json`의 `params`와 `semantic`에 기록한다.

## Episode Scenario

경로:

```text
<UserProject>/runs/<RunId>/episodes/<EpisodeId>/scenario.json
```

schema:

```json
"episode_scenario"
```

역할:

- 해당 episode가 실제로 사용한 확정 입력이다.
- source scenario, setting, profile hash와 episode seed를 기록한다.
- `actions.jsonl`, `events.jsonl`, `trace.jsonl`, `result.json`, `summary.json`이 참조하는 semantic id의 기준이다.
- 사용자가 직접 수정하지 않는다.

권장 root:

```json
{
  "schema": "episode_scenario",
  "version": 1,
  "episode": {
    "episode_id": "000001",
    "seed": 3007
  },
  "source": {
    "scenario_ref": "runs/000001/snapshot/scenario.json",
    "scenario_hash": "sha256:scenariohash0001",
    "profile_ref": "runs/000001/snapshot/profile.json",
    "profile_hash": "sha256:profilehash0001",
    "setting_ref": "runs/000001/snapshot/setting.json",
    "setting_hash": "sha256:settinghash0001",
    "generator_version": "0.1.0"
  },
  "params": {},
  "semantic": {},
  "validation": {}
}
```

## 다른 Schema와의 경계

| 필드/영역 | 합의 |
| --- | --- |
| seed/count | `<UserProject>/setting.json`의 `sampling` 소유 |
| profile/robot capability | `<UserProject>/profile.json` 소유 |
| policy 설정 | `<UserProject>/policy/` 소유 |
| catalog(surface/prop/persona) | [Environment Catalog](../environment-catalog.md) 또는 시스템 프롬프트 입력 |
| 실행 payload | scenario에 저장하지 않고 preview/run 시점에 파생 |

## 검증

Scenario는 저장 시 검증하고, episode scenario는 생성 직후 검증한다.

| 등급 | 대응 |
| --- | --- |
| `error` | 저장 또는 episode 생성 중단 |
| `warning` | 생성 계속, episode scenario의 `validation.diagnostics`에 기록 |
| `repair` | 보정 후 생성, 보정 사실을 `validation.diagnostics`에 기록 |

주요 검증 규칙:

- `segments[].id`, `placements[].id`, `encounters[].id`는 각각 unique해야 한다.
- placement, encounter, robot이 참조하는 segment는 존재해야 한다.
- `corridor_pose.along_m`은 참조 segment의 `along_range_m` 안에 있어야 한다.
- `allow_blocking` 없이 `min_clear_width_m` 계약을 깨면 error다.
