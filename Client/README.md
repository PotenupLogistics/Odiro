# Odiro Client

주행 로봇 시뮬레이터, 시나리오 에디터를 포함한 클라이언트.

## Simulation Overview
- 주행 로봇은 시작 위치로부터 도착 위치까지 최적 경로 이동
- 돌발 이벤트: 장애물, 보행자, 신호 변경 등
- 실제 환경은 센서 관측 데이터 입력 (RGB, Depth, Semantic Segmentation 등)
- 행동 정책: Rule-based. 여러 Rule 동시 발생 가능, 이 때 센서 원본 대신 Aggregator 출력 평가로 우선순위 계산 후 선택 (Goal Vector, Obstacle Sector, Terrain Score, Robot State, Path Progress 등)
- AI-Agent 분석 데이터: 센서 출력, 평가 지표, 실패 케이스, near-miss 태그

> 센서 데이터 목록은 변동 가능

## Scripts

- `..\task-build.bat client`: Unreal 빌드
- `..\task-dev.bat -SkipAgents`: Unreal Editor 실행
- `..\task-run.bat -SkipAgents -SkipBridge -- <PreviewArgs>`: Standalone PIE 실행 (`UnrealEditor.exe <uproject> -game -NoSplash`)
  - user project run: `-OdiroProject=<UserProject> -RunId=<RunId> [-PolicyPort=<Port>]`
- Rider `.run/*.run.xml`: `task-setup.bat` 실행 시 `tools/sync-ide-run-configs.ps1`가 루트 VSCode 실행/태스크 흐름과 맞춰 생성
  - `Preview Services`: `Preview Mode` 디버깅 전 Agents API와 Bridge service 실행
  - `Preview Mode`: Rider Unreal 실행 설정으로 Client preview mode 실행. C++ 디버깅 가능
  - `Preview Mode With Flags`: preview flag 입력 후 Client preview mode 실행. C++ 디버깅 가능
