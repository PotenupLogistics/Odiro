---
status: In Progress
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
- `Client/Docs/Data/**`: 제거됨. 전역 데이터 계약 기준 아님

## 현재 상태

- T01-T09: 런타임 구현 완료. 최종 검증 진행 중
- T10: 이전 계약 정리 진행 중
- T11: Client UI 프로토타입 구현 완료. 수동/Editor 검증 대기
- `MainMenuWidget`: user project 실행 중심 prototype 적용

## 문서별 책임

- `project-structure.md`: 최종 폴더 구조
- `simulation-interface.md`: 용어, 실행 흐름, 입력 고정 시점
- `user-project-data.md`: project 입력 파일, run 결과 파일, project preset, run 기본 폴더
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

### T01 계약 문서 정리 [완료]

목적:

- 최종 계약 고정
- 문서 책임 분리

변경:

- `project-structure.md`: 최종 구조만 유지
- `simulation-interface.md`: 최종 용어와 실행 흐름만 유지
- 공유 데이터 파일 형식 → `contracts/specs/user-project-data.md`
- Bridge IPC 호출 형식 → `contracts/specs/bridge-ipc.md`
- `Client/Docs/Data/**` 전역 계약 내용 → `user-project-data.md`로 통합
- 남은 `Client/Docs/Data/**` 제거
- 남은 `Client/Docs/**` → Client 전용 문서만 유지

검증:

- 오래된 용어 검색
- `git diff --check`

### T02 Project preset 정리 [완료]

목적:

- 새 project 생성 원본 정리

변경:

- 개발 원본: `static/templates/`
- 배포 원본: `resources/templates/`
- 지원 preset id: 문서 고정 목록 X
- 지원 preset id source: `scenario/*.json`, `profile/*.json`, `policy/` category 직접 하위 폴더
- 필수 파일: `setting.json`, `scenario/<id>.json`, `profile/<id>.json`, `policy/<id>/__init__.py:create_policy`
- `policy/demo`: 기존 `Client/Tools/PythonAgent/agent` 내용을 project policy 형태로 이전
- `policy/blank`: 최소 유효 skeleton. demo policy 기준으로 쓰지 않음
- `Client/Tools/PythonAgent/samples`: 사용처 확인 후 fixture 유지 또는 제거

검증:

- Bridge build
- 개발/배포 내용 비교

### T03 Run 기본 폴더 [완료]

목적:

- `workspace.createRun`용 run 기본 폴더를 static resource로 관리

변경:

- 개발 원본: `static/run-defaults/`
- 배포 원본: `resources/run-defaults/`
- 새 run directory 생성 전 복사
- 허용: `review/`, `episodes/` 같은 빈 폴더
- source-only marker: `.gitkeep`
- 금지: `status.json`, `summary.json`, `snapshot/`, 실제 episode 결과

검증:

- static copy dry-run
- run directory 구조 확인

### T04 Bridge 기본 기능 [완료]

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
- `getRunStatus`
- `stopSimulator`
- `status.json` 작성

검증:

- `cd Bridge; go test ./...`
- build script
- `git diff --check`

### T05 Agents 경계 정리 [완료]

목적:

- RunQueue/API 대체 경로 제거

변경:

- v1 scenario route: 제거 안내만 유지
- v2 scenario 응답: `scenario`
- v2 analysis 입력: `project_path + run_id`
- RunQueue API 생성 경로 제거
- Agents 문서의 현재 API 설명 정리

검증:

- 관련 pytest
- `cd Agents; uv run ruff check app tests`
- `git diff --check`

### T06 Client project 실행 기반 [완료]

목적:

- Bridge가 만든 run snapshot으로 simulator process bootstrap

변경:

- `-OdiroProject`
- `-RunId`
- `-PolicyPort`
- snapshot parse
- `setting.runtime.map_id`, `setting.runtime.fixed_fps` 적용
- `policy/__init__.py:create_policy` 확인
- `Client/Tools/PythonAgent/server.py` → `Client/Resources/policy-runtime.py` 이전
- project preset에는 policy runtime을 복사하지 않음
- 실패 시 0이 아닌 종료 코드
- episode scenario 생성과 실제 episode 실행 연결은 T07에서 처리

검증:

- `Build.bat OdiroSimEditor Win64 Development -Project=X:\Odiro\Client\OdiroSim.uproject -WaitMutex -FromMsBuild -NoXGE -MaxParallelActions=1`
- `OdiroSim.UserProjectRun`
- `OdiroSim.SimulationSetup.CommandLine.Parse`
- `OdiroSim.SimulatorLaunch.CommandLine`

### T07 Client 입력 계약 정리 [완료]

목적:

- 이전 Client 입력 표면 제거

변경:

- command-line/launcher project root 전달
- `setting/profile/scenario` read/write
- `EpisodeScenario` reader/writer
- project run은 RunQueue 파일을 만들지 않고 episode input 배열로 runner 시작
- `episode_scenario` → runtime WorldSpec adapter
- `simulation_profile` field alias → 기존 DeliveryBot setup compiler

