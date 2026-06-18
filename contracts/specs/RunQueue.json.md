# RunQueue JSON

상태: legacy input contract.

- 최종 실행 계약에서 사용하지 않음
- 새 실행 입력: `<UserProject>/setting.json`, `profile.json`, `scenario.json`, `policy/`
- 새 episode 입력: `runs/<RunId>/episodes/<EpisodeId>/scenario.json`
- RunQueue adapter 금지
- 기존 reader/writer: project runner 입력 생성으로 대체 후 제거
- 최종 기준: `contracts/specs/user-project-data.md`

# 목적

RunQueue JSON은 기존 시스템에서 여러 EpisodeSetup/DeliveryBotSetup pair를 순서대로 실행하기 위한 입력이었다. 샘플 경로는 `Json/Input/EpisodeRunQueueSample.json`이다. 새 구조에서는 한 project가 하나의 scenario를 가지며, run 생성 시 seed로 episode scenario를 만든다.

# pair 개념

한 번의 실행은 EpisodeSetup JSON 하나와 DeliveryBotSetup JSON 하나의 조합으로 정의된다.

| 필드 | 설명 |
| --- | --- |
| `pair_id` | 사람이 읽고 추적하기 쉬운 pair ID |
| `episode_setup` | 실행할 EpisodeSetup JSON 경로 |
| `delivery_bot_setup` | 실행할 DeliveryBotSetup JSON 경로 |

Runner는 더 이상 기본 DeliveryBotSetup 파일로 fallback하지 않는다. 각 run entry는 `episode_setup`과 `delivery_bot_setup`을 모두 가져야 한다.

# Root 구조

```json
{
  "schema": "episode_run_queue",
  "version": 1,
  "runs": []
}
```

| 필드 | 필수 | 설명 |
| --- | --- | --- |
| `schema` | 권장 | `episode_run_queue` |
| `version` | 권장 | 큐 양식 버전. 초기값 `1` |
| `runs` | 필수 | 실행할 pair 목록. 배열 순서대로 실행 |

# 실행 예시

```json
{
  "schema": "episode_run_queue",
  "version": 1,
  "runs": [
    {
      "pair_id": "sample_0",
      "episode_setup": "Json/Input/EpisodeSetup_narrow_sidewalk_fixed_center_block_000.json",
      "delivery_bot_setup": "Json/Input/DeliveryBotSetup_policy_000_baseline.json"
    },
    {
      "pair_id": "sample_1",
      "episode_setup": "Json/Input/EpisodeSetupSample_1.json",
      "delivery_bot_setup": "Json/Input/DeliveryBotSetupSample_1.json"
    }
  ]
}
```

# 실행 흐름

1. Runner가 RunQueue JSON을 읽는다.
2. `runs` 배열의 순서대로 pair를 하나씩 준비한다.
3. EpisodeSetup JSON을 컴파일해서 월드 배치와 로봇 route를 만든다.
4. DeliveryBotSetup JSON을 컴파일해서 로봇 튜닝값을 만든다.
5. 두 결과를 merge해서 로봇 액터를 초기화한다.
6. 에피소드가 종료되면 다음 pair로 넘어간다.

# 사용 규칙

- `pair_id`는 batch 안에서 unique하게 두는 것을 권장한다.
- 경로는 legacy client project 기준 상대 경로를 사용했다.
- Input JSON은 `Json/Input` 아래에 둔다.
- EvaluationReport 자동 저장이 켜져 있으면 결과는 `Json/Output` 아래에 저장된다.
- pair 하나가 compile/setup에 실패해도 Runner는 기록을 남기고 다음 pair로 넘어갈 수 있다.

# 체크리스트

- `runs`가 비어 있지 않은가
- 모든 entry에 `pair_id`가 있는가
- 모든 entry에 `episode_setup`과 `delivery_bot_setup`이 둘 다 있는가
- 두 경로가 실제 파일을 가리키는가
- EpisodeSetup과 DeliveryBotSetup이 같은 실험 의도를 공유하는 pair인가
- 파일 경로가 `Json/Input/...` 구조를 따르는가
