# OdiroSim Data

이 문서는 사용자 project 데이터 구조의 메인 인덱스다.
`Docs/Data/` 하위 문서는 데이터 schema별 필드 합의를 기록한다.

## 전체 구조

```sh
<UserProject>/
  setting.json                   # episode count, base seed, runtime, evaluation 기준
  profile.json                   # 로봇 capability profile
  scenario.json                  # 사용자가 편집하는 단일 random scenario

  policy/
    __init__.py
    <subscript>.py
    <config>.json

  runs/
    <RunId>/
      snapshot/
        setting.json
        profile.json
        scenario.json
        policy/
      summary.json
      review/
      episodes/
        <EpisodeId>/
          scenario.json          # snapshot scenario + seed로 확정한 episode scenario
          actions.jsonl
          events.jsonl
          trace.jsonl
          result.json
          preview.png
          captures/
```

## 전체 데이터 Schema

| Schema | 경로 | Schema / 형식 | 문서 |
| --- | --- | --- | --- |
| Project Setting | `<UserProject>/setting.json` | `project_setting` | [Data/experiments/setting.md](Data/experiments/setting.md) |
| Project Profile | `<UserProject>/profile.json` | `simulation_profile` | [Data/experiments/profile.md](Data/experiments/profile.md) |
| Project Scenario | `<UserProject>/scenario.json` | `scenario` | [Data/experiments/scenario.md](Data/experiments/scenario.md) |
| Policy Package | `<UserProject>/policy/` | Python package + config | [Data/experiments/policy/policy-package.md](Data/experiments/policy/policy-package.md) |
| Run Policy Snapshot | `<UserProject>/runs/<RunId>/snapshot/policy/` | copied policy package | [Data/experiments/runs/policy-snapshot.md](Data/experiments/runs/policy-snapshot.md) |
| Run Summary | `<UserProject>/runs/<RunId>/summary.json` | `run_summary` | [Data/experiments/runs/summary.md](Data/experiments/runs/summary.md) |
| AI Review | `<UserProject>/runs/<RunId>/review/` | 논의중 | [Data/experiments/runs/review.md](Data/experiments/runs/review.md) |
| Episode Scenario | `<UserProject>/runs/<RunId>/episodes/<EpisodeId>/scenario.json` | `episode_scenario` | [Data/experiments/scenario.md](Data/experiments/scenario.md) |
| Episode Result | `<UserProject>/runs/<RunId>/episodes/<EpisodeId>/result.json` | `episode_result` | [Data/experiments/runs/episodes/result.md](Data/experiments/runs/episodes/result.md) |
| Episode Events | `<UserProject>/runs/<RunId>/episodes/<EpisodeId>/events.jsonl` | `episode_event` | [Data/experiments/runs/episodes/events.md](Data/experiments/runs/episodes/events.md) |
| Robot Actions | `<UserProject>/runs/<RunId>/episodes/<EpisodeId>/actions.jsonl` | `robot_action` | [Data/experiments/runs/episodes/actions.md](Data/experiments/runs/episodes/actions.md) |
| Episode Trace | `<UserProject>/runs/<RunId>/episodes/<EpisodeId>/trace.jsonl` | 논의중 | [Data/experiments/runs/episodes/trace.md](Data/experiments/runs/episodes/trace.md) |
| Episode Preview | `<UserProject>/runs/<RunId>/episodes/<EpisodeId>/preview.png` | PNG artifact | [Data/experiments/runs/episodes/preview.md](Data/experiments/runs/episodes/preview.md) |
| Sensor Captures | `<UserProject>/runs/<RunId>/episodes/<EpisodeId>/captures/` | image/data artifacts | [Data/experiments/runs/episodes/captures.md](Data/experiments/runs/episodes/captures.md) |

폐기 문서:

| 문서 | 상태 |
| --- | --- |
| [Data/templates/scenario-template.md](Data/templates/scenario-template.md) | scenario template split 폐기 안내 |
| [Data/experiments/scenarios/scenario-sample.md](Data/experiments/scenarios/scenario-sample.md) | scenario sample split 폐기 안내 |
| [Data/templates/profile-template.md](Data/templates/profile-template.md) | project 외부 profile template 폐기 안내 |

## Schema 상태

확정된 v1 schema:

| Schema | 상태 |
| --- | --- |
| `scenario` | 사용자가 편집하는 project-local scenario source |
| `episode_scenario` | run/episode 생성 시 seed로 확정한 실행 입력 |
| `simulation_profile` | project에 고정되는 robot capability/setup profile |
| `project_setting` | episode scenario 생성/runtime/evaluation 설정 |
| `run_summary` | run-level 집계 |
| `episode_result` | episode terminal result |
| `episode_event` | episode event log |
| `robot_action` | policy `/scenario/decide` 요청/응답 로그 |

논의중인 schema surface:

| 항목 | 현재 합의 |
| --- | --- |
| Episode Trace | `run_time_seconds` 조인만 확정. line schema는 논의중 |
| AI Review | 저장 위치와 source reference만 확정. report/finding schema는 논의중 |
| Sensor Captures | artifact 위치만 확정. index/manifest 필요 여부는 논의중 |
| Policy Package / Snapshot | 폴더 구조와 snapshot 원칙만 확정. runtime interface와 hash 규칙은 별도 담당 범위 |

## 핵심 결정

- 사용자는 project root를 직접 생성한다.
- 한 project에는 사용자가 편집하는 `scenario.json` 하나만 둔다.
- `scenario.json`은 고정값과 random range/choices를 모두 담을 수 있다.
- episode 수, base seed, evaluation 기준은 `<UserProject>/setting.json`에 둔다.
- run 시작 시 `setting.json`, `profile.json`, `scenario.json`, `policy/`를 snapshot한다.
- 각 episode는 snapshot scenario와 deterministic seed로 `episodes/<EpisodeId>/scenario.json`을 생성한다.
- episode scenario는 실행 입력/재현성 artifact이며 사용자가 직접 수정하지 않는다.
- `summary.json`은 결과에 대한 요약 통계이며 원본은 아니다.
