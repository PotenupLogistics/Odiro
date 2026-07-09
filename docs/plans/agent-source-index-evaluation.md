---
date: 2026-07-09
status: draft
---

# Source Indexing이 Agentic Coding 성능을 높일 수 있을까?

## 요약

Agentic Coding 수행 시 AI는 가장 먼저 관련 소스코드를 찾아 프로젝트의 상태를 이해하는 과정이 필요하다.
이 탐색 단계가 길거나 부정확하면 토큰 사용량과 이해도가 떨어져 변경 범위가 부적절하거나 구현 품질이
떨어질 수 있다.
이 문제를 줄이기 위해 각 주제 영역과 관련된 소스코드, 주의할 점, 검증 방법을 요약한 인덱스를 자동 생성하도록 했다.

이 방법이 실제로 효과가 있는지 정량 평가를 수행한다.
인덱스를 적용한 프로젝트의 commit 내역을 토대로, 인덱스를 적용했을 때와 적용하지 않았을 때의 탐색 비용, 파일 읽기 적중률, 구현 성공률을 비교한다.
또한 인덱스가 협업 과정에서 어떤 문제가 발생할 수 있고, 잘못 설계되었을 때 어떤 문제가 발생하는지도 분석한다.

## 1. 서론

### 1.1 문제 배경

바이브 코딩이 유행하고 있다.

세간의 평가를 보면 취미 개발이나 프로토타이핑에는 충분히 도움이 되지만,
실제 제품 개발에서는 구현 품질이 만족스럽지 않아 사람이 직접 구현하는 것 대비 효율이 떨어진다는 평가가 많다.

좀 더 깊게 살펴보면, 프로젝트가 커지고 복잡해질수록 AI의 성능이 떨어진다는 간증이 많다.
AI가 작업을 구현하기 위해서는 기존 프로젝트의 상태를 먼저 이해해야 하는데,
작업과 관련된 소스코드와 문서를 찾는 과정에서 다음과 같은 문제가 발생한다. (레퍼런스 필요)

1. 작업과 무관한 파일을 읽느라 토큰과 시간을 낭비한다.  
   비용 문제 뿐만 아니라, 모델의 컨텍스트를 과도하게 소모하면서 추론에 필요한 컨텍스트가 부족해 성능이 하락하게 된다.

2. 관련된 파일을 놓쳐 정보 부족으로 인해 잘못된 설계를 한다.  
   구현 시 지켜야 할 작업 규칙을 놓치거나, 기존 구현과 충돌하는 설계를 하게 된다. 이미 중복된 기능이 존재하는데도 모르고 새로 구현하는 경우도 있다.

3. Scope 판단을 잘못하여 모듈화가 훼손된다.  
   광범위한 범위를 건드리면서 작업 내용이 분산되고, 불필요한 의존성을 만들거나 흐름 추적을 어렵게 만든다.

### 1.2 Source Index — 탐색을 돕는 메타데이터

많은 회사는 문서화를 통해 이러한 문제를 해결하려고 한다.
각 영역별로 관련된 작업 규칙, 주의할 점, 검증 방법을 문서화하면 해당 문서를 먼저 읽음으로서 탐색 범위를 좁힐 수 있다.

그러나 문서화에는 그 나름의 한계가 있다.

- 구현이 변경되었을 때 문서를 업데이트하지 않으면 stale 문서가 되어 잘못된 정보를 제공한다. 이는 오히려 잘못된 방향으로 에이전트를 이끌 수 있다.

- 문서화는 사람이 읽는 것을 전제로 한다. 사람과 달리 AI는 컨텍스트 한계가 작기 때문에 불필요한 설명이 많으면 성능이 저하될 수 있다.

이 문제를 해결하기 위해, 각 영역별로 관련된 소스코드와 문서, 작업 규칙, 주의할 점, 검증 방법을 요약한 내용을 AI가 알아서 업데이트하도록 했고, 이것을 Source Index라고 부르기로 했다.

Source Index는 사람이 읽는 용도가 아닌 오롯이 AI의 탐색을 돕기 위한 메타데이터로, AI가 작업을 시작할 때 최소한의 정보를 제공해 탐색 범위를 좁히고, 구현 변경 시 바로 내용을 업데이트하여 stale 정보가 발생하지 않도록 유도한다.

