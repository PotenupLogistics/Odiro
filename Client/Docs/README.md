# OdiroSim Data

이 문서는 합의된 프로젝트 데이터 구조의 메인 인덱스다.
`Docs/Data/` 하위 문서는 데이터 schema별 필드 합의를 기록한다.

## 전체 구조

```sh
%appdata%/OdiroSim/
  templates/                    # 템플릿
    scenarios/                  # 시나리오 템플릿. 에디터와 Agent는 이걸 수정
      <Scenario1>.template.json # 시나리오 구성. 랜덤 요소 넣기 가능
    profiles/                   # 시뮬레이션/로봇 capability profile
      <Profile1>.json           # 실험에 복사될 고정 robot setup profile

  experiments/                  # 실험 구성
    <Experiment1>/              # 폴더로 구분
      setting.json              # 실험 설정. sample 생성, runtime, evaluation 기준
      profile.json              # 로봇 capability profile, 템플릿에서 복사됨

      policy/                   # 행동 정책. 실험마다 수정
        __init__.py             # entrypoint. 지정된 인터페이스로 구현
        <subscript>.py          # 파일 분리 후 __init__.py에서 import 가능
        <config>.json           # policy가 해석하는 설정 파일

      scenarios/
        <000001>.json           # 시나리오 샘플. 템플릿에서 샘플링된 고정 구성
        ...

      runs/                     # 실행 결과
        <000001>/               # 실행할 때 폴더 생성
          policy/               # 해당 실행에 사용된 policy snapshot
          summary.json          # run-level 집계
          review/               # AI 분석 결과
          episodes/             # episode별 결과
            <000001>/
              actions.jsonl     # 로봇의 입출력 기록
              events.jsonl      # 발생 이벤트 snapshot
              trace.jsonl       # 환경 상태 trace
              result.json       # episode 최종 결과
              preview.png       # 대표 이벤트 이미지
              captures/         # 센서 데이터 이미지/파일
```

## 전체 데이터 Schema

| Schema | 경로 | Schema / 형식 | 문서 |
| --- | --- | --- | --- |
| Scenario Template | `templates/scenarios/*.template.json` | `scenario_template` | [Data/templates/scenario-template.md](Data/templates/scenario-template.md) |
| Profile Template | `templates/profiles/*.json` | `simulation_profile` | [Data/templates/profile-template.md](Data/templates/profile-template.md) |
| Experiment Setting | `experiments/<Experiment>/setting.json` | `experiment_setting` | [Data/experiments/setting.md](Data/experiments/setting.md) |
| Experiment Profile | `experiments/<Experiment>/profile.json` | `simulation_profile` | [Data/experiments/profile.md](Data/experiments/profile.md) |
| Policy Package | `experiments/<Experiment>/policy/` | Python package + config | [Data/experiments/policy/policy-package.md](Data/experiments/policy/policy-package.md) |
| Scenario Sample | `experiments/<Experiment>/scenarios/*.json` | `scenario_sample` | [Data/experiments/scenarios/scenario-sample.md](Data/experiments/scenarios/scenario-sample.md) |
| Run Policy Snapshot | `experiments/<Experiment>/runs/<RunId>/policy/` | copied policy package | [Data/experiments/runs/policy-snapshot.md](Data/experiments/runs/policy-snapshot.md) |
| Run Summary | `experiments/<Experiment>/runs/<RunId>/summary.json` | `run_summary` | [Data/experiments/runs/summary.md](Data/experiments/runs/summary.md) |
| AI Review | `experiments/<Experiment>/runs/<RunId>/review/` | 논의중 | [Data/experiments/runs/review.md](Data/experiments/runs/review.md) |
| Episode Result | `experiments/<Experiment>/runs/<RunId>/episodes/<SampleId>/result.json` | `episode_result` | [Data/experiments/runs/episodes/result.md](Data/experiments/runs/episodes/result.md) |
| Episode Events | `experiments/<Experiment>/runs/<RunId>/episodes/<SampleId>/events.jsonl` | `episode_event` | [Data/experiments/runs/episodes/events.md](Data/experiments/runs/episodes/events.md) |
| Robot Actions | `experiments/<Experiment>/runs/<RunId>/episodes/<SampleId>/actions.jsonl` | `robot_action` | [Data/experiments/runs/episodes/actions.md](Data/experiments/runs/episodes/actions.md) |
| Episode Trace | `experiments/<Experiment>/runs/<RunId>/episodes/<SampleId>/trace.jsonl` | 논의중 | [Data/experiments/runs/episodes/trace.md](Data/experiments/runs/episodes/trace.md) |
| Episode Preview | `experiments/<Experiment>/runs/<RunId>/episodes/<SampleId>/preview.png` | PNG artifact | [Data/experiments/runs/episodes/preview.md](Data/experiments/runs/episodes/preview.md) |
| Sensor Captures | `experiments/<Experiment>/runs/<RunId>/episodes/<SampleId>/captures/` | image/data artifacts | [Data/experiments/runs/episodes/captures.md](Data/experiments/runs/episodes/captures.md) |

