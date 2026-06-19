# summary.json

Run 전체 episode 결과를 빠르게 필터링하고 비교하기 위한 집계 파일이다. 원본 증거는 각 episode의 `scenario.json`, `result.json`, `events.jsonl`이다.

## 경로

```text
runs/<RunId>/summary.json
```

## schema

```json
"run_summary"
```

## Root Fields

| 필드 | 타입 | 필수 | 설명 |
| --- | --- | --- | --- |
| `schema` | string | 예 | 고정값 `run_summary`. |
| `version` | number | 예 | 고정값 `1`. |
| `run` | object | 예 | Run 메타데이터. |
| `rows` | array | 예 | Episode별 결과 요약 row. |

## run

| 필드 | 타입 | 필수 | 설명 |
| --- | --- | --- | --- |
| `run_id` | string | 예 | 6자리 decimal run id. |
| `project_id` | string | 예 | Project 식별자. |
| `started_at` | string | 예 | UTC 실행 시작 시각. |
| `ended_at` | string | 예 | UTC 실행 종료 시각. |
| `policy_snapshot_hash` | string | 예 | 실행 시점 policy snapshot hash. |

## rows[]

| 필드 | 타입 | 필수 | 설명 |
| --- | --- | --- | --- |
| `episode_id` | string | 예 | 6자리 decimal episode id. |
| `scenario_id` | string | 예 | Scenario 표시/조인 식별자. |
| `scenario_hash` | string | 예 | Episode `scenario_sample` content hash. |
| `scenario_source_hash` | string | 예 | Run snapshot `scenario.json` hash. |
| `profile_hash` | string | 예 | Run snapshot `profile.json` hash. |
| `setting_hash` | string | 예 | Run snapshot `setting.json` hash. |
| `seed` | number | 예 | Episode seed. |
| `outcome` | string | 예 | `Success`, `Failure`, `Cancelled` 등 최종 결과 분류. |
| `terminal_reason` | string | 예 | Episode 종료 원인. |
| `duration_s` | number | 예 | Episode 실행 시간. |
| `usable_for_llm_tuning` | boolean | 예 | 분석/튜닝 근거로 사용할 수 있는지 여부. |
| `metrics` | object | 예 | 주요 count/distance metric subset. |
| `scenario_params` | object | 예 | 핵심 `scenario_sample.scenario.params` subset. |
| `scenario_semantic` | object | 예 | 핵심 `scenario_sample.scenario.semantic` subset. |

## 사용 규칙

- `summary.json`은 빠른 검토용 집계이며 source of truth가 아니다.
- Episode 원본은 같은 run 아래 `episodes/<EpisodeId>/scenario.json`, `result.json`, `events.jsonl`에서 확인한다.
- LLM은 summary row로 후보 episode를 좁힌 뒤 원본 파일을 읽고 판단한다.
