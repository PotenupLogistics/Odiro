---
status: Draft
type: architecture
specs:
  - Docs/specs/Interfaces.md
  - Docs/specs/simulation-json-files.md
---

# 플랫폼 설계

## 목표

시나리오 편집기, 시뮬레이터 실행, Python API 서버, LLM 서버를 하나의 플랫폼으로 통합하여 사용자에게 일관된 인터페이스를 제공한다.

## 구성 요소

- **Unreal Engine 기반**
  - **플랫폼**: 통합 애플리케이션. 사용자 인터페이스와 시뮬레이터 실행을 담당
  - **시나리오 편집기**: 시나리오를 시각적으로 편집할 수 있는 에디터
  - **시뮬레이터**: 지정된 구성대로 시뮬레이션 환경을 로드하고 실행. 사용자 입력을 받지 않음

- **LLM 서버**
  - **시나리오 편집 Agent**: 프롬프트 기반으로 시나리오 구성 JSON 파일을 생성 및 편집
  - **에피소드 구성 샘플링 Agent**: 시뮬레이터에서 적절한 에피소드 구성 JSON 파일을 생성
  - **결과 분석 Agent**: 시뮬레이션 결과 로그를 분석하여 문제 상황 탐지, 개선 방법 등을 포함한 리포트 생성
  - **행동 정책 편집 Agent**: 분석 결과를 바탕으로 로봇 행동 정책 개선 제안 및 Python 스크립트 편집

- **Python API 서버**
  - **행동 정책 API**: 시뮬레이터에서 로봇 행동을 제어하는 API. 로봇 액터가 시뮬레이션 과정 중 호출

## 설계

하나의 프로젝트로 Unreal Engine 클라이언트를 모두 개발한다.
그냥 켜면 플랫폼 MainMenu가 실행되고, 시뮬레이터는 Commandline Parameter와 함께 별도의 프로세스로 실행된다.
시나리오 편집기는 `EpisodeEditorMap`에서 제공하며, MainMenu는 에디터 내부 기능을 구현하지 않고 에디터 map으로 진입하는 역할만 맡는다.

### Main Window

- 플랫폼 UI 표시. 창모드로 실행, Unreal Engine의 Slate 또는 UMG를 활용하여 UI 구현
- 좌측 사이드바: 내비게이션 메뉴, 각 메뉴 클릭 시 우측 콘텐츠 영역에 해당 화면 표시
  - 홈: 랜딩 페이지, 공지사항, 최근 활동 등 표시
  - 시나리오: 시나리오 구성 목록 표시, 각 항목 클릭 시 `EpisodeEditorMap` 열기
  - 행동 정책: 행동 정책 목록 표시, 각 항목 클릭 시 텍스트 에디터 열기
  - 실험 구성: 시뮬레이션 구성 목록 표시, 각 항목 클릭 시 설정 값 편집 화면 전환
  - 실험 현황: 시뮬레이션 실행 현황 확인 및 관리
  - 실험 결과: 시뮬레이션 결과 로그 목록 표시, 각 항목 클릭 시 분석 리포트 확인 화면 전환
  - 설정: 서버 통신 설정, LLM API 키 설정 등

### 시나리오 편집기

- 특정 시나리오 구성 JSON 파일을 열어 시각적으로 표현. Editor Level은 `EpisodeEditorMap`에서 실행
- 에디터 UI와 authoring 기능은 에디터 담당 작업자의 소유로 두고, Platform 작업은 MainMenu에서 `EpisodeEditorMap`으로 진입하는 Level 전환만 담당
- MVP에서는 MainMenu와 EpisodeEditorMap을 같은 Unreal process의 별도 map으로 취급한다. 같은 process 안에서 두 map을 각각 독립 창으로 동시에 실행하는 구조는 구현하지 않는다
- 월드 뷰: 액터 배치 및 시뮬레이션 환경 시각화. 액터를 클릭하여 속성 패널에서 편집 가능
- 시나리오 구성 트리: 시나리오 구성 요소를 트리 형태로 표시, 각 요소 클릭 시 속성 패널에 상세 정보 표시
- 속성 패널: 선택한 요소의 속성 편집 가능. 예: 액터 위치, 행동 정책 매핑 등
- 액터 Drawer: 사용할 수 있는 액터 목록 표시, 클릭으로 배치 시작 가능
- AI 채팅 패널: 시나리오 편집 Agent와 대화하여 시나리오 구성 편집 가능

### 시뮬레이터