논의중인 항목도 문서 링크를 유지한다. 팀에서 세부 필드를 확정하면 해당 문서를 갱신한다.

## Schema 상태

### 확정된 v1 schema

| Schema | 상태 |
| --- | --- |
| `scenario_template` | template authoring source |
| `scenario_sample` | template에서 생성된 고정 scenario sample |
| `simulation_profile` | 실험에 고정되는 robot capability/setup profile |
| `experiment_setting` | 실험 sampling/runtime/evaluation 설정 |
| `run_summary` | run-level 집계 |
| `episode_result` | episode terminal result |
| `episode_event` | episode event log |
| `robot_action` | policy `/scenario/decide` 요청/응답 로그 |

### 논의중인 schema surface

| 항목 | 현재 합의 |
| --- | --- |
| Episode Trace | `run_time_seconds` 조인만 확정. line schema는 논의중 |
| AI Review | 저장 위치와 source reference만 확정. report/finding schema는 논의중 |
| Sensor Captures | artifact 위치만 확정. index/manifest 필요 여부는 논의중 |
| Policy Package / Snapshot | 폴더 구조와 snapshot 원칙만 확정. runtime interface와 hash 규칙은 별도 담당 범위 |

## 문서 작성 규칙

`Docs/Data/` 하위 schema 문서는 가능한 한 같은 순서를 사용한다.

1. 경로
2. schema 또는 형식
3. 상태
4. 합의
5. Root 또는 주요 필드
6. Join/source 관계
7. 제외 또는 추후 확정

`Docs/JSON_Guide/` 하위 문서는 AI Agent/LLM이 읽는 작성 guide이므로 예시 JSON, 필드 설명, 출력 체크리스트를 더 자세히 둘 수 있다.

## 필드 명명 규칙

- canonical JSON schema의 모든 필드는 `snake_case`를 사용한다.
- 단위가 있는 필드는 가능하면 suffix를 붙인다. 예: `_m`, `_s`, `_degree`, `_kmh`, `_mps`.
- runtime 내부 API나 enum 값이 PascalCase/camelCase를 쓰더라도 저장되는 JSON field name은 snake_case로 변환한다.

## Templates

`templates/`는 실험 전에 재사용할 수 있는 원본 자산을 둔다.

| 경로 | 문서 | 역할 |
| --- | --- | --- |
| `templates/scenarios/*.template.json` | [Data/templates/scenario-template.md](Data/templates/scenario-template.md) | 시나리오 생성 규칙 |
| `templates/profiles/*.json` | [Data/templates/profile-template.md](Data/templates/profile-template.md) | 로봇 capability/setup profile 원본 |

합의:

- Editor와 LLM은 `scenarios/*.template.json`을 작성/편집한다.
- `profiles/*.json`은 실험 생성 시 `experiments/<Experiment>/profile.json`으로 복사된다.
- profile 값은 실험의 고정 입력이며, 실행마다 randomize하지 않는다.
- surface/prop/pedestrian catalog 같은 환경 해석 정보는 profile에 넣지 않고 [Data/environment-catalog.md](Data/environment-catalog.md) 또는 시스템 프롬프트 입력으로 분리한다.

## Catalog Guides

| 문서 | 역할 |
| --- | --- |
| [Data/environment-catalog.md](Data/environment-catalog.md) | Scenario Template 작성을 위한 surface/prop/pedestrian 어휘 |

## Experiments

`experiments/<Experiment>/`는 한 실험을 실행하기 위한 고정 구성과 실행 결과를 둔다.

