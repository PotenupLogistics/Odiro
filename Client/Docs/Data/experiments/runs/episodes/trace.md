# Episode Trace

경로:

```text
experiments/<Experiment>/runs/<RunId>/episodes/<SampleId>/trace.jsonl
```

schema:

```json
"논의중"
```

상태: `run_time_seconds` 조인 계약만 확정. line schema는 논의중이다.

## 합의

- JSON Lines 형식이다.
- robot이 직접 보지 못한 환경 정보를 분석/리플레이용으로 기록한다.
- `actions.jsonl`과 `events.jsonl`은 `run_time_seconds`로 이 로그와 조인한다.
- `sequence`는 action/event 조인 전용이며, trace join의 기본 key로 사용하지 않는다.
- 전역 위치/거리 단위는 meter다.

## 현재 확정된 Schema

| 항목 | 합의 |
| --- | --- |
| format | JSON Lines |
| purpose | replay, debugging, post-run analysis |
| time key | `run_time_seconds` |
| join source | `actions.jsonl.run_time_seconds`, `events.jsonl.run_time_seconds` |

## 추후 확정

| 항목 | 메모 |
| --- | --- |
| line schema | robot/world/pedestrian/obstacle 상태 표현 |
| capture scope | 모든 frame 기록 여부와 downsample 규칙 |
| privacy/filtering | robot observation과 환경 상태 trace 분리 기준 |