## 2. 적용 예시

### 2.1 프로젝트 개요

4명의 개발자가 참여한 한 달짜리 프로젝트에 해당 방법론을 적용해봤다.

해당 프로젝트는 Unreal Engine 5 프로젝트와 AI Agent를 동작시키는 Python 서버를 포함하는 monorepo의 형태를 띄고 있다.

Source Index는 다음과 같이 `.agents/index` 폴더에 저장했다.

```text
.agents/index/
  README.md
  cards/
    agent-context.yaml
    root-dev-workflow.yaml
    bridge-host.yaml
    client-runtime-foundation.yaml
    ...
```

`README.md`에 index의 목적과 구조를 정의하고, YAML card로 각 주제에 대해 다음 정보를 제공하도록 했다.

| 필드          | 의미                                                    |
| ------------- | ------------------------------------------------------- |
| `description` | card가 다룰 주제                                        |
| `paths`       | 이 card가 적용될 file path 패턴                         |
| `entry`       | 세부 주제와 함께 우선적으로 읽을 파일 제시              |
| `guardrails`  | 해당 주제 작업 시 지켜야 할 규칙, 주의점, workflow 설명 |
| `verify`      | 변경 유형별 최소 검증 방법                              |
| `links`       | canonical docs/specs                                    |
| `related`     | cross-boundary 작업에서 같이 볼 card                    |

의도한 사용 흐름은 단순하다. 에이전트는 먼저 `.agents/index/README.md`를 읽고, task와 관련된 card만 고른 뒤,
그 card의 `entry` 순서대로 파일을 연다. 이후 필요할 때만 `links`와 `related`를 따라간다.

### 1.3 왜 정량 평가가 필요한가

좋은 source index는 체감상 편해 보일 수 있지만, 실제 효과는 별도로 확인해야 한다. index가 잘 관리되면 탐색을
줄일 수 있지만, stale path나 과도하게 넓은 card는 오히려 잘못된 방향으로 에이전트를 이끌 수 있다.

따라서 평가는 다음을 동시에 봐야 한다.

- 탐색 비용이 줄었는가?
- 관련 파일을 더 잘 찾았는가?
- 불필요한 파일 읽기가 줄었는가?
- 검증 선택이 좋아졌는가?
- 구현 성공률이나 변경 품질이 떨어지지 않았는가?
- 실패가 발생했다면 어떤 card 설계 문제 때문인가?

이 실험은 `.agents/index`를 유지할지, 줄일지, 확장할지 결정하기 위한 근거를 제공한다. 또한 card split/merge,
stale path 정리, verify 보강, source sanity 자동화 같은 후속 작업의 우선순위를 정하는 데 사용한다.

### 1.4 실험 아이디어 요약

실험은 과거 commit을 “이미 정답이 있는 과제”로 재사용한다. `target_commit`의 직전 상태를 `base_commit`으로
checkout하고, commit 메시지와 diff에서 복원한 작업 설명을 에이전트에게 준다. 단, 에이전트에게 after diff나
정답 파일 목록은 보여주지 않는다.

같은 과제를 두 조건으로 실행한다.

- A: `.agents/index` 사용 금지
- B: `.agents/index` 사용

이후 실제 `target_commit`의 변경 파일과 사람이 작성한 gold label을 기준으로 run 결과를 채점한다.

### 1.5 본 문서의 성격

이 문서는 실제 실험 전 작성하는 설계 초안이다. 아직 결과를 주장하지 않는다. 대신 실험 질문, 표본 선정 기준,
조건, 실행 절차, metric, 결과 기록 양식을 사전에 고정해 실험 후 해석 편향을 줄인다.

## 2. Method

### 2.1 Research Questions

