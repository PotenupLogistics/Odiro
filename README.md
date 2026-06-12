# Delivery Bot Simulator Prototype

## Overview
- 주행 로봇은 시작 위치로부터 도착 위치까지 최적 경로 이동
- 돌발 이벤트: 장애물, 보행자, 신호 변경 등
- 실제 환경은 센서 관측 데이터 입력 (RGB, Depth, Semantic Segmentation 등)
- 행동 정책: Rule-based. 여러 Rule 동시 발생 가능, 이 때 센서 원본 대신 Aggregator 출력 평가로 우선순위 계산 후 선택 (Goal Vector, Obstacle Sector, Terrain Score, Robot State, Path Progress 등)
- AI-Agent 분석 데이터: 센서 출력, 평가 지표, 실패 케이스, near-miss 태그

> 센서 데이터 목록은 변동 가능

## Project Contracts
- Simulation authority: 최종 Dedicated Server, MVP는 Standalone PIE
- Server responsibilities: Delivery Bot 이동, 충돌 판정, 정책 평가, metric 기록
- Observer boundary: 관찰과 UI 명령 요청만 허용
- Policy input: 센서 원본이 아닌 Observation Aggregator 출력
- Sensor contract: 변동 가능 목록, Aggregator 출력 계약

## Source Mapping
| Area | Description |
| --- | --- |
| DeliveryBot | 주행 로봇 Actor/Component/Subsystem. 이동/경로 추종/회피/정책 판단 |
| Episode | JSON 에피소드 컴파일, 런타임 스폰/조회, 에피소드 Actor/Component, 실행/측정/평가 흐름 |
| Platform | MainMenu UI, simulator subprocess 실행, `-Simulate=<SimulationSetupFile>` 기반 simulator process bootstrap, status polling |
| Shared | Episode, Simulation, DeliveryBot 사이의 공유 타입, 실행 설정, 리플레이, 측정 로그, 시나리오 스펙 |

## Scripts
- `BuildProject.bat`: PowerShell/Rider/Visual Studio 없이 UnrealBuildTool로 프로젝트 빌드
  - 기본값: `ProtoRobotSimEditor Win64 Development`
  - 예: `BuildProject.bat -Target Game -Configuration Development`
  - `UE_ENGINE_DIR`, `UE_EDITOR_EXE`, `PATH`, `ProtoRobotSim.uproject`의 `EngineAssociation` 순서로 로컬 Unreal Engine 경로 탐색
- `RunPreview.bat`: 패키징 프리뷰 (`UnrealEditor.exe <uproject> -game -NoSplash`)
  - `-Simulate=<SimulationSetupFile> -RunId=<RunId>` 로 시뮬레이터 실행
- `RunPythonPolicyServer.bat`: 로봇의 정책 실현 서버 `Tools\PythonPolicyServer\server.py`를 `127.0.0.1:8000`에서 runtime policy mode로 실행한다.
- `RunLlmServer.bat`: `Proto-AI` FastAPI LLM authoring server를 `127.0.0.1:8711`에서 실행한다. 생성 JSON 산출물은 배치 파일 위치 기준 프로젝트 루트의 `Json\Input`으로 저장되도록 설정한다. `OPENAI_API_KEY`는 환경변수로 미리 설정하거나 실행 시 입력한다.
- `.run/GeneratePreviewConfigs.ps1`: Rider 시작 시 `.run/*.local.run.xml` Preview 실행 설정 생성
  - `ProtoRobotSim.uproject`의 `EngineAssociation` 기반으로 로컬 Unreal Editor 경로 탐색
  - 생성된 `*.local.run.xml`은 사용자별 절대 경로를 담으므로 git에서 제외
- `.run/SetEnginePath.ps1`: `UE_INSTALL_DIR` Path Variable 수동 설정용 보조 스크립트