- 특정 시뮬레이션 구성 JSON 파일을 로드 후, 시뮬레이션 프로세스 실행
- Main Window에서 Commandline Parameter로 시뮬레이터 실행 (별도의 Unreal Engine 프로세스)
- Player Screen 띄우지 않음. 예시 옵션: `-SimulatorMode -UseFixedTimeStep -FPS=60 -unattended -novsync -RenderOffScreen -ResX=16 -ResY=16 -NoSplash -NOSCREENMESSAGES`
- 시뮬레이터 프로세스는 Python API 서버와 통신하여 로봇 행동 제어 처리, 메인 플랫폼과 통신하여 시뮬레이션 진행 상황 업데이트 및 결과 전달

### Python API 서버

- 시뮬레이터에서 로봇 행동 제어를 위한 API 제공
- 시뮬레이터 프로세스 실행 시 함께 실행, Socket 또는 HTTP API로 통신

### LLM 서버

- 시나리오 편집 Agent, 에피소드 구성 샘플링 Agent, 결과 분석 Agent, 행동 정책 편집 Agent로 구성
- 원격 서버에서 독립적으로 실행, HTTP API로 플랫폼과 통신
- 추후 플랫폼과 통합하여 플랫폼 실행 시 동시에 실행되도록 개선 가능

## 분석

> 이하 `## 분석`은 위 설계를 바탕으로 Codex가 작성한 실행 계획이다.
> 위쪽 원문 예시와 충돌하면 본 절의 `-Simulate=<SimulationSetupFile>` 계약을 우선한다.

### 범위

- `EpisodeSimulationMap`: 시뮬레이션 수행
- `EpisodeEditorMap`: 구현된 시나리오 에디터 map. 에디터 UI와 authoring 기능은 다른 작업자 담당으로 제외. 이 계획은 MainMenu에서 에디터 map을 여는 진입점만 포함
- Platform UI와 Simulator는 별도 프로세스로 실행
- Platform UI 프로세스는 fixed-step을 적용하지 않고 `EpisodeSimulationMap`을 직접 로드하지 않음
- Simulator 프로세스는 `EpisodeSimulationMap`을 로드하고 fixed-step으로 실행
- 로봇 구현은 다른 작업자가 진행 중으로 이 계획에서 Python API Server 통신 구현 제외
- 이 계획은 플랫폼이 시뮬레이션 실행, 상태 추적, 결과 조회, 시나리오 에디터 진입을 연결하는 범위만 다룸
- `EpisodeSimulationMap`의 Episode spawn 경로는 즉시 구현

### 현재 구현

| 영역                      | 상태    | 근거                                                                    | 플랫폼 설계에서의 의미                                                 |
| ------------------------- | ------- | ----------------------------------------------------------------------- | ---------------------------------------------------------------------- |
| Runtime module            | 구현    | `ProtoRobotSim.uproject`, `Source/ProtoRobotSim/ProtoRobotSim.Build.cs` | 플랫폼, 시뮬레이터, 편집기 기능을 같은 runtime module 안에서 시작 가능 |
| Episode JSON compile      | 구현    | `UEpisodeCompiler`                                                      | 시나리오 구성 JSON을 검증하고 runtime spec으로 변환하는 기반           |
| Episode world setup       | 구현    | `UEpisodeSimulationSubsystem`                                           | `EpisodeSimulationMap`에서 실행할 월드 배치 기반                       |
| DeliveryBot setup compile | 구현    | `UDeliveryBotSetupCompiler`                                             | 실행 설정의 입력 파일로 선택하고 검증 가능                             |
| Batch runner              | 구현    | `UEpisodeRunnerSubsystem::StartBatchFromRunQueueJsonFile`               | 실험 구성 실행의 핵심 API                                              |
| Evaluation report         | 구현    | `FEpisodeEvaluationReportJson`                                          | 실험 결과 화면의 요약 데이터 기반                                      |
| Measurement log           | 구현    | `UEpisodeMeasurementLogSubsystem`                                       | 결과 분석 Agent와 결과 상세 화면의 raw log 기반                        |
| Map 분리                  | 구현    | `EpisodeSimulationMap`, `EpisodeEditorMap`                              | 시뮬레이션 실행과 시나리오 편집 map을 분리                             |
| Remote policy API         | 범위 밖 | `UDeliveryBot_PolicyJudgmentComponent`                                  | 로봇 담당 작업자의 구현 범위                                           |
| Platform UI               | 구현    | `AMainMenuPlayerController`, `UMainMenuWidget`                          | MainMenu에서 simulation 실행과 status/report/log 조회 가능             |
| Simulator CLI bootstrap   | 구현    | `USimulatorProcessSubsystem`, `FSimulationCommandLine`                 | `-Simulate=<SimulationSetupFile>` 감지, fixed-step 적용, map load, runner 시작, status 기록 |
| LLM 연동                  | 미구현  | 관련 source 없음                                                        | 후순위 구현 대상                                                       |