| ID  | 질문                                                 | 판단 기준                                            |
| --- | ---------------------------------------------------- | ---------------------------------------------------- |
| Q1  | `.agents/index`가 첫 관련 파일 도달 시간을 줄이는가? | `time_to_first_relevant_file` 감소                   |
| Q2  | 관련 파일을 더 빠짐없이 읽는가?                      | `relevant_file_recall` 증가                          |
| Q3  | 불필요한 파일 읽기를 줄이는가?                       | `irrelevant_file_reads` 감소                         |
| Q4  | 적절한 검증 명령을 더 잘 고르는가?                   | `verification_recall`, `verification_precision` 증가 |
| Q5  | 구현 결과 품질을 떨어뜨리지 않는가?                  | `success_rate`, `scope_precision` 유지 또는 증가     |
| Q6  | stale card나 모호한 card가 실패를 유발하는가?        | `misroute_count`, `stale_index_failure_count`        |

### 2.2 Hypotheses

| ID  | 가설                                                                      | 성공 기준 초안             |
| --- | ------------------------------------------------------------------------- | -------------------------- |
| H1  | index 사용 조건은 첫 관련 파일 도달 시간을 줄인다.                        | median 30% 이상 감소       |
| H2  | index 사용 조건은 불필요한 파일 읽기를 줄인다.                            | median 25% 이상 감소       |
| H3  | index 사용 조건은 검증 선택 정확도를 높인다.                              | verification F1 증가       |
| H4  | index 사용 조건은 구현 성공률을 낮추지 않는다.                            | 성공률 동등 또는 증가      |
| H5  | 실패 사례는 특정 card의 stale path, 과도한 entry, 누락 verify에 집중된다. | card별 개선 항목 도출 가능 |

성공 기준 수치는 pilot 이후 조정할 수 있다. 단, 구현 성공률이 유의하게 하락하면 탐색 비용 개선이 있어도
index 품질을 충분하다고 보지 않는다.

### 2.3 Experimental Unit

기본 실험 단위는 Git commit before/after 쌍이다.

| 이름            | 의미                                                     |
| --------------- | -------------------------------------------------------- |
| `base_commit`   | 과제를 시작하는 상태. 보통 `target_commit^`              |
| `target_commit` | 실제 변경이 반영된 commit                                |
| `task_prompt`   | 에이전트에게 줄 과제 설명. after diff를 노출하지 않는다. |
| `gold`          | after diff, 관련 파일, 기대 card, 기대 검증, 수용 기준   |
| `run`           | 한 과제를 한 조건에서 한 번 수행한 결과                  |

merge commit은 기본적으로 제외한다. 필요 시 first-parent 기준으로 별도 표기한다.

### 2.4 Commit Pair Selection

포함 기준:

- `base_commit`에서 checkout, 탐색, 최소 검증이 가능하다.
- commit 메시지나 주변 이슈로 과제를 재구성할 수 있다.
- 변경 범위가 한 에이전트 run으로 수행 가능한 크기다.
- source, tests, docs, config, asset metadata 중 최소 하나를 의미 있게 변경한다.
- after diff가 gold labeling에 쓸 수 있을 만큼 명확하다.

제외 기준:

- binary asset만 변경한 commit
- 순수 formatting, lockfile, generated output만 변경한 commit
- 대규모 migration처럼 대체 정답이 지나치게 넓은 commit
- 외부 서비스, secret, production data가 필요한 commit
- `base_commit` 자체가 build/test 불능이고 과제와 무관한 이유로 실패하는 commit
- 현재 index를 replay할 수 없을 만큼 경로 구조가 크게 다른 오래된 commit

영역별 편향을 줄이기 위해 card ownership 기준으로 층화한다.

| 영역                           | 목표 비율 | 예시 card                                        |
| ------------------------------ | --------: | ------------------------------------------------ |
| Root tooling / CI              |    10-15% | `root-dev-workflow`                              |
| Agent context                  |    10-15% | `agent-context`                                  |
| Agents runtime / RAG / harness |    15-20% | `agents-*`                                       |
| Bridge                         |    10-15% | `bridge-host`                                    |
| Client runtime foundation      |    15-20% | `client-runtime-foundation`                      |
| Client platform / simulation   |    15-20% | `client-platform-execution`, `client-simulation` |
| Contracts / shared data        |     5-10% | `contracts-shared-data`                          |
| Icon / asset workflow          |     5-10% | `icon-assets`                                    |

Pilot은 10개 과제, 본 실험은 30-50개 과제를 목표로 한다.

