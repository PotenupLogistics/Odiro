# Scenario Sample

상태: 폐기.

시나리오 템플릿과 시나리오 샘플을 별도 사용자 입력으로 나누지 않는다.
사용자가 편집하는 계약은 [Project Scenario](../scenario.md)의 `<UserProject>/scenario.json`이다.

이전 경로:

```text
<LegacyExperiment>/scenarios/<SampleId>.json
```

이전 schema:

```json
"scenario_sample"
```

새 구조:

- `<UserProject>/scenario.json` 하나가 random range/choices를 포함한다.
- `setting.sampling.base_seed`와 `setting.sampling.episode_count`가 episode scenario 생성을 결정한다.
- run 시작 시 snapshot을 만든 뒤, 각 episode마다 `<UserProject>/runs/<RunId>/episodes/<EpisodeId>/scenario.json`을 생성한다.
- 이 episode scenario는 재현성 artifact이며 사용자가 직접 수정하는 입력이 아니다.
