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

- `Task-Build.bat`: Unreal 빌드
- `Task-Dev.bat`: Unreal Editor 실행
- `Task-RunPreview.bat`: Standalone PIE 실행 (`UnrealEditor.exe <uproject> -game -NoSplash`)
  - 시뮬레이터: `-Simulate=<SimulationSetupFile> -RunId=<RunId>`
- `Task-RunPythonPolicyServer.bat`: 로봇의 정책 실현 서버 `Tools\PythonAgent\server.py`를 `127.0.0.1:8000`에서 runtime policy mode로 실행
- `.run/GeneratePreviewConfigs.ps1`: Rider 시작 시 `.run/*.local.run.xml` Preview 실행 설정 생성
  - `OdiroSim.uproject`의 `EngineAssociation` 기반으로 로컬 Unreal Editor 경로 탐색
  - 생성된 `*.local.run.xml`은 사용자별 절대 경로를 담으므로 git에서 제외
- `.run/SetEnginePath.ps1`: `UE_INSTALL_DIR` Path Variable 수동 설정용 보조 스크립트
