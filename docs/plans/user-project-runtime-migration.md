---
status: Draft
type: change
specs:
  - docs/specs/project-structure.md
  - docs/specs/simulation-interface.md
  - contracts/specs/user-project-data.md
  - contracts/specs/bridge-ipc.md
---

# 사용자 프로젝트 실행 구조 전환 계획

## 목적

- 분산 실행 데이터 → 하나의 사용자 project folder
- 제거 대상: `Json/Input`, `SimulationSetup`, RunQueue, AppData, `_appdata` 중심 실행 계약

## 기준 문서

| 우선 | 문서                                   | 역할                                          |
| ---- | -------------------------------------- | --------------------------------------------- |
| 1    | `docs/specs/project-structure.md`      | 최종 repository, release, 사용자 project 구조 |
| 2    | `docs/specs/simulation-interface.md`   | 최종 용어, 실행 흐름                          |
| 3    | `contracts/specs/user-project-data.md` | 공유 사용자 project 파일 계약                 |
| 4    | `contracts/specs/bridge-ipc.md`        | Bridge IPC 호출 계약                          |

주의:

- `project-structure.md`, `simulation-interface.md`: 최종 상태 기준
- 현재 구현 상태에 맞춘 두 문서 되돌림 금지
- `Client/Docs/Data/**`: 전역 데이터 계약 기준 아님
- `Client/Docs/Data/**` 내용: 필요한 항목만 `user-project-data.md`로 이관 후 제거

## 현재 상태

- git `HEAD` 기준 변경분: 완료 구현으로 간주 X
- 현재 worktree 변경분: 중단된 초안
- 각 단계 완료 조건: 재검토 + 검증 통과

## 문서별 책임

- `project-structure.md`: 최종 폴더 구조
- `simulation-interface.md`: 용어, 실행 흐름, 입력 고정 시점
- `user-project-data.md`: project 입력 파일, run 결과 파일, project template, run 기본 폴더
- `bridge-ipc.md`: IPC 요청/응답, 오류 코드, 경로 검증 규칙
- 이 계획 문서: 전환 순서, 기존 데이터 이동 기준, 구현 책임, 위험

## 기존 데이터 이동 기준

| 기존 데이터                                | 이동 위치                                           | 처리                                           |
| ------------------------------------------ | --------------------------------------------------- | ---------------------------------------------- |
| `Json/Input/SimulationSetup*.json`         | `<UserProject>/setting.json`                        | FPS, seed, episode count, 실행/evaluation 설정 |
| `Json/Input/DeliveryBotSetup*.json`        | `<UserProject>/profile.json`                        | robot body/drive/lidar capability              |
| `Json/Input/ScenarioSetup*.json`           | `<UserProject>/scenario.json`                       | project-local editable scenario                |
| `scenario_template`                        | `scenario`                                          | template/sample 분리 폐기                      |
| `scenario_sample`                          | `episode_scenario`                                  | run 중 seed로 확정한 episode 입력              |
| `Json/Input/*RunQueue*.json`               | 없음                                                | 호환 계층 없이 제거                            |
| `Saved/AnalysisLogs/MeasurementLog*.jsonl` | `actions.jsonl`, `events.jsonl`, `trace.jsonl`      | 목적별 분리                                    |
| `EpisodeEvaluationReport*.json`            | `result.json`, `summary.json`                       | episode 결과와 run 집계 분리                   |
| Agents review output                       | `runs/<RunId>/review/analysis_run_response_v2.json` | `/api/v2/analysis/run` 응답 snapshot           |

## 구현 책임

| 영역               | 책임                                                                                        |
| ------------------ | ------------------------------------------------------------------------------------------- |
| Bridge `api`       | Client, Agents, Simulator 공유 IPC 호출                                                     |
| Bridge `workspace` | project 검증, template 복사, run id, snapshot, 경로 안전성                                  |
| Bridge `process`   | simulator 자식 process 실행, 추적, `status.json` 생명주기                                   |
| Simulator          | snapshot 읽기, policy runtime 준비, EpisodeScenario 생성, episode 결과 파일, `summary.json` |
| Agents             | `scenario` 생성, run 분석, `review/analysis_run_response_v2.json` 저장                      |

금지:

- AppData 또는 `_appdata` 대체 경로
- RunQueue 호환 계층
- `SimulationSetup` 기반 신규 writer
- `scenario_template`/`scenario_sample` 신규 공개 계약
- `policy` 하위 `agent` directory 진입점 구조

## 작업 단계

### T01 계약 문서 정리

목적:

- 최종 계약 고정
- 문서 책임 분리

변경:

- `project-structure.md`: 최종 구조만 유지
- `simulation-interface.md`: 최종 용어와 실행 흐름만 유지
- 공유 데이터 파일 형식 → `contracts/specs/user-project-data.md`
- Bridge IPC 호출 형식 → `contracts/specs/bridge-ipc.md`
- `Client/Docs/Data/**` 전역 계약 내용 → `user-project-data.md` 기준 재작성
- 남은 `Client/Docs/**` → Client 전용 문서만 유지

검증:

- 오래된 용어 검색
- `git diff --check`

### T02 Project template 정리

목적:

- 새 project 생성 원본 정리

변경:

- 개발 원본: `static/project-templates/<TemplateId>`
- 배포 원본: `resources/project-templates/<TemplateId>`
- 지원 template id: 문서 고정 목록 X
- 지원 template id source: `static/project-templates/` 직접 하위 폴더
- 필수 파일: `setting.json`, `profile.json`, `scenario.json`, `policy/__init__.py:create_policy`