현재 시뮬레이션 내부 기능은 대략적으로 구현되어 있고, 실행 운영 계층 필요.
플랫폼에서 내비게이션 메뉴를 구현해 시뮬레이션 실행, 추적, 결과 조회 가능하도록 추가 구현 필요.

### 판단

- `EpisodeSetup JSON`, `DeliveryBotSetup JSON`, `EpisodeRunQueue JSON` 유지
- `SimulationSetup JSON`을 실행 설정 파일로 추가해 platform과 simulator가 같은 run queue, logging, report, status, fixed-step 설정 읽음
- Platform UI는 Simulator process를 실행하고 status/report/log만 읽음
- Platform UI는 `EpisodeSimulationMap`을 직접 로드하지 않음
- Simulator process는 SimulatorMode 내부에서 `UseFixedTimeStep`을 적용하고 `EpisodeSimulationMap` 로드
- fixed-step FPS는 Platform UI에서 실행 전 설정하고 `SimulationSetup JSON.fixed_step.fps`에 저장
- 시뮬레이터 프로세스는 `-Simulate=<SimulationSetupFile>` 인자를 받으면 내부적으로 SimulatorMode로 진입
- `-Simulate=<SimulationSetupFile>`은 기존 `-SimulatorMode -SimulationSetup=<file>` 조합을 대체
- SimulatorMode는 내부적으로 `-UseFixedTimeStep`을 적용하므로 외부 실행 인자에서 생략 가능
- `SimulationSetup JSON.fixed_step.fps`는 SimulatorMode의 fixed-step FPS 값으로 적용
- 시뮬레이터는 주기적으로 실험 현황 Status JSON으로 기록, 플랫폼도 주기적으로 읽어 UI 표시
- 시뮬레이터 중단, 오류 시 Status JSON에 실패 기록 필요
- `EpisodeSimulationMap`의 spawn 실행을 먼저 연결
- `EpisodeEditorMap` 내부 authoring, spawn/preview, 저장 흐름은 에디터 담당 작업자 소유로 두고 Platform에서는 직접 구현하지 않음
- MainMenu에서 `EpisodeEditorMap`을 열 때는 같은 process 안의 `OpenLevel` 기반 전환을 MVP 기본값으로 둠
- 같은 Unreal process 안에서 MainMenuMap과 EpisodeEditorMap을 각각 별도 runtime window로 동시에 띄우는 구조는 `SWindow`만으로 해결되지 않고 별도 `UWorld`/viewport lifecycle 관리가 필요하므로 MVP 범위에서 제외
- 에디터를 MainMenu와 동시에 유지해야 하는 요구가 생기면, 별도 Unreal process를 실행하는 launcher 방식으로 확장한다
- LLM 서버는 runtime 필수 의존성이 아니라 파일 생성, 분석, 수정 제안 도구로 둔다

## 작업

MVP 범위: T01~T05 / T06: 사용성 확장 / T07~T08은 병렬 작업자의 산출물이 안정된 뒤 진행

공통 경계:
- Platform UI, Simulator는 별도 process
- Platform UI는 fixed-step 적용 X, `EpisodeSimulationMap` 직접 로드 X
- Simulator는 `-Simulate=<SimulationSetupFile>`로 진입, fixed-step 적용
- 로봇 내부 구현, Python API Server 통신은 이 계획에서 구현 X
- `EpisodeEditorMap` 에디터 UI와 authoring 기능은 별도 작업자 담당, 이 계획은 MainMenu의 에디터 진입점만 다룬다.

### T01 실행 계약과 타입 고정 [x]

목표: platform과 simulator가 공유할 파일, command line, 결과 경로를 구현 가능한 계약으로 고정한다.

의존:
- 없음

상세 작업:
- [Docs/specs/simulation-json-files.md](x:/UE5/Proto-Unreal/Docs/specs/simulation-json-files.md)의 `SimulationSetup JSON`과 `Run Status JSON` 계약 구현 기준으로 사용
- `SimulationSetup JSON`의 `map_id`, `run_queue`, `fixed_step.fps`, `logging`, `report`, `status.output_path` field를 C++ 타입으로 정의
- simulator 실행 command는 `-Simulate=<SimulationSetupFile>`와 optional `-RunId=<RunId>`만 public 계약
- `-Simulate`가 있으면 내부 SimulatorMode로 진입하고 외부 command line의 `-SimulatorMode`, `-UseFixedTimeStep`, `-FPS`는 요구 X
- `Json/Input/SimulationSetupSample.json` 추가
- 신규 C++ 타입과 bootstrap 파일 위치 확정