### 2.5 Index Snapshot Policy

before/after commit 실험에서 어떤 index를 사용할지 분리한다.

Primary condition은 Current Index Replay다. 현재 `.agents/index`의 실효성을 보려면 `base_commit` worktree에
현재 index를 overlay한다.

장점:

- 현재 index 품질을 평가한다.
- 동일한 index로 여러 과거 과제를 비교할 수 있다.

주의:

- 오래된 commit에는 현재 index path가 없을 수 있다.
- 이런 경우 `index_path_missing`으로 기록하고 표본 제외 또는 stale risk로 별도 분석한다.

Secondary condition은 Historical Index Replay다. `base_commit`에 존재하던 `.agents/index`를 그대로 사용한다.

장점:

- 당시 실제 agent context 품질을 평가한다.

주의:

- index가 없던 시기의 commit은 비교가 불가능하다.
- 현재 index 개선 효과와 혼동될 수 있다.

본 실험의 기본 결론은 Current Index Replay 기준으로 작성한다. Historical Index Replay는 추세 분석이나 보조
근거로만 사용한다.

### 2.6 Gold Label

각 과제마다 사람이 `gold`를 작성한다. after diff는 labeling에만 사용하고 agent prompt에는 포함하지 않는다.

필수 label:

| 필드                     | 설명                                     |
| ------------------------ | ---------------------------------------- |
| `task_id`                | 안정적인 과제 ID                         |
| `base_commit`            | 시작 commit                              |
| `target_commit`          | 정답 commit                              |
| `prompt`                 | agent에게 줄 과제 설명                   |
| `prompt_quality`         | commit 정보만으로 task가 얼마나 명확한지 |
| `expected_cards`         | 읽어야 할 index card                     |
| `required_files`         | 이해 또는 수정에 사실상 필요한 파일      |
| `allowed_related_files`  | 읽어도 타당한 주변 파일                  |
| `expected_changed_files` | after diff 기준 변경 파일                |
| `expected_verification`  | 기대 검증 명령 또는 검증 종류            |
| `acceptance_criteria`    | 구현 성공 판정 기준                      |
| `exclusion_notes`        | 제외 또는 주의 사유                      |

Label 기준:

- `required_files`는 after diff 파일만 뜻하지 않는다. 변경하지 않았어도 이해에 필수인 entry point, test,
  schema, config를 포함한다.
- `allowed_related_files`는 관련 탐색으로 인정하지만 recall 계산의 분모에는 넣지 않는다.
- `expected_verification`은 commit에서 실제 수행된 명령보다 “이 repo에서 최소로 필요한 검증”을 기준으로 한다.
- 정답 구현이 여러 개 가능한 과제는 after diff 일치율보다 acceptance criteria를 우선한다.

### 2.7 Conditions

| 조건 | 이름        | 설명                                                          |
| ---- | ----------- | ------------------------------------------------------------- |
| A    | No Index    | `.agents/index`를 읽지 않고 일반 source navigation만 사용     |
| B    | Index       | `.agents/index/README.md`와 관련 card를 사용                  |
| C    | Stale Index | 일부 path 또는 verify가 오래된 index snapshot 사용. 선택 실험 |
| D    | Oracle      | 사람이 정한 관련 파일 목록 제공. 상한선 측정용 선택 실험      |

필수 비교는 A/B다. C/D는 pilot 이후 필요할 때 추가한다.

### 2.8 Execution Modes

Navigation-only 모드는 순수 탐색 품질을 측정한다.

허용:

- 파일 읽기
- 검색
- git metadata 조회
- 계획과 검증 후보 제출

금지:

- 파일 수정
- 테스트 실행이 긴 명령
- after diff 조회

출력 형식:

```json
{
  "understood_task": "...",
  "selected_cards": ["..."],
  "relevant_files": ["..."],
  "planned_changes": ["..."],
  "verification_plan": ["..."],
  "risks": ["..."]
}
```

Implementation 모드는 탐색 이득이 실제 변경 품질로 이어지는지 확인한다.

허용:

- 파일 수정
- 최소 관련 test/lint/build/runtime check
- self-review diff

금지:

- commit, push, publish
- after diff 조회
- gold label 조회

결과는 after diff와 직접 비교하되, 최종 성공 판정은 acceptance criteria와 검증 결과를 우선한다.

### 2.9 Instrumentation

각 run에서 다음 raw event를 기록한다.

| 이벤트       | 예시                                           |
| ------------ | ---------------------------------------------- |
| file read    | `Get-Content`, `rg --files`, editor read       |
| search       | `rg`, `git grep`, path glob                    |
| command      | test, lint, build, parse check                 |
| edit         | changed path, added/deleted lines              |
| timing       | start, first relevant file, first plan, finish |
| token        | input/output/tool token, 가능할 때만           |
| final answer | 계획, 검증, 리스크                             |

파일 읽기 판정은 path 단위로 계산한다. `rg` 결과는 “읽기”가 아니라 “검색”으로 별도 계산하고, 검색 결과에서
특정 파일을 열었을 때 file read로 기록한다.

### 2.10 Metrics

Navigation metric:

| 지표                                | 정의                                           | 방향          |
| ----------------------------------- | ---------------------------------------------- | ------------- |
| `time_to_first_relevant_file`       | run 시작부터 첫 `required_files` read까지 시간 | 낮을수록 좋음 |
| `tool_calls_to_first_relevant_file` | 첫 관련 파일까지 tool call 수                  | 낮을수록 좋음 |
| `relevant_file_recall`              | 읽은 `required_files` / 전체 `required_files`  | 높을수록 좋음 |
| `file_read_precision`               | 읽은 관련 파일 / 전체 읽은 파일                | 높을수록 좋음 |
| `irrelevant_file_reads`             | required/allowed가 아닌 파일 read 수           | 낮을수록 좋음 |
| `selected_card_recall`              | 선택한 card 중 gold card 포함률                | 높을수록 좋음 |
| `misroute_count`                    | 잘못된 card 또는 영역으로 인해 낭비된 탐색 수  | 낮을수록 좋음 |

Verification metric:

| 지표                            | 정의                                | 방향          |
| ------------------------------- | ----------------------------------- | ------------- |
| `verification_recall`           | 기대 검증 중 선택 또는 실행한 비율  | 높을수록 좋음 |
| `verification_precision`        | 선택한 검증 중 과제에 적절한 비율   | 높을수록 좋음 |
| `verification_execution_rate`   | 계획이 아니라 실제 실행한 검증 비율 | 높을수록 좋음 |
| `verification_failure_handling` | 실패 원인 보고와 후속 판단의 적절성 | 높을수록 좋음 |

Implementation metric:

| 지표                     | 정의                                  | 방향          |
| ------------------------ | ------------------------------------- | ------------- |
| `success_rate`           | acceptance criteria 통과 비율         | 높을수록 좋음 |
| `changed_file_recall`    | expected changed files 중 수정한 비율 | 참고          |
| `scope_precision`        | 수정 파일 중 gold/allowed 범위 비율   | 높을수록 좋음 |
| `test_pass_rate`         | 기대 검증 성공 비율                   | 높을수록 좋음 |
| `human_review_score`     | reviewer 0-2점 판정                   | 높을수록 좋음 |
| `unrelated_change_count` | 과제 외 변경 수                       | 낮을수록 좋음 |

### 2.11 Analysis Plan

기본 단위는 같은 task의 A/B paired comparison이다.

- median, mean, p25/p75를 함께 보고한다.
- task별 A/B 차이를 계산한 뒤 bootstrap confidence interval을 산출한다.
- 성공률 같은 binary metric은 paired proportion 차이로 본다.
- 반복 run이 있으면 task를 random effect로 두고 조건 효과를 별도 계산한다.
- 전체 평균 외에 card 영역별, 난이도별, 변경 유형별 subgroup을 본다.

권장 반복:

- navigation-only: task당 조건별 3회
- implementation: task당 조건별 1-2회

판정:

- navigation metric이 개선되어도 implementation 성공률이 하락하면 “부분 효과”로 분류한다.
- 특정 card에서만 악화되면 전체 index 실패가 아니라 card 개선 과제로 분리한다.

### 2.12 Procedure

Phase 0: 준비

