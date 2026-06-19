# analysis_run_response_v2.json

AI run review 결과를 저장하는 JSON artifact다.

## 경로

```text
runs/<RunId>/review/analysis_run_response_v2.json
```

## schema

```json
"analysis_run_response_v2"
```

## 사용 규칙

- `/api/v2/analysis/run` 응답과 같은 JSON을 저장한다.
- 분석 근거는 `project_id`, `run_id`, `episode_id`로 추적 가능해야 한다.
- 주요 근거 파일은 `summary.json`, episode `scenario.json`, episode `result.json`, episode `events.jsonl`이다.
- 추가 report/finding schema와 prompt 기록 방식은 아직 이 문서에서 확정하지 않는다.