구현 위치:
- [SimulationSetupTypes.h](x:/UE5/Proto-Unreal/Source/ProtoRobotSim/Public/Shared/SimulationSetupTypes.h): `FSimulationSetup`, `FSimulationRunStatus`, `FSimulationCommandLineOptions`
- [SimulationSetupTypes.cpp](x:/UE5/Proto-Unreal/Source/ProtoRobotSim/Private/Shared/SimulationSetupTypes.cpp): `SimulationSetup JSON` parser, `-Simulate`/`-RunId` parser
- [SimulationSetupTypesTest.cpp](x:/UE5/Proto-Unreal/Source/ProtoRobotSim/Private/Shared/Tests/SimulationSetupTypesTest.cpp): sample parse, invalid field, missing file, command line parser automation
- [SimulationSetupSample.json](x:/UE5/Proto-Unreal/Json/Input/SimulationSetupSample.json): T02 bootstrap용 sample setup
- T02 bootstrap 예정 위치: `Source/ProtoRobotSim/Public/Platform/SimulatorProcessSubsystem.h`, `Source/ProtoRobotSim/Private/Platform/SimulatorProcessSubsystem.cpp`

검증:
- `SimulationSetup JSON` parser가 sample file을 읽고 validation error를 반환
- `-Simulate`, `-RunId` parser test 존재
- 잘못된 setup path나 field를 simulator 시작 전에 실패 처리

### T02 Simulator bootstrap 구현 [x]

목표: Platform UI 없이 simulator process만 실행해 `EpisodeSimulationMap`에서 batch run을 시작한다.

의존:
- T01

상세 작업:
- game instance 또는 engine startup 경로에서 `-Simulate=<SimulationSetupFile>` 감지
- SimulatorMode 진입 시 Main Window와 Platform UI 생성 경로 차단
- SimulatorMode에서 `EpisodeSimulationMap` 로드
- SimulatorMode에서 `FApp::SetUseFixedTimeStep(true)`, `FApp::SetFixedDeltaTime(1 / fixed_step.fps)` 적용
- setup의 `run_queue`를 [EpisodeRunnerSubsystem.cpp](x:/UE5/Proto-Unreal/Source/ProtoRobotSim/Private/Episode/EpisodeRunnerSubsystem.cpp)의 `UEpisodeRunnerSubsystem::StartBatchFromRunQueueJsonFile`에 전달
- [EpisodeSimulationSubsystem.cpp](x:/UE5/Proto-Unreal/Source/ProtoRobotSim/Private/Episode/EpisodeSimulationSubsystem.cpp) 경로로 `EpisodeSimulationMap`의 Episode spawn 수행

구현 위치:
- [SimulatorProcessSubsystem.h](x:/UE5/Proto-Unreal/Source/ProtoRobotSim/Public/Platform/SimulatorProcessSubsystem.h): SimulatorMode 상태, setup/run id 조회, map/fixed-step helper
- [SimulatorProcessSubsystem.cpp](x:/UE5/Proto-Unreal/Source/ProtoRobotSim/Private/Platform/SimulatorProcessSubsystem.cpp): `-Simulate` 감지, setup parse, fixed-step 적용, target map load, runner start
- [SimulatorProcessSubsystemTest.cpp](x:/UE5/Proto-Unreal/Source/ProtoRobotSim/Private/Platform/Tests/SimulatorProcessSubsystemTest.cpp): map id 정규화와 fixed-step 계산 automation

T03 완료 범위:
- Run Status JSON writer
- setup의 logging 설정을 `UEpisodeMeasurementLogSubsystem`에 적용
- setup의 report 설정을 `UEpisodeRunnerSubsystem`에 적용

검증:
- `-Simulate=Json/Input/SimulationSetupSample.json`으로 simulator process가 단독 실행
- Platform UI process는 fixed-step을 적용 X
- Simulator process는 setup의 FPS로 fixed-step을 적용
- sample run queue가 runner에 전달되고 Episode spawn이 발생

### T03 결과와 상태 기록 연결 [x]

목표: 별도 process로 실행되는 simulator 상태를 Platform UI가 파일만으로 추적할 수 있게 한다.

의존:
- T02

상세 작업:
- simulator 시작, 진행, 완료, 실패, 취소 상태를 `Run Status JSON`으로 기록
- [EpisodeMeasurementLogSubsystem.cpp](x:/UE5/Proto-Unreal/Source/ProtoRobotSim/Private/Episode/EpisodeMeasurementLogSubsystem.cpp)에 setup의 logging 설정 적용
- [EpisodeRunnerSubsystem.cpp](x:/UE5/Proto-Unreal/Source/ProtoRobotSim/Private/Episode/EpisodeRunnerSubsystem.cpp)에 setup의 evaluation report JSON output 경로 적용
- runner 완료 시 status에 report path와 log path 기록
- setup 읽기 실패, map load 실패, runner 실패는 status `Failed`와 error message 남김