- 현재 `.agents/index` snapshot 저장
- 후보 commit 목록 생성
- 제외 기준 적용
- 영역별 표본 선정
- task/gold label 초안 작성

완료 산출물:

- `tasks.yaml`
- `gold/*.yaml`
- index snapshot

Phase 1: Pilot

- 10개 task 선정
- navigation-only A/B 각 1회 실행
- metric 산출 가능 여부 확인
- prompt leakage, logging 누락, gold 모호성 수정
- 성공 기준 수치 조정

완료 산출물:

- pilot 결과표
- 수정된 runner/log schema
- 제외 또는 보류 task 목록

Phase 2: 본 실험

- 30-50개 task 실행
- navigation-only 조건별 3회 반복
- 대표 task implementation 실행
- run 순서 randomization
- 실패 run은 원인 분류만 하고 임의 재시도하지 않는다.

완료 산출물:

- raw run logs
- scored metrics
- 실패 사례 분류

Phase 3: 결과 검토

- metric aggregate 산출
- card별 성능과 실패 사례 분석
- stale path, ambiguous card, missing verify 식별
- index 개선 PR 후보 작성

완료 산출물:

- 최종 보고서
- 개선 backlog
- 재평가 기준

### 2.13 Experiment Tooling Plan

실험 도구는 root tooling 아래 별도 폴더로 둘 수 있다.

예상 구조:

```text
tools/agent-index-eval/
  README.md
  select-commits.ps1
  prepare-worktree.ps1
  run-navigation.ps1
  run-implementation.ps1
  score-runs.ps1
  schemas/
    task.schema.json
    gold.schema.json
    run.schema.json
  data/
    tasks.yaml
    gold/
    runs/
    reports/
```

도구 책임:

| 파일                     | 책임                                               |
| ------------------------ | -------------------------------------------------- |
| `select-commits.ps1`     | 후보 commit 추출, 변경 파일 통계 생성              |
| `prepare-worktree.ps1`   | `base_commit` worktree 생성, current index overlay |
| `run-navigation.ps1`     | 조건별 navigation-only prompt 실행                 |
| `run-implementation.ps1` | 조건별 구현 실험 실행                              |
| `score-runs.ps1`         | run log와 gold label 비교, metric 산출             |

초기 구현은 navigation-only만 자동화한다. implementation 자동화는 pilot 후 runner 안정성을 확인하고 추가한다.

### 2.14 Prompt Template

공통 prompt:

```text
You are evaluating a repository task from a historical base commit.
Do not inspect the target commit or any after diff.
Use only the working tree, allowed git metadata, and the instructions below.

Task:
<TASK_PROMPT>

Return the required JSON object.
```

No Index 조건:

```text
Do not read .agents/index or any file under it.
Use normal repository search and local documentation instead.
```

Index 조건:

```text
Follow the repository rule for .agents/index:
start from .agents/index/README.md, scan cards, and open only matching cards.
```

Navigation-only 출력:

```json
{
  "selected_cards": [],
  "files_to_read_or_already_read": [],
  "planned_change_files": [],
  "verification_plan": [],
  "confidence": "low|medium|high",
  "risks": []
}
```

### 2.15 Validity Threats

| 위협                           | 영향                     | 완화                                        |
| ------------------------------ | ------------------------ | ------------------------------------------- |
| after diff leakage             | 결과 과대평가            | prompt와 runner에서 target commit 조회 금지 |
| commit 메시지 부족             | 과제 재구성 왜곡         | gold 작성 시 prompt 품질 등급 기록          |
| 모델 출력 변동성               | 조건 차이 불안정         | 조건별 반복, paired comparison              |
| current index overlay mismatch | 과거 code와 index 불일치 | missing path 기록, 오래된 commit 제외       |
| base commit 자체 문제          | 구현 실패 오판           | baseline checkout/build smoke               |
| 정답이 여러 개인 과제          | diff 일치율 왜곡         | acceptance criteria 우선                    |
| human label bias               | gold 편향                | label review, 불확실 label 표시             |

## 3. Result

이 섹션은 실제 실험 후 채운다. 현재는 결과 기록 양식만 정의한다.

### 3.1 Executive Summary

