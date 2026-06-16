# Scenario Template

상태: 폐기.

시나리오 템플릿과 시나리오 샘플을 분리하지 않는다.
project에서 사용자가 편집하는 시나리오 계약은 [Project Scenario](../experiments/scenario.md)를 따른다.

이전 경로:

```text
templates/scenarios/<Scenario>.template.json
```

이전 schema:

```json
"scenario_template"
```

마이그레이션:

- template의 `template_id`는 `scenario_id`로 이동한다.
- template의 `intent`, `corridor`, `obstacles`, `pedestrians`, `robot`은 `<UserProject>/scenario.json`으로 이동한다.
- range/choices 값은 `scenario.json`에 그대로 남긴다.
- seed와 episode 수는 `<UserProject>/setting.json`의 `sampling`이 소유한다.
- 확정된 episode 입력은 run 생성 중 `<UserProject>/runs/<RunId>/episodes/<EpisodeId>/scenario.json`에 episode scenario로 저장한다.