구현 위치:
- [SimulationSetupTypes.h](x:/UE5/Proto-Unreal/Source/ProtoRobotSim/Public/Shared/SimulationSetupTypes.h): `FSimulationRunStatusJson`
- [SimulationSetupTypes.cpp](x:/UE5/Proto-Unreal/Source/ProtoRobotSim/Private/Shared/SimulationSetupTypes.cpp): `SimulationRunStatus JSON` serialization/file writer
- [EpisodeRunnerSubsystem.h](x:/UE5/Proto-Unreal/Source/ProtoRobotSim/Public/Episode/EpisodeRunnerSubsystem.h): runner state/record completion native delegate, total/completed/current pair 조회
- [EpisodeRunnerSubsystem.cpp](x:/UE5/Proto-Unreal/Source/ProtoRobotSim/Private/Episode/EpisodeRunnerSubsystem.cpp): report output path를 `FEpisodeRunRecord`에 기록, runner state change broadcast
- [EpisodeMeasurementLogSubsystem.h](x:/UE5/Proto-Unreal/Source/ProtoRobotSim/Public/Episode/EpisodeMeasurementLogSubsystem.h): runtime logging settings 적용 API
- [SimulatorProcessSubsystem.cpp](x:/UE5/Proto-Unreal/Source/ProtoRobotSim/Private/Platform/SimulatorProcessSubsystem.cpp): report/logging setup 적용, runner 상태를 status JSON에 기록, 완료 시 log/report path 반영
- `-Simulate` 모드에서 target map이 기존 batch를 먼저 시작하면 해당 batch를 취소하고 `SimulationSetup.run_queue`를 실행 기준으로 사용

T04로 남긴 범위:
- Launcher process의 simulator launcher
- Platform UI status polling 화면

검증:
- 정상 run: status=`Completed`
- 실패 run: status=`Failed`, error message 포함
- `Json/Output`에 evaluation report 생성
- `Saved/AnalysisLogs`에 measurement log 생성
- Platform UI가 simulator 내부 객체 참조 없이 status/report/log 파일로 진행 상황을 알 수 있음

### T04 Platform launcher 구현 [x]

목표: Platform UI가 simulator process를 별도로 실행하고 lifecycle을 관리한다.

의존:
- T03

상세 작업:
- Launcher process에서 simulator 실행 command를 조립하는 launcher service 생성
- launcher는 `-Simulate=<SimulationSetupFile>`, `-RunId=<RunId>` 전달
- launcher process에는 fixed-step 관련 command line을 넘기지 않음
- packaged simulator exe가 아직 없으면 개발 검증 fallback으로 `RunPreview.bat "-Simulate=<SimulationSetupFile>" "-RunId=<RunId>"`를 subprocess로 호출
- `RunPreview.bat` fallback은 packaged exe와 같은 public command parameter를 받되 내부적으로 `UnrealEditor.exe <uproject> -game -NoSplash`를 붙여 실행
- process start 실패와 simulator status 실패 구분
- status file polling으로 `Pending`, `Running`, `Completed`, `Failed`, `Canceled` 상태 추적

검증:
- MainMenu에서 sample setup으로 simulator process 시작
- 패키징 전 환경에서는 `RunPreview.bat` fallback으로 같은 `-Simulate`, `-RunId` 인자를 전달해 subprocess 시작
- MainMenu process가 `EpisodeSimulationMap`을 로드하지 않은 상태로 run 상태 표시
- simulator 종료 후 report/log/status path를 조회
- 동일 setup을 새 run id로 다시 실행

구현 위치:
- [SimulatorLaunchSubsystem.h](x:/UE5/Proto-Unreal/Source/ProtoRobotSim/Public/Platform/SimulatorLaunchSubsystem.h): simulator launcher API, active run 상태, command argument helper
- [SimulatorLaunchSubsystem.cpp](x:/UE5/Proto-Unreal/Source/ProtoRobotSim/Private/Platform/SimulatorLaunchSubsystem.cpp): packaged exe 실행, `RunPreview.bat` fallback, process start/status failure 분리, status polling
- [SimulationSetupTypes.h](x:/UE5/Proto-Unreal/Source/ProtoRobotSim/Public/Shared/SimulationSetupTypes.h): `FSimulationRunStatusJson` read API
- [SimulationSetupTypes.cpp](x:/UE5/Proto-Unreal/Source/ProtoRobotSim/Private/Shared/SimulationSetupTypes.cpp): `SimulationRunStatus JSON` reader/writer 대칭 구현
- [SimulatorLaunchSubsystemTest.cpp](x:/UE5/Proto-Unreal/Source/ProtoRobotSim/Private/Platform/Tests/SimulatorLaunchSubsystemTest.cpp): launcher command contract와 terminal state automation