| 항목                    | 결과 |
| ----------------------- | ---- |
| 실험 과제 수            | TODO |
| 반복 수                 | TODO |
| 탐색 비용 개선          | TODO |
| 관련 파일 발견률 변화   | TODO |
| 불필요한 파일 읽기 변화 | TODO |
| 검증 선택 정확도 변화   | TODO |
| 구현 성공률 변화        | TODO |
| 주요 개선 대상 card     | TODO |

### 3.2 Overall Results

| Metric                        | No Index | Index | Delta | 판단 |
| ----------------------------- | -------: | ----: | ----: | ---- |
| `time_to_first_relevant_file` |     TODO |  TODO |  TODO | TODO |
| `relevant_file_recall`        |     TODO |  TODO |  TODO | TODO |
| `file_read_precision`         |     TODO |  TODO |  TODO | TODO |
| `irrelevant_file_reads`       |     TODO |  TODO |  TODO | TODO |
| `verification_f1`             |     TODO |  TODO |  TODO | TODO |
| `success_rate`                |     TODO |  TODO |  TODO | TODO |

### 3.3 Results by Card

| Card                        | Task 수 | 개선 | 악화 | 주요 원인 |
| --------------------------- | ------: | ---: | ---: | --------- |
| `agent-context`             |    TODO | TODO | TODO | TODO      |
| `root-dev-workflow`         |    TODO | TODO | TODO | TODO      |
| `bridge-host`               |    TODO | TODO | TODO | TODO      |
| `client-runtime-foundation` |    TODO | TODO | TODO | TODO      |

### 3.4 Failure Cases

| Task | 조건 | 실패 유형 | 설명 | 개선 후보 |
| ---- | ---- | --------- | ---- | --------- |
| TODO | TODO | TODO      | TODO | TODO      |

실패 유형:

- `missing_card`
- `ambiguous_card`
- `stale_path`
- `stale_verify`
- `overbroad_entry`
- `underlisted_entry`
- `prompt_ambiguous`
- `base_commit_broken`
- `model_variance`

### 3.5 Observations

실험 후 다음 질문에 답한다.

- index가 평균적으로 도움이 되었는가?
- 효과가 큰 task 유형은 무엇인가?
- 효과가 작거나 악화된 task 유형은 무엇인가?
- 특정 card가 반복적으로 misroute를 만들었는가?
- `verify` 정보가 실제 검증 선택에 기여했는가?
- navigation-only 효과가 implementation 성공률로 이어졌는가?

## 4. Conclusion

이 섹션은 실제 실험 후 최종 판단을 작성한다. 결론은 다음 중 하나로 분류한다.

| 판단      | 의미                                                    | 후속 조치                                       |
| --------- | ------------------------------------------------------- | ----------------------------------------------- |
| 유지      | index가 탐색 비용을 줄이고 성공률을 유지 또는 개선했다. | stale card 정리와 정기 검증 추가                |
| 부분 유지 | 일부 영역에서만 효과가 있었다.                          | 효과 낮은 card split/merge, verify 보강         |
| 축소      | 평균 효과가 작거나 maintenance cost가 크다.             | 핵심 card만 남기고 중복 제거                    |
| 재설계    | misroute나 stale 정보가 성능을 악화했다.                | card format, ownership 기준, update flow 재설계 |

최종 보고서는 다음 조건을 만족해야 한다.

- pilot과 본 실험의 task 목록, 제외 목록, gold label이 남아 있다.
- raw run log와 scored metrics가 재계산 가능하다.
- A/B paired 결과가 전체와 card별로 정리되어 있다.
- 구현 실험을 수행하지 않은 경우 그 이유와 navigation-only 한계를 명시한다.
- 개선 대상 card와 구체 수정 방향이 나온다.
- `.agents/index` 개선이 필요한 경우 별도 change list로 분리한다.

실험 후 결정할 항목:

- `.agents/index` 유지/축소/확장 방향
- card split 또는 merge 필요 여부
- `verify` 항목을 script/hook/CI로 옮길 후보
- index update를 PR checklist나 source sanity에 넣을지 여부
- 동일 실험을 정기 regression으로 자동화할지 여부
