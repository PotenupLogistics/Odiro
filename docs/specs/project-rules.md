# Odiro 프로젝트 규칙

## 목적

이 문서는 Odiro 시뮬레이션 플랫폼의 지속적인 repository 구조, 소유권, 실행, 배포 규칙을 정의한다.

구조 자체는 [project-structure.md](./project-structure.md)를 기준으로 한다.

## 이름 규칙

### File naming

파일명은 영역별 생태계 관례를 우선한다.

규칙:

- 문서 파일은 kebab-case를 기본값으로 둔다.
- 루트 관례 파일은 기존 대문자 관례를 유지한다: `AGENTS.md`, `README.md`, `LICENSE`.
- 계획 문서는 `PLAN-<title>.md` 형식을 유지하되 `<title>`은 kebab-case로 작성한다.
- C++ class/source 파일은 Unreal reflection type 이름과 맞춰 PascalCase를 사용한다.
- Unreal asset 파일은 Unreal prefix와 PascalCase 이름을 사용한다.
- Go, Python, web 파일은 각 언어와 framework 관례를 따른다.
- 기존 문서는 링크를 함께 고칠 수 있을 때 kebab-case로 정리한다.

예:

```text
docs/specs/project-rules.md
docs/guides/working-rules.md
docs/plans/PLAN-service-bootstrap.md
Client/Source/OdiroSim/DeliveryBotComponent.h
Client/Content/UI/WBP_MainMenu.uasset
```

### Top-level

실행 또는 패키징 단위인 서브 프로젝트는 PascalCase를 사용한다.

```text
Client/
Bridge/
Agents/
```

개발 보조 폴더와 자동 생성 output 폴더는 lowercase 또는 kebab-case를 사용한다.

```text
contracts/
docs/
tools/
tests/
build/
```

규칙:

- `Client`, `Bridge`, `Agents`는 Release에도 직접 대응되는 제품 구성 단위다.
- `contracts`, `docs`, `tools`, `tests`는 개발과 검증을 위한 source 영역이다.
- `build`는 자동 생성 output 영역이며 Git 추적 대상이 아니다.
- Unreal 프로젝트 내부는 Unreal 관례를 유지한다.
- Go와 Python 프로젝트 내부는 각 언어 생태계 관례를 유지한다.

### Singular / Plural

하나의 실행 주체 또는 패키징 단위는 singular를 기본값으로 둔다.

```text
Client
Bridge
```

여러 기능, 파일, 계약, 문서, 테스트 묶음은 plural를 기본값으로 둔다.

```text
Agents
contracts
docs
tools
tests
schemas
examples
fixtures
```

예외:

- `Agents`는 단일 실행 파일로 패키징될 수 있지만, 내부에 여러 agent 기능을 포함하므로 plural를 사용한다.
- Unreal 표준 폴더인 `Config`, `Content`, `Plugins`, `Source`는 Unreal 관례를 따른다.

## 서브 프로젝트 소유권

### Client

`Client`는 Unreal 프로젝트를 소유한다.

포함 대상:

- Unreal `Config`
- Unreal `Content`
- Unreal `Plugins`
- Unreal `Source`
- Unreal 전용 `Tools`
- Unreal 전용 `Docs`
- `Client`가 Release에서도 사용하는 정적 파일

`PolicyRuntime`은 별도 top-level 프로젝트가 아니다.

```text
Client/Static/PolicyRuntime/
```

`PolicyRuntime`은 Unreal `Client`가 사용자 Python interpreter로 실행하는 보조 런타임이다.

### Bridge

`Bridge`는 Go background service를 소유한다.

역할:

- Controller와 Dashboard용 HTTP API 제공
- Simulator 프로세스 실행 및 상태 추적
- Agents 프로세스 실행 및 통신
- User Directory 읽기/쓰기 API 제공
- Dashboard 정적 파일 serving

Release에서는 단일 바이너리로 배포한다.

```text
Release/OdiroHost.exe
```

### Agents

`Agents`는 Python Agent Server를 소유한다.

역할:

- 자연어 기반 시나리오 생성
- 정책 RAG 검색
- `WorldConfig` 생성과 검증
- `EpisodeSetup`, `DeliveryBotSetup`, `RunQueue` 생성
- 시뮬레이션 결과 분석
- 정책 개선안 또는 자동 정책 생성

`Agents`는 `Bridge`와 별도 프로세스로 실행한다.

## Static 규칙

각 서브 프로젝트가 Release에서도 사용하는 정적 파일은 해당 프로젝트 내부에 둔다.

```text
Client/Static/
Bridge/static/
Agents/static/
```

top-level `Static` 폴더는 만들지 않는다.

프로젝트별 규칙:

- `Client/Static`은 Unreal 관례와 기존 구조를 유지한다.
- `Client/Static/PolicyRuntime`은 `Client` 소유이며 Release에 복사한다.
- `Bridge/static/Dashboard`는 개발용 Dashboard 원본이다.
- `Bridge/static/Dashboard`는 Release에서 `OdiroHost.exe`에 embed한다.
- `Agents/static`은 Agents가 Release에서도 직접 읽는 파일이 있을 때만 사용한다.

