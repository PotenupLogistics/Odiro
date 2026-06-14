# Run Summary

경로:

```text
experiments/<Experiment>/runs/<RunId>/summary.json
```

schema:

```json
"run_summary_v1"
```

## 합의

- run 전체의 Level 1 table이다.
- 각 row는 sample 하나의 episode 결과 요약이다.
- 원본 데이터가 아니라 빠른 분석/필터링용 집계다.
- 원본은 scenario sample, episode `result.json`, `events.jsonl`이다.

## Root

```json
{
  "schema": "run_summary_v1",
  "version": 1,
  "run": {},
  "rows": []
}
```

## run

| 필드 | 합의 |
| --- | --- |
| `run_id` | run 식별자 |
| `experiment_id` | experiment 식별자 |
| `started_at` | 실행 시작 시각 |
| `ended_at` | 실행 종료 시각 |
| `policy_snapshot_hash` | opaque policy snapshot hash |

## rows[]

| 필드 | 합의 |
| --- | --- |
| `episode_id` | episode 식별자 |
| `sample_id` | sample 식별자 |
| `scenario_id` | scenario 표시 식별자 |
| `template_id` | 원본 template id |
| `template_hash` | 원본 template hash |
| `profile_hash` | profile hash |
| `setting_hash` | setting hash |
| `seed` | sample seed |
| `outcome` | `Success`, `Failure`, `Cancelled` 등 최종 결과 분류 |
| `terminal_reason` | episode 종료 원인 |
| `duration_s` | 실행 시간 |
| `usable_for_llm_tuning` | 분석/튜닝 근거 사용 가능 여부 |
| `metrics` | 주요 count/distance metric subset |
| `scenario_params` | 핵심 `scenario.params` subset |
| `scenario_semantic` | 핵심 `scenario.semantic.summary` subset |