검증:

- Bridge build
- 개발/배포 내용 비교

### T03 Run 기본 폴더

목적:

- `workspace.createRun`용 run 기본 폴더를 static resource로 관리

변경:

- 개발 원본: `static/run-defaults/`
- 배포 원본: `resources/run-defaults/`
- 새 run directory 생성 전 복사
- 허용: `review/`, `episodes/` 같은 빈 폴더
- 금지: `status.json`, `summary.json`, `snapshot/`, 실제 episode 결과

검증:

- createRun 간단 점검
- run directory 구조 확인

### T04 Bridge 기본 기능

목적:

- project/run 생명주기 경계 도입

변경:

- `api`
- `workspace`
- `process`
- `createProject`
- `validateProject`
- `createRun`
- `startSimulator`

검증:

- `cd Bridge; go test ./...`
- build script

### T05 Agents 경계 정리

목적:

- RunQueue/API 대체 경로 제거

변경:

- v1 scenario route: 제거 안내만 유지
- v2 scenario 응답: `scenario`
- v2 analysis 입력: `project_path + run_id`

검증:

- 관련 pytest

### T06 Client project 실행 기반

목적:

- snapshot 기반 simulator 실행

변경:

- `-OdiroProject`
- `-RunId`
- snapshot parse
- policy 준비 확인
- 실패 시 0이 아닌 종료 코드

검증:

- UE build
- ProjectRuntime automation
- SimulatorProcess automation

### T07 Client 입력 계약 정리

목적:

- 이전 Client 입력 표면 제거

변경:

- MainMenu/launcher project root 선택
- `setting/profile/scenario` read/write
- `EpisodeScenario` reader/writer

검증:

- 오래된 Client symbol 검색
- editor/runtime automation

### T08 로그 계약 정리

목적:

- 단일 MeasurementLog 분리

변경:

- `actions` writer
- `events` writer
- `trace` writer
- `result` writer
- `summary` writer

검증:

- log 파일 간단 점검
- 오래된 `Saved/AnalysisLogs` 검색

### T09 전체 흐름 점검

목적:

- 전체 실행 검증

변경:

- static project template 목록 조회
- project 생성
- run 생성
- Bridge 경유 simulator 실행
- 결과 파일 확인

검증:

- `status`
- `snapshot`
- `episode`
- `result`
- `summary`
- `review`

### T10 이전 계약 정리

목적:

- 이전 계약 폐기

변경:

- 오래된 samples/tests/docs 보관 또는 삭제
- `Client/Docs/Data/**` 제거
- `.agents/index` 재검토

검증:

- repo 전체 오래된 계약 검색

## 검색 명령

T01:

```powershell
rg -n "materialized scenario|scenario_template|scenario_sample|%APPDATA%|_appdata|policy[/\\]agent" docs Client/Docs contracts
git diff --check
```

T07/T10:

```powershell
rg -n "ScenarioTemplate|ScenarioSample|SimulationSetup|DeliveryBotSetup|RunQueue|run_queue|MeasurementLog|Saved/AnalysisLogs" Client/Source/OdiroSim
rg -n "RunQueue|run_queue|scenario_template|scenario_sample|SimulationSetup|DeliveryBotSetup|EpisodeEvaluationReport|OdiroSim/experiments|%APPDATA%|_appdata|materialized|policy[/\\]agent" docs Client/Docs contracts Agents Bridge Client/Source
```

허용 hit:

- 이전/보관 문서
- 이전 계획 기록
- 전환 보호 규칙

## 보류 결정

| 항목                                 | 결정 시점                                                                           |
| ------------------------------------ | ----------------------------------------------------------------------------------- |
| `review/` 추가 report/finding schema | Agents analysis 재작성 전                                                           |
| `summary.json` 집계 field            | Client result writer 후                                                             |
| `result.json` advanced metrics       | Client result writer 후                                                             |
| snapshot hash 산정 규칙              | `workspace.createRun` 안정화 후                                                     |
| 패키징된 policy runtime 형태         | 배포 패키징 전. `Client/Resources/policy-runtime.py` 유지 또는 `policy-runtime.pyz` |

## 위험

| 위험                                | 대응                                                                      |
| ----------------------------------- | ------------------------------------------------------------------------- |
| 최종 spec를 현재 구현 상태로 되돌림 | `project-structure.md`, `simulation-interface.md` 우선                    |
| RunQueue 제거로 v1 API 중단 영향    | v1은 `410 RUN_QUEUE_REMOVED` 제거 안내 응답. 호환 계층 금지               |
| seed 재현성 불일치                  | `base_seed + episode_index`, `generator_version` 기록 테스트              |
| snapshot 누락                       | `workspace.createRun` copy 검증                                           |
| policy runtime 경로 혼선            | `Client/Resources/policy-runtime.py`, snapshot `policy/` import root 검증 |
| Bridge/Simulator 책임 혼선          | Bridge는 process 생명주기, Simulator는 episode 결과 파일                  |
| 이전 log 분석 의존                  | 이전 log 호환 없음 명시, 새 analysis 입력만 검증                          |

## 중단 조건

- scenario editor/type/runner 변경이 최종 계약 재결정을 요구
- project template schema와 `user-project-data.md` schema 충돌
- Bridge 경로 안전성 보장 실패
- snapshot `policy/__init__.py:create_policy` 계약 불일치
