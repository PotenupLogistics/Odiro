# Odiro 프로젝트 구조

Odiro는 자연어 기반 시나리오 생성, 시뮬레이션 실행, 결과 분석, 정책 개선을 하나의 개발/배포 단위로 묶는 시뮬레이션 플랫폼이다.

공통 구조와 소유권 규칙은 [project-rules.md](./project-rules.md)를 따른다.

## Repository

- 실행 또는 패키징 단위인 서브 프로젝트는 PascalCase를 사용한다.
- 개발 보조 폴더는 lowercase 또는 kebab-case를 사용한다.
- 루트 `docs`의 일반 문서 파일명은 kebab-case를 사용한다.
- 런타임에 사용하는 정적 파일은 각 프로젝트 내부에 둔다.
- top-level `Static` 폴더는 만들지 않는다.

```text
Odiro/
  Agents/                         # Python Agent Server
    app/                          # FastAPI app, models, services
    data/                         # RAG chunk, processed source, knowledge card
    docs/                         # Agent 전용 도메인 문서
    harness/                      # Agent 검증 harness
    schemas/                      # Agent 내부 JSON schema
    scripts/                      # Agent 전용 CLI/tooling
    tests/                        # Agent 단위/계약 테스트
    static/                       # Agents가 Release에서도 직접 사용하는 정적 파일
    main.py
    uv.lock
    pyproject.toml

  Client/                         # Unreal 프로젝트
    Config/
    Content/
    Plugins/
    Source/
    Static/                       # Client가 소유하고 Release에 포함하는 정적 파일
      PolicyRuntime/              # 사용자 행동 정책 Python과 Unreal을 연결하는 런타임 스크립트
    Tools/                        # Unreal 전용 개발 도구
    Docs/                         # Unreal 전용 문서
    OdiroSim.uproject
    RunPreview.bat                # 개발용 Controller 실행 wrapper

  Bridge/                        # Go background service
    go.mod
    cmd/
      OdiroHost/
        main.go
    internal/
      api/                        # Controller/Dashboard용 HTTP API
      agents/                     # Agents process 실행과 통신
      dashboard/                  # Dashboard serving
      process/                    # Simulator 등 child process 관리
      project/                    # User Directory 접근
      runs/                       # 실행 상태 추적
    static/
      Dashboard/                  # 개발용 Dashboard 원본. Release에서는 OdiroHost.exe에 embed
        index.html
        dashboard.js
        dashboard.css

  contracts/                      # 프로젝트 간 공유 인터페이스
    schemas/                      # 공유 JSON Schema
    examples/                     # 공유 예제 payload
    openapi/                      # 공개 HTTP API 명세가 필요할 경우

  docs/                           # 리포지토리 전체 개발 문서
    specs/                        # 현재 구조와 요구사항
    plans/                        # 변경 계획
    decisions/                    # 장기 의사결정
    guides/                       # 개발/운영 가이드

  tools/                          # 전체 프로젝트 개발 도구
    bootstrap.ps1                 # 의존성 확인 및 안내
    setup-git-hooks.ps1           # Git hook local 설정
    dev.ps1                       # 개발용 통합 실행
    package.ps1                   # Release 조립

  setup.bat                       # clone 직후 실행하는 Windows setup entrypoint

  tests/                          # 컴포넌트 간 통합 테스트
    integration/
    fixtures/

  build/                          # 자동 생성되는 빌드/패키징 output
```

## Release

```text
Release/
  OdiroHost.exe                # Go 단일 바이너리. Dashboard 정적 파일 embed

  Client/                         # Unreal 패키징 결과
    WindowsNoEditor/
      OdiroSim.exe                # 기본 실행: Controller + Scenario Editor
                                  # 옵션 실행: Simulator / FixedStep Mode
    Static/
      PolicyRuntime/              # Client/Static/PolicyRuntime에서 복사

  Agents/                         # Agent Runtime
    OdiroAgents.exe
    _internal/                    # PyInstaller onedir 내부 Python 실행 환경
    static/                       # Agents/static에서 복사. 필요 없으면 생략 가능
```

## 개발 실행

개발 중에는 사람이 여러 프로그램을 직접 켜지 않고 `tools/dev.ps1` 하나로 실행한다.

```powershell
.\tools\dev.ps1 preview
```

권장 동작:

1. `Bridge`를 `go run`으로 실행한다.
2. 개발 모드에서는 `--dashboard-dir ./Bridge/static/Dashboard` 옵션을 전달한다.
3. `Bridge`가 `Agents` 서버를 실행하거나, 초기 구현 단계에서는 `tools/dev.ps1`이 대신 실행한다.
4. `Bridge`와 `Agents` health check를 통과하면 `Client/RunPreview.bat`을 실행한다.
5. `Client`에는 `Bridge` 주소와 필요한 runtime 경로를 실행 인자로 전달한다.

```powershell
go run ./Bridge/cmd/OdiroHost --dashboard-dir ./Bridge/static/Dashboard
```

Release에서는 `--dashboard-dir`을 사용하지 않는다. `OdiroHost.exe`는 embed된 Dashboard 파일을 사용한다.

## 경로 처리 규칙

- Unreal `Client`는 Release 또는 Repository 상대 경로를 직접 추측하지 않는다.
- `Bridge`는 실행 환경에 맞는 경로를 계산하고 `Client` 실행 인자로 전달한다.
- `Client/Static/PolicyRuntime`은 개발 중과 Release에서 같은 소유권을 유지한다.
- 사용자 프로젝트 경로는 Release 폴더 내부가 아니라 사용자가 선택한 User Directory를 사용한다.