검증:

- `Build.bat OdiroSimEditor Win64 Development -Project=X:\Odiro\Client\OdiroSim.uproject -WaitMutex -FromMsBuild -NoXGE -MaxParallelActions=1`
- `OdiroSim.UserProjectEpisodeScenario.WorldSpecAdapter.Valid`
- `OdiroSim.UserProjectData`
- `OdiroSim.SimulatorLaunch.ProjectRun.Validation`
- `git diff --check`
- project run direct input 경로 확인
- 오래된 Client symbol 검색은 T10에서 최종 제거 기준으로 재수행

### T08 로그 계약 정리 [완료]

목적:

- 단일 MeasurementLog 분리

변경:

- 완료:
  - project run episode 종료 시 `result.json` 작성
  - project run policy decide 시 `actions.jsonl` 작성
  - `events.jsonl` 작성
  - 종료 원인 event 항상 기록
  - project run runtime tick에서 `trace.jsonl` 작성
  - run 종료 시 `summary.json` 작성
  - `PlatformAnalysisAiSubsystem` v2 project run 입력 추가
  - v2 analysis 응답을 `runs/<RunId>/review/analysis_run_response_v2.json`에 저장

T10 처리:

- legacy report/log analysis 요청 제거 또는 보관 범위 명시

세부 변경 대상:

- `actions` writer
- `events` writer
- `trace` writer
- `result` writer
- `summary` writer

검증:

- `Build.bat OdiroSimEditor Win64 Development -Project=X:\Odiro\Client\OdiroSim.uproject -WaitMutex -FromMsBuild -NoXGE -MaxParallelActions=1`
- `OdiroSim.UserProjectData.RunOutput.Write`
- `OdiroSim.UserProjectData.RobotAction.Write`
- `OdiroSim.UserProjectData.EpisodeTrace.Write`
- `OdiroSim.Platform.AnalysisAi.ProjectRunRequestJsonBuild`
- log 파일 간단 점검
- 오래된 `Saved/AnalysisLogs` 검색 결과를 T10 대상과 대조

### T09 전체 흐름 점검 [완료]

목적:

- 전체 실행 검증

변경:

- 완료:
  - Bridge process lifecycle test 보강
  - fake simulator child 실행으로 `status.json` lifecycle 확인
  - 실제 `static/templates` 기준 project 생성 dry-run
  - 실제 `static/run-defaults` 기준 run 생성 dry-run
  - run snapshot 경로와 generated cache 제외 확인
  - 실제 simulator process smoke 확인
    - `-OdiroProject`, `-RunId`, `-PolicyPort`로 실행
    - run 완료 후 simulator child process 정상 종료
    - Simulator는 `status.json`을 쓰지 않음. `status.json` lifecycle은 Bridge process test로 검증
    - `episodes/000001/result.json`
    - `episodes/000001/actions.jsonl`
    - `episodes/000001/events.jsonl`
    - `episodes/000001/trace.jsonl`
    - `summary.json`
    - `review/` directory 유지

검증:

- `cd Bridge; go test ./...`
- OdiroSimEditor build
- `OdiroSim.UserProjectData`
- `OdiroSim.SimulatorLaunch.ProjectRun.Validation`
- `OdiroSim.Platform.AnalysisAi.ProjectRunRequestJsonBuild`
- `status`
- `snapshot`
- `episode`
- `result`
- `summary`
- `review`

### T10 이전 계약 정리 [진행 중]

목적:

- 이전 계약 폐기

변경:

- 완료:
  - `Client/Docs/Data/**` 제거
  - Scenario LLM authoring의 RunQueue 저장/실행 경로 제거
  - Scenario LLM authoring은 v2 `scenario` 응답을 user project `scenario.json`에 저장
  - `SimulatorLaunchSubsystem`의 SimulationSetup/RunQueue/legacy report helper는 Blueprint 호환용 deprecated API로 보관
  - Agents RunQueue export tooling/test는 legacy tooling 범위로 보관
  - legacy contract 문서(`RunQueue`, `DeliveryBotSetup`, `EpisodeEvaluationReport`)는 보관 guide로 명시

보류:

- MainMenu legacy `Json/Input`, `RunQueue`, `Saved/AnalysisLogs` fallback 제거
- UE 내부 Bridge IPC client 연결

검증:

- repo 전체 오래된 계약 검색
- OdiroSimEditor build
- Agents focused pytest/ruff

### T11 Client UI 프로토타입 [구현 완료, 검증 완료]

목적:

- MainMenu에서 사용자 project를 선택하고 실행
- 디자인보다 기능 우선
- UI 배치는 `WBP_MainMenu`가 소유
- C++는 widget binding, event 연결, workflow logic만 소유

구현 판단:

- 현재 UE Client에는 Bridge IPC client가 없음
- 이번 UI는 `USimulatorLaunchSubsystem`에 임시 file 기반 workspace helper를 둠
- 나중에 Bridge IPC client가 생기면 helper 구현을 `workspace.*`, `process.*` 호출로 대체
- `StartProjectRun(projectPath, runId)`는 기존 direct simulator launch를 사용
- Bridge `status.json` lifecycle은 아직 UI direct path에서 사용하지 않음
- UI direct launch는 임시 고정 `-PolicyPort=18145` 사용
- Bridge 연결 후 `process.startSimulator`가 port를 소유하고 전달
- UMG 수정은 MCP로 수행. `apply_layout` 대신 `create_widget` 경로를 사용해 Blueprint variable GUID를 유지

프로토타입 UI:

- project path 입력
- scenario/profile/policy preset 선택: `static/templates/*` 또는 `resources/templates/*`
- project 생성
- project 검증
- run 생성
- run 시작
- run 목록 표시
- `summary.json`, episode `result.json`, `actions/events/trace` 미리보기
- v2 AI 분석 요청: `project_path + run_id`

구현 대상:

- `SimulatorLaunchSubsystem`
  - project preset 목록 [구현]
  - project 생성 [구현]
  - project 검증 [구현]
  - run snapshot 생성 [구현]
  - project run directory/result/log 목록 [구현]
- `MainMenuWidget`
  - `WBP_MainMenu`의 project controls optional binding [구현]
  - project path 입력 시 project mode로 분기 [구현]
  - project mode 결과 목록은 `<UserProject>/runs/<RunId>` 기준 [구현]
  - project run preview는 `summary.json`, episode `result.json`, `actions/events/trace` 기준 [구현]
  - project run 선택 시 v2 analysis 호출 [구현]
- `UmgMcp`
  - `create_widget`가 생성 widget을 Blueprint variable로 등록 [구현]
  - `delete_widget`가 subtree와 stale GUID를 함께 정리 [구현]

주의:

- project path가 비어 있으면 기존 MainMenu legacy 흐름 유지
- project path가 있으면 project mode가 우선
- project mode에서는 legacy `SimulationSetup`, RunQueue writer/launcher를 호출하지 않음
- project mode UI control은 C++에서 생성하지 않음
- 이번 단계는 Bridge IPC 미연결. UI 검증 후 helper 내부를 Bridge 호출로 교체

검증:

- 통과:
  - `git diff --check`
  - OdiroSimEditor build
    - `task-build.bat client`
  - MCP Blueprint compile/save
    - `WBP_MainMenu`
    - project mode BindWidget 이름 존재 확인
  - `OdiroSim.MainMenu.ProjectMode.Smoke`
    - MainMenu prototype API로 project 생성
    - project 검증
    - run snapshot 생성과 선택
  - MainMenuMap headless smoke
    - `Project mode controls bound: true`
    - `WBP_MainMenu_C` shown
  - 최신 Blueprint compile/save 구간에서 widget GUID ensure 재발 없음
- 미검증:
  - 실제 버튼 click event 경유 smoke
  - simulator process 실행 버튼 smoke
  - project run result preview visual 확인
  - policy-runtime 기본 포트 bind 오류 원인

남은 판단:

- Bridge IPC client를 UE에 붙일 시점
- root `task-build.bat client`는 병렬/XGE에서 MSVC PCH memory 부족 가능

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

## 완료 검증

- `git diff --check`
- `cd Bridge; go test ./...`
- `cd Agents; uv run ruff check app tests`
- `cd Agents; uv run pytest tests/test_scenario_generation_api.py tests/test_v2_analysis_run_api.py tests/test_v2_graph_settings.py tests/test_v2_result_analysis_graph_runner.py`
- `Build.bat OdiroSimEditor Win64 Development -Project=X:\Odiro\Client\OdiroSim.uproject -WaitMutex -FromMsBuild -NoXGE -MaxParallelActions=1`
- UE automation:
  - `OdiroSim.SimulationSetup.CommandLine.Parse`
  - `OdiroSim.UserProjectRun.Snapshot`
  - `OdiroSim.UserProjectData`
  - `OdiroSim.UserProjectEpisodeScenario.WorldSpecAdapter.Valid`
  - `OdiroSim.ScenarioSample.WorldSpecAdapter.Valid`
  - `OdiroSim.SimulatorLaunch.ProjectRun.Validation`
  - `OdiroSim.Platform.AnalysisAi.ProjectRunRequestJsonBuild`
- Direct process smoke:
  - `-OdiroProject`, `-RunId=000001`, `-PolicyPort=18145`
  - exit code `0`
  - `status.json` 없음
  - `summary.json` rows `3`
  - `episodes/000001/result.json` success `true`, terminal reason `GoalReached`
  - `summary.json`/`result.json` policy snapshot hash 일치
  - `actions.jsonl`, `events.jsonl`, `trace.jsonl` 생성