| 경로 | 문서 | 역할 |
| --- | --- | --- |
| `experiments/<Experiment>/setting.json` | [Data/experiments/setting.md](Data/experiments/setting.md) | sample 생성, runtime, evaluation 기준 |
| `experiments/<Experiment>/profile.json` | [Data/experiments/profile.md](Data/experiments/profile.md) | 실험에 고정된 profile copy |
| `experiments/<Experiment>/policy/` | [Data/experiments/policy/policy-package.md](Data/experiments/policy/policy-package.md) | 실험에 사용할 행동 정책 |
| `experiments/<Experiment>/scenarios/*.json` | [Data/experiments/scenarios/scenario-sample.md](Data/experiments/scenarios/scenario-sample.md) | 생성된 고정 scenario sample |
| `experiments/<Experiment>/runs/<RunId>/summary.json` | [Data/experiments/runs/summary.md](Data/experiments/runs/summary.md) | run-level 집계 |
| `experiments/<Experiment>/runs/<RunId>/review/` | [Data/experiments/runs/review.md](Data/experiments/runs/review.md) | AI 분석 결과 |
| `experiments/<Experiment>/runs/<RunId>/policy/` | [Data/experiments/runs/policy-snapshot.md](Data/experiments/runs/policy-snapshot.md) | 실행 시점 policy snapshot |
| `experiments/<Experiment>/runs/<RunId>/episodes/<SampleId>/result.json` | [Data/experiments/runs/episodes/result.md](Data/experiments/runs/episodes/result.md) | episode 최종 결과 |
| `experiments/<Experiment>/runs/<RunId>/episodes/<SampleId>/events.jsonl` | [Data/experiments/runs/episodes/events.md](Data/experiments/runs/episodes/events.md) | episode event log |
| `experiments/<Experiment>/runs/<RunId>/episodes/<SampleId>/actions.jsonl` | [Data/experiments/runs/episodes/actions.md](Data/experiments/runs/episodes/actions.md) | robot action log |
| `experiments/<Experiment>/runs/<RunId>/episodes/<SampleId>/trace.jsonl` | [Data/experiments/runs/episodes/trace.md](Data/experiments/runs/episodes/trace.md) | environment trace |
| `experiments/<Experiment>/runs/<RunId>/episodes/<SampleId>/preview.png` | [Data/experiments/runs/episodes/preview.md](Data/experiments/runs/episodes/preview.md) | 대표 이미지 |
| `experiments/<Experiment>/runs/<RunId>/episodes/<SampleId>/captures/` | [Data/experiments/runs/episodes/captures.md](Data/experiments/runs/episodes/captures.md) | sensor capture artifacts |

합의:

- `setting.json`은 sample 생성 조건, runtime 조건, evaluation 기준을 소유한다.
- `profile.json`은 template profile에서 복사된 `simulation_profile` 고정 입력이다.
- `scenarios/*.json`은 template + profile + setting + seed로 생성된다.
- `scenarios/*.json`은 실험 중 고정되어야 한다.
- run 시작 시 `runs/<RunId>/`가 생성된다.

## Runs

`runs/<RunId>/`는 하나의 실행 묶음이다.

```text
runs/<RunId>/
  policy/
  summary.json
  review/
  episodes/
    <SampleId>/
      actions.jsonl
      events.jsonl
      trace.jsonl
      result.json
      preview.png
      captures/
```

합의:

- `summary.json`은 run-level 집계 table이며 원본은 아니다.
- episode 원본 결과는 `episodes/<SampleId>/result.json`, `events.jsonl`, `actions.jsonl`, `trace.jsonl`, `preview.png`, `captures/`에 분산된다.
- `policy/`는 run 시작 시점의 policy snapshot이다.
- `review/`는 run 또는 episode 결과를 읽고 생성한 AI 분석 산출물이다.

## Episodes

`episodes/<SampleId>/`는 scenario sample 하나를 한 번 실행한 결과다.

| 파일 | 문서 | 역할 |
| --- | --- | --- |
| `result.json` | [Data/experiments/runs/episodes/result.md](Data/experiments/runs/episodes/result.md) | episode 최종 결과 |
| `events.jsonl` | [Data/experiments/runs/episodes/events.md](Data/experiments/runs/episodes/events.md) | 의미 있는 사건 로그 |
| `actions.jsonl` | [Data/experiments/runs/episodes/actions.md](Data/experiments/runs/episodes/actions.md) | robot action log |
| `trace.jsonl` | [Data/experiments/runs/episodes/trace.md](Data/experiments/runs/episodes/trace.md) | environment trace |
| `preview.png` | [Data/experiments/runs/episodes/preview.md](Data/experiments/runs/episodes/preview.md) | 대표 이미지 |
| `captures/` | [Data/experiments/runs/episodes/captures.md](Data/experiments/runs/episodes/captures.md) | sensor capture artifacts |

합의:

- `result.json`은 episode 최종 결과다.
- `events.jsonl`은 episode 중 발생한 의미 있는 사건 로그다.
- `actions.jsonl`은 policy `/scenario/decide` 요청/응답 로그다.
- `trace.jsonl`은 `run_time_seconds`로 action/event와 조인하는 환경 trace다.
- 기존 evaluation report의 `events` 배열은 `events.jsonl`로 분리한다.

## 핵심 결정

- sample 생성 수, base seed, evaluation 기준은 `experiments/<Experiment>/setting.json`에 둔다.
- sample의 `sample.source.seed`는 사용된 seed를 기록한다.
- sample의 `sample.source.setting_ref`와 `sample.source.setting_hash`는 생성에 사용된 setting을 기록한다.
- sample의 `scenario.params`는 seed로 확정된 template 범위 값을 기록한다.
- sample의 `scenario.semantic`은 LLM/분석용 의미론 view다.
- `summary.json`은 결과에 대한 요약 통계이며 원본은 아니다.