수동/후속 확인:
- Platform UI에서 Start Run 버튼으로 실제 simulator window가 뜨고 terminal status에서 종료되는지 화면 확인
- packaged exe가 생기면 같은 executable self-launch 경로로 `-Simulate=<SimulationSetupFile>`, `-RunId=<RunId>`가 전달되는지 확인
- packaged build에서 `MainMenuMap`, `EpisodeSimulationMap`, `Json/Input`, `Json/Output` staging/cook 설정 확인

### T05 Platform 최소 실행 화면 구현 [x]

목표: 사용자가 JSON 파일을 선택하고 run을 시작한 뒤 결과를 확인한다.

의존:
- T04

상세 작업:
- Platform main widget과 실험 실행 화면 추가
- `Json/Input`의 `SimulationSetup JSON` 목록 표시
- 선택한 setup의 `run_queue`, `fixed_step.fps`, logging/report/status 경로 표시
- 실행 전 `fixed_step.fps`를 수정해 setup에 저장할 수 있게 한다
- 실행 중 status polling 결과 표시
- 완료 후 `Json/Output`의 evaluation report 목록과 상세 표시
- measurement JSONL은 tick 전체를 로드하지 않고 앞/뒤 일부 line preview만 표시

검증:
- 사용자가 코드 수정 없이 sample setup을 선택해 실행
- invalid setup은 실행 전에 validation error 표시
- run 완료 후 report가 결과 화면에 나타난다
- UI는 simulator world object를 직접 참조하지 않음

구현 위치:
- [MainMenuPlayerController.h](x:/UE5/Proto-Unreal/Source/ProtoRobotSim/Public/Platform/MainMenuPlayerController.h): MainMenu widget 생성, viewport 부착, menu input mode 적용
- [MainMenuPlayerController.cpp](x:/UE5/Proto-Unreal/Source/ProtoRobotSim/Private/Platform/MainMenuPlayerController.cpp): `WBP_MainMenu` class resolution, widget lifecycle, cursor/input mode 관리
- [BP_MainMenuGameMode.uasset](x:/UE5/Proto-Unreal/Content/Blueprints/MainMenu/BP_MainMenuGameMode.uasset): MainMenuMap이 `AMainMenuPlayerController`를 사용하도록 연결하는 Blueprint GameMode
- [MainMenuWidget.h](x:/UE5/Proto-Unreal/Source/ProtoRobotSim/Public/Platform/Widget/MainMenuWidget.h): `WBP_MainMenu` binding 계약과 platform event handler
- [MainMenuWidget.cpp](x:/UE5/Proto-Unreal/Source/ProtoRobotSim/Private/Platform/Widget/MainMenuWidget.cpp): `BindWidget` control wiring, `Json/Input` setup 목록, fixed-step FPS 저장, run 시작, status/report/log preview 표시

수동/후속 확인:
- `MainMenuMap`의 World Settings에서 GameMode Override를 `BP_MainMenuGameMode`로 지정한다
- `WBP_MainMenu`를 직접 지정하려면 `MainMenuPlayerController`를 상속한 Blueprint에서 `MainWidgetClass`를 설정하고, GameMode의 Player Controller Class를 그 Blueprint로 지정한다
- `WBP_MainMenu`가 정식 MainMenu layout이다. `BindWidget` 이름/타입 불일치는 C++ fallback 없이 버그로 보고 수정한다
- 실제 MainMenuMap 화면 표시와 버튼 클릭은 visible run 또는 PIE에서 확인해야 한다
- measurement JSONL preview는 전체 tick load 대신 앞/뒤 일부 line preview만 표시한다. event 중심 필터링 UI는 T06 이후 사용성 확장에서 보강한다
- `MainMenuMap`은 Platform 메뉴와 3D 배경을 둘 수 있는 user-facing world로 유지한다. Platform UI는 이 world 위에 overlay로 표시하며, map이 비어 있거나 배경 actor가 있어도 같은 경로로 동작해야 한다

### T06 실험 설정 편집 구현 [x]

목표: 사용자가 Platform 안에서 실행할 simulation setup과 run queue를 구성한다.

의존:
- T05

상세 작업:
- `Json/Input`에서 `SimulationSetup`, `EpisodeSetup`, `DeliveryBotSetup`, `EpisodeRunQueue` 후보 파일을 분류해 selector에 표시
- `EpisodeRunQueue` editor에서 EpisodeSetup과 DeliveryBotSetup pair를 추가, 제거, 위/아래 이동
- `SimulationSetup` editor에서 map, run queue, fixed-step FPS, logging, report, status output 편집
- 새 queue 파일은 사용자가 입력한 path에 첫 pair를 추가하는 방식으로 생성
- 저장 전 `EpisodeSetup`은 `UEpisodeCompiler`, `DeliveryBotSetup`은 `UDeliveryBotSetupCompiler`, `SimulationSetup`은 `FSimulationSetupJson` 계약으로 validation

