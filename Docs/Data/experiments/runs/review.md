# AI Review

경로:

```text
experiments/<Experiment>/runs/<RunId>/review/
```

schema:

```json
"논의중"
```

상태: 분석 artifact surface만 확정. report/finding schema는 논의중이다.

## 합의

- run 또는 episode 결과를 읽고 생성한 AI 분석 산출물을 둔다.
- 원본 데이터가 아니라 분석 결과다.
- 분석 근거가 된 `summary.json`, `result.json`, `events.jsonl`, scenario sample을 추적할 수 있어야 한다.

## 현재 확정된 Schema

| 항목 | 합의 |
| --- | --- |
| 위치 | `runs/<RunId>/review/` |
| 성격 | AI-generated analysis artifact |
| source reference | 분석에 사용한 run/episode/scenario 참조 필요 |

## 추후 확정

| 항목 | 메모 |
| --- | --- |
| report schema | run-level/episode-level report 형식 |
| finding schema | 문제 원인, 근거, 권고안 표현 |
| prompt metadata | model, prompt, 생성 시각, 입력 hash 기록 방식 |