`Bridge` 개발 모드에서는 Dashboard directory를 옵션으로 지정할 수 있다.

```powershell
go run ./Bridge/cmd/OdiroHost --dashboard-dir ./Bridge/static/Dashboard
```

Release에서는 `--dashboard-dir`을 사용하지 않는다.

## contracts 규칙

`contracts`는 여러 프로젝트가 공유하는 기계적 인터페이스를 보관한다.

포함 대상:

- JSON Schema
- OpenAPI 문서
- IPC message schema
- shared example payload
- 컴포넌트 간 호환성 테스트 fixture

포함하지 않는 대상:

- 사람이 읽는 제품 요구사항
- 구현 계획
- 팀별 내부 설계 문서
- 특정 프로젝트에서만 사용하는 private schema

구분:

```text
docs/specs/
  사람이 읽는 명세와 구조 설명

contracts/
  코드와 테스트가 참조하는 인터페이스 계약
```

공유 여부 기준:

- `Client`, `Bridge`, `Agents` 중 둘 이상이 참조하면 `contracts` 후보
- 한 프로젝트만 참조하면 해당 프로젝트 내부에 둔다

## docs 규칙

루트 `docs`는 리포지토리 전체 문서를 보관한다.

```text
docs/specs/
docs/plans/
docs/decisions/
docs/guides/
```

용도:

- `docs/specs`: 현재 구조, 요구사항, 데이터 모델
- `docs/plans`: time-boxed 변경 계획
- `docs/decisions`: 장기 의사결정
- `docs/guides`: 개발, 실행, 배포 가이드

컴포넌트 내부 `Docs` 또는 `docs`는 해당 컴포넌트 전용 문서를 보관한다.

예:

```text
Client/Docs/
Agents/docs/
```

규칙:

- 전체 프로젝트 구조와 공통 실행 흐름은 루트 `docs`에 둔다.
- Unreal 전용 문서는 `Client/Docs`에 둔다.
- Agent pipeline, provider, RAG, handoff 문서는 `Agents/docs`에 둔다.
- 컴포넌트 내부 문서는 도메인별 분류를 허용한다.
- 루트 `docs`는 성격별 분류를 기본값으로 둔다.

## 경로 처리 규칙

Unreal `Client`는 repository 또는 Release 상대 경로를 직접 추측하지 않는다.

경로 우선순위:

1. 실행 인자
2. 환경 변수
3. 설정 파일
4. 개발용 fallback autodetect

`Bridge`는 실행 환경에 맞는 경로를 계산하고 `Client` 실행 인자로 전달한다.

예:

```powershell
Client/RunPreview.bat `
  -OdiroHostUrl="http://127.0.0.1:3333" `
  -OdiroProjectRoot="X:\Odiro\samples\default-project" `
  -OdiroPolicyRuntime="X:\Odiro\Client\Static\PolicyRuntime"
```

Release 예:

```powershell
Client/WindowsNoEditor/OdiroSim.exe `
  -OdiroHostUrl="http://127.0.0.1:3333" `
  -OdiroProjectRoot="D:\OdiroProjects\Sample" `
  -OdiroPolicyRuntime="C:\Program Files\OdiroSim\Client\Static\PolicyRuntime"
```

금지:

- `GetCurrentDirectory()` 기준으로 repository layout 추측
- packaged Unreal 실행 파일 위치 기준으로 개발 경로 추측
- `../..` 같은 상대 경로로 다른 프로젝트 내부 파일 접근

## 개발 실행 규칙

개발 실행 진입점은 `tools/dev.ps1` 하나로 통일한다.

```powershell
.\tools\dev.ps1 preview
```

권장 동작:

1. `Bridge`를 `go run`으로 실행한다.
2. 개발 모드에서는 `--dashboard-dir ./Bridge/static/Dashboard`를 전달한다.
3. `Agents`를 `uv run`으로 실행한다.
4. `Bridge`와 `Agents` health check를 수행한다.
5. `Client/RunPreview.bat`을 실행한다.
6. `Client`에 Bridge endpoint, User Directory, runtime 경로를 인자로 전달한다.

개발 중에는 `OdiroHost.exe`를 수동 build하지 않는다.

Release 조립 단계에서만 `go build`를 사용한다.

## Release 규칙

Release 구조는 사용자 실행과 배포를 기준으로 최소화한다.

```text
Release/
  OdiroHost.exe
  Client/
  Agents/
```

규칙:

- `OdiroHost.exe`는 top-level 단일 바이너리다.
- Dashboard 정적 파일은 `OdiroHost.exe`에 embed한다.
- `Client/Static/PolicyRuntime`은 Release에 복사한다.
- `Agents`는 PyInstaller onedir 패키징을 기본값으로 둔다.
- 사용자 프로젝트 파일은 Release 내부에 저장하지 않는다.