검증:
- 기존 EpisodeSetup과 DeliveryBotSetup pair를 queue에 추가
- 저장된 queue와 setup을 simulator runner가 그대로 실행
- validation 실패 항목은 파일과 run item 단위로 확인
- `ProtoRobotSim.SimulationSetup` automation과 `ProtoRobotSim.SimulatorLaunch` automation 통과

수동/후속 확인:
- 실제 `MainMenuMap` visible run 또는 PIE에서 `WBP_MainMenu`가 화면에 표시되고 버튼 클릭이 가능한지 확인
- `WBP_MainMenu` layout 변경 시 `UMainMenuWidget`의 `BindWidget` 이름과 타입을 맞춘다
- 새 SimulationSetup 파일 생성은 path 입력 후 `Save Setup`으로 가능하지만, MVP UI에는 별도 "Create Setup" 버튼을 두지 않음

### T07 EpisodeEditorMap 진입점 연결 [x]

목표: MainMenu에서 구현된 `EpisodeEditorMap`을 열 수 있게 한다.

의존:
- T05
- 에디터 UI 담당 작업자의 `EpisodeEditorMap` 기본 진입 lifecycle 확정

상세 작업:
- 에디터 UI 담당 작업자의 `EpisodeEditorMap` 진입 조건과 초기 로드 인자를 확인
- MainMenu에서 선택한 EpisodeSetup 파일을 기준으로 `EpisodeEditorMap`을 연다
- MVP에서는 같은 process 안에서 `OpenLevel`로 전환한다
- `UEpisodeEditorLaunchSubsystem`이 선택한 EpisodeSetup path를 보관하고 `EpisodeEditorMap` load 후 `AEpisodeEditorController::LoadEpisodeSetupJsonFile` 자동 호출을 시도한다
- MainMenuMap과 EpisodeEditorMap을 동시에 유지하는 별도 runtime window 구조는 MVP 범위에서 제외한다
- 후속으로 에디터를 별도 프로세스로 열 필요가 생기면 `-EditScenario=<EpisodeSetupFile>` 같은 명시적 command line 계약을 추가한다

검증:
- Platform에서 시나리오 에디터를 열 수 있는 진입점이 있다
- MainMenu에서 `EpisodeEditorMap`으로 전환해 에디터 담당 기능이 시작된다
- 에디터 진입 경로가 simulator fixed-step이나 runner 실행 경로에 영향을 주지 않는다
- `ProtoRobotSimEditor` build 통과

수동/후속 확인:
- `EpisodeEditorMap`의 GameMode/PlayerController가 `AEpisodeEditorController` 또는 그 Blueprint subclass를 사용해야 선택한 EpisodeSetup 자동 로드가 동작한다
- visible run 또는 PIE에서 `Open Editor` 버튼 클릭 후 `EpisodeEditorMap` 전환과 선택 EpisodeSetup 로드 상태를 확인
- 에디터에서 MainMenu로 돌아가는 버튼은 MVP 범위에 없음. 현재는 같은 process level 전환이므로 필요하면 후속으로 back navigation 또는 별도 editor process 방식을 선택

### T08 LLM 연동 후속 연결 [ ]

목표: 결과 분석과 파일 수정 제안을 Platform workflow에 연결한다.

의존:
- T05
- LLM server endpoint 계약 확정

상세 작업:
- LLM server endpoint settings를 Platform 설정 화면에 둔다
- evaluation report와 measurement log의 분석 payload builder 추가
- 결과 분석 report를 실험 결과 화면에 표시
- 제안 변경은 즉시 적용하지 않고 시나리오 에디터 또는 정책 편집 흐름으로 넘김

검증:
- evaluation report 기반 분석 리포트를 생성
- 제안 변경은 사용자가 검토한 뒤 별도 편집 흐름에서 적용
- 적용 후 같은 SimulationSetup으로 재실행

## 진행 현황

- [x] T01 실행 계약과 타입 고정
- [x] T02 Simulator bootstrap 구현
- [x] T03 결과와 상태 기록 연결
- [x] T04 Platform launcher 구현
- [x] T05 Platform 최소 실행 화면 구현
- [x] T06 실험 설정 편집 구현
- [x] T07 EpisodeEditorMap 진입점 연결
- [ ] T08 LLM 연동 후속 연결

## 검증

