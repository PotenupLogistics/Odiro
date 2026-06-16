# 프로젝트 구조

공통 구조와 소유권 규칙은 [프로젝트 규칙](./project-rules.md)을 따른다.

## Repository

- 실행, 패키징 단위인 서브 프로젝트는 PascalCase 사용
- 개발 보조 폴더는 kebab-case 사용

```sh
Odiro/
  .github/
    workflows/                    # GitHub Actions checks
      pull-request-check.yml      # PR merge 전 검증
      post-merge-task.yml         # main 반영 후 LFS unlock

  .githooks/                      # Git hooks

  .agents/                        # 프로젝트 관련 agent context
    index/                        # agent source index cards
    skills/
      ue5-dev/

  Agents/                         # --- Python Agent 서버 ---
    app/                          # FastAPI 구현체
    data/                         # Agents 데이터. TODO: static으로 이동
    docs/                         # Agent 도메인 문서
    harness/                      # Agent-local 검증 harness
    scripts/                      # Agent 전용 CLI/tooling. TODO: tools로 이동
    tools/                        # Agents 전용 보조 도구
    tests/                        # Agent 단위 테스트
    task-setup.bat                # uv sync 의존성 설치
    task-run.bat                  # Agents API server 실행
    task-dev.bat                  # 개발용으로 실행 (코드 변경 시 자동 재시작)
    main.py
    uv.lock
    pyproject.toml

  Client/                         # --- Unreal 프로젝트 ---
    Config/
    Content/
    Plugins/
    Source/
    Resources/
      policy-runtime.py           # 사용자 행동 정책 Python과 Unreal을 연결하는 런타임 스크립트
    Tools/                        # Unreal 전용 보조 도구
    Docs/                         # Unreal 전용 문서
    Task-Setup.bat                # 의존성 확인
    Task-Build.bat                # C++ 컴파일
    Task-Dev.bat                  # Unreal Editor 실행
    Task-RunPreview.bat           # 단독 모드 PIE 프리뷰 실행
    OdiroSim.uproject

  Bridge/                         # --- Go 백그라운드 서비스 ---
    go.mod
    README.md
    cmd/
      odirohost/
        main.go                   # host 실행 진입점
    internal/
      ipc/                        # IPC 전송 레이어
      protocol/                   # JSON 통신 프로토콜
      api/                        # Client, Agents, Simulator가 사용할 API
      process/                    # Simulator 등 child process 실행, 추적, lifecycle 관리
      workspace/                  # 사용자 project root, 파일 layout, 경로 검증 관리
    public/                       # Bridge가 사용하는 정적 파일 (예: HTML)
    tools/
    task-setup.bat
    task-build.bat
    task-run.bat

  contracts/                      # --- 공통 규약 및 인터페이스 정의 ---
    schemas/                      # 공유 JSON Schema
    specs/                        # 사람이 읽는 공유 contract spec
    examples/                     # 예제 payload
    openapi/                      # 공개 HTTP API 명세가 필요할 경우

  static/                         # --- 배포/초기화용 기본 리소스 ---
    agents/                       # Agents 런타임 데이터
    defaults/                     # 프로젝트 초기화 소스 bundle

  docs/                           # --- 리포지토리 전체 개발 문서 ---
    specs/                        # 현재 구조와 요구사항
    plans/                        # 변경 계획
    decisions/                    # 장기 의사결정
    guides/                       # 개발/운영 가이드

  tools/                          # 프로젝트 단위 도구
    set-git-config.ps1             # Git hook/LFS 설정
    manual-unlock.ps1              # human-only dangling lock 정리
  tests/                          # 통합 테스트
    integration/
    fixtures/

  build/                          # 패키징 결과물
    Release/

  task-setup.bat                  # 개발 환경 설정 및 의존성 설치
  task-build.bat                  # 전체 빌드
  task-run.bat                    # 패키징 없이 프리뷰 실행
  task-dev.bat                    # 개발용 hot reload 세션 실행

  README.md
  AGENTS.md
```

## Release

```sh
build/Release/
  OdiroHost.exe                   # 백그라운드 서비스
  resources/
    agents/                       # Agents 런타임 데이터
    defaults/                     # 프로젝트 초기화 소스 bundle
    policy-runtime.pyz             # 패키징된 Python 런타임

  Client/                         # Unreal 패키징 결과
    WindowsNoEditor/
      OdiroSim.exe

  Agents/                         # Agent 런타임
    OdiroAgents.exe               # 패키징된 Python 런타임
    _internal/                    # 패키징된 내부 모듈
```

## Project Structure

한 프로젝트는 하나의 시뮬레이션 구성을 나타내며, 시나리오, 행동 정책, 실행 결과를 포함한다.

```sh
<UserProject>/                    # 사용자 프로젝트 루트
  setting.json                    # 프로젝트 설정. FPS, seed, episode count 등
  profile.json                    # 시뮬레이션 환경 프로필 설정. 기존 DeliveryBotSetup 포함

  scenario.json                   # 편집 가능한 단일 시나리오. 랜덤 요소 가능. seed/count는 setting.json 소유

  policy/                         # 행동 정책
    __init__.py                   # entrypoint. 지정된 인터페이스로 구현해야 함
    <subscript>.py                # 파일 분리하고 __init__.py에서 import 가능

  runs/                           # --- 실행 결과 ---
    <000001>/                     # 실행할 때 폴더 생성
      snapshot/                   # 해당 실행에 사용된 입력 snapshot
        setting.json
        profile.json
        scenario.json
        policy/

      summary.json                # 총 실행 시간, 통계 등
      review/                     # AI 분석 결과 저장

      episodes/                   # 에피소드마다 결과 폴더 생성
        <000001>/
          scenario.json           # snapshot/scenario.json과 setting seed로 확정한 episode scenario
          actions.jsonl           # 로봇의 입출력 기록 (주기마다 센서 데이터, 현재 위치, 행동 변경 등)
          events.jsonl            # 발생 이벤트 (장애물 감지, 충돌 등)
          trace.jsonl             # 환경 정보 기록. 로봇이 못본 데이터 분석/리플레이에 활용
          result.json             # 실행 시간, 성공/실패, 충돌 횟수 등
          preview.png             # 대표 이벤트 이미지
          captures/               # 센서 데이터 이미지
```
