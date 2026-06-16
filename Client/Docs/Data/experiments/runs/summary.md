# Run Summary

경로:

```text
<UserProject>/runs/<RunId>/summary.json
```

schema:

```json
"run_summary"
```

## 합의

- run 전체의 Level 1 table이다.
- 각 row는 episode 하나의 결과 요약이다.
- 원본 데이터가 아니라 빠른 분석/필터링용 집계다.
- 원본은 episode scenario, episode `result.json`, `events.jsonl`이다.

## Root

```json
{
  "schema": "run_summary",
  "version": 1,
  "run": {},
  "rows": []
}
```

## run

| 필드 | 합의 |
| --- | --- |
| `run_id` | run 식별자 |
| `project_id` | project 식별자 |
| `started_at` | 실행 시작 시각 |
| `ended_at` | 실행 종료 시각 |
| `policy_snapshot_hash` | opaque policy snapshot hash |

## rows[]

| 필드 | 합의 |
| --- | --- |
| `episode_id` | episode 식별자 |
| `scenario_id` | scenario 표시 식별자 |
| `scenario_hash` | episode scenario hash |
| `scenario_source_hash` | snapshot scenario hash |
| `profile_hash` | profile hash |
| `setting_hash` | setting hash |
| `seed` | episode seed |
| `outcome` | `Success`, `Failure`, `Cancelled` 등 최종 결과 분류 |
| `terminal_reason` | episode 종료 원인 |
| `duration_s` | 실행 시간 |
| `usable_for_llm_tuning` | 분석/튜닝 근거 사용 가능 여부 |
| `metrics` | 주요 count/distance metric subset |
| `scenario_params` | 핵심 `scenario.params` subset |
| `scenario_semantic` | 핵심 `scenario.semantic.summary` subset |