| 단계 | 검증 |
| --- | --- |
| T01 | `SimulationSetup` parser automation, command line parser automation |
| T02 | `SimulatorProcess` helper automation, `-Simulate=<SimulationSetupFile>` 실행, SimulatorMode fixed-step/FPS 적용 확인 |
| T03 | sample setup 실행 후 report/log/status 생성 확인 |
| T04 | Platform launcher command automation, process start/status polling 코드 경로 빌드 확인, visible subprocess smoke는 수동 확인 |
| T05 | Platform UI widget 빌드 확인, invalid JSON validation 표시 코드 경로 구현, visible UI smoke는 수동 확인 |
| T06 | `SimulationSetup` writer round-trip, `EpisodeRunQueue` writer/parser automation, Editor target build |
| T07 | `UEpisodeEditorLaunchSubsystem` compile 확인, visible `EpisodeEditorMap` 진입은 수동 확인 |
| T08 | LLM analysis payload와 report 표시 확인 |

## 완료 기록

### T01 완료

- `SimulationSetup JSON`, `Run Status JSON`, `-Simulate=<SimulationSetupFile>`, optional `-RunId=<RunId>` 계약 타입과 parser 추가
- `Json/Input/SimulationSetupSample.json` sample setup 추가
- sample parse, invalid field, missing file, command line parser, status read/write automation 추가

### T03 선행 작업

- `UEpisodeRunnerSubsystem` runner state/record completion native delegate와 total/completed/current pair 조회 API 추가
- `FEpisodeRunRecord`에 evaluation report JSON path 기록
- `UEpisodeMeasurementLogSubsystem`에 runtime logging settings 적용 API 추가

### T02/T03 완료

- `USimulatorProcessSubsystem`이 `-Simulate=<SimulationSetupFile>`를 감지해 simulator mode로 진입
- setup parse 후 fixed-step FPS 적용, target map load, `UEpisodeRunnerSubsystem::StartBatchFromRunQueueJsonFile` 실행 연결
- runner 상태 변화와 record 완료를 `Run Status JSON`으로 기록
- setup의 logging/report 설정을 simulator process 실행 흐름에 적용하고 report/log path를 status에 반영

### T04 완료

- `USimulatorLaunchSubsystem`으로 simulator subprocess command 조립과 실행 lifecycle 관리 추가
- packaged exe가 없을 때 `RunPreview.bat` fallback으로 같은 public command parameter 전달
- status file polling으로 `Pending`, `Running`, `Completed`, `Failed`, `Canceled` terminal state 추적
- process start failure와 simulator status failure를 분리해 UI가 진단할 수 있게 함

### T05 완료

- `AMainMenuPlayerController`가 MainMenu widget을 viewport에 붙이고 menu input mode를 관리
- `UMainMenuWidget`이 setup 목록, fixed-step FPS 저장, run 시작, status/report/log preview를 제공
- `BP_MainMenuGameMode`와 `WBP_MainMenu`를 추가하고 `MainMenuMap`에 연결
- Platform UI는 simulator world object를 직접 참조하지 않고 status/report/log 파일만 읽음

### T06 완료

- `FSimulationSetupJson`에 `SimulationSetup JSON` writer와 file save API 추가
- `USimulatorLaunchSubsystem`에 EpisodeSetup, DeliveryBotSetup, EpisodeRunQueue selector용 파일 분류 API 추가
- `USimulatorLaunchSubsystem`에 `EpisodeRunQueue JSON` read/write, pair 추가, 제거, 재정렬 API 추가
- `WBP_MainMenu` layout과 `UMainMenuWidget` C++ event handler로 setup 실행/저장 UI 제공
- queue 저장 전 EpisodeSetup/DeliveryBotSetup validation을 수행해 잘못된 pair 저장을 차단

### T07 완료

- `UEpisodeEditorLaunchSubsystem` 추가
- MainMenu에서 선택한 EpisodeSetup을 보관하고 `EpisodeEditorMap`을 `OpenLevel`로 연다
- `EpisodeEditorMap` load 후 PlayerController가 `AEpisodeEditorController`이면 선택한 EpisodeSetup 자동 로드를 시도
- 같은 process 안의 별도 runtime window 동시 실행은 MVP 범위에서 제외하고 Plan에 후속 선택지로 남김

### 현재 결과

T01~T07 완료.
Launcher process는 packaged exe 또는 개발 fallback `RunPreview.bat`를 별도 process로 실행한다.
Simulator process는 `SimulationSetup JSON` 기반 fixed-step run을 수행하고, Platform UI는 status/report/log 파일로 진행 상황과 결과를 조회한다.
MainMenu는 실험 설정과 run queue를 편집하고, 선택한 EpisodeSetup으로 `EpisodeEditorMap`에 진입할 수 있다.
남은 큰 범위는 T08 LLM 연동이다.
