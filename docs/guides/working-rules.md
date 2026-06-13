# 작업 공통 규칙

## 기본 원칙

- 변경 전에 대상 파일과 주변 구조를 먼저 확인한다.
- 요청 범위를 벗어난 리팩터링은 하지 않는다.
- 기존 작업자의 변경을 되돌리지 않는다.
- 생성물, cache, build output은 source tree에 포함하지 않는다.
- 실행 경로를 코드에서 임의로 추측하지 않는다.
- 공통 인터페이스는 코드보다 `contracts`를 먼저 갱신한다.
- 문서, 구현, 테스트가 서로 다른 이야기를 하지 않게 한다.

## 문서 작업 규칙

루트 문서는 리포지토리 전체 규칙과 결정을 담는다.

```text
docs/specs/
docs/plans/
docs/decisions/
docs/guides/
```

컴포넌트 내부 문서는 해당 컴포넌트 전용 내용만 담는다.

```text
Client/Docs/
Agents/docs/
```

규칙:

- 전체 구조 변경은 `docs/specs/project-structure.md`에 반영한다.
- 공통 규칙 변경은 `docs/specs/project-rules.md`에 반영한다.
- 작업 방식 변경은 이 문서에 반영한다.
- 장기 의사결정은 `docs/decisions`로 승격할 수 있다.
- 루트 `docs`의 일반 문서 파일명은 kebab-case로 작성한다.
- 단순 Q&A를 문서 끝에 누적하지 않고 기존 섹션에 병합한다.

## 실행 작업 규칙

개발 실행 진입점은 하나로 유지한다.

```powershell
.\tools\dev.ps1 preview
```

목표:

- 작업자가 `Bridge`, `Agents`, `Client`를 각각 수동 실행하지 않는다.
- 개발 중 `OdiroHost.exe`를 매번 직접 build하지 않는다.
- `Bridge`는 개발 중 `go run`으로 실행한다.
- `Agents`는 `uv run`으로 실행한다.
- `Client`는 `Client/RunPreview.bat`을 통해 실행한다.

`tools/dev.ps1 preview` 권장 순서:

1. 필수 도구 존재 확인
2. `Bridge` 실행
3. `Agents` 실행
4. health check 대기
5. `Client/RunPreview.bat` 실행

## 경로 작업 규칙

Unreal `Client`는 다른 프로젝트 경로를 직접 계산하지 않는다.

금지:

```text
../Agents
../Bridge
../../contracts
현재 작업 디렉터리 기반 repository 추측
packaged exe 위치 기반 개발 경로 추측
```

허용:

```text
명시적 실행 인자
환경 변수
설정 파일
Bridge가 전달한 경로
```

`PolicyRuntime`은 `Client` 소유다.

```text
Client/Static/PolicyRuntime
```

## 인터페이스 작업 규칙

컴포넌트 경계는 명확히 둔다.

```text
Client  <-> Bridge
Bridge <-> Agents
Client  <-> PolicyRuntime
```

규칙:

- `Client`는 `Agents`를 직접 호출하지 않는다.
- `Agents`는 `Client` 내부 파일을 직접 수정하지 않는다.
- `Agents`가 생성한 결과를 User Directory에 최종 반영하는 책임은 `Bridge`가 가진다.
- 공유 JSON payload는 `contracts`의 schema/example과 맞아야 한다.
- contract 변경은 가능하면 하위 호환성을 유지한다.

## 검증 규칙

변경 후 가장 작은 검증을 수행한다.

예:

```powershell
# 문서
Get-Content -Raw docs/specs/project-structure.md

# Agents
cd Agents
uv run pytest

# Bridge
cd Bridge
go test ./...

# Client
Client/RunPreview.bat --dry-run
```

검증 실패 시 결과를 숨기지 않고 실패 명령과 원인을 기록한다.

## Git 작업 규칙

- commit, push, force operation은 명시 요청이 있을 때만 수행한다.
- history rewrite는 명시 요청과 사전 합의된 plan이 있을 때만 수행한다.
- 작업 전 dirty tree를 확인한다.
- unrelated change는 되돌리지 않는다.
- 대규모 이동 전 대상 경로와 제외 대상을 다시 확인한다.
- clone 직후 실행하는 repository setup entrypoint는 프로젝트 루트 `setup.bat`으로 둔다.
- `setup.bat`이 호출하는 실제 PowerShell script는 `tools/*.ps1`에 둔다.
- `tools/*.ps1`은 Windows PowerShell 2.0 호환 문법을 기본값으로 쓴다.
- setup 출력 색상은 ANSI escape 대신 PowerShell host color를 사용한다.
- Commit message 규칙은 [commit-message-rules.md](./commit-message-rules.md)를 따른다.

## 보안 규칙

Git에 포함하지 않는 항목:

```text
.env
.env.local
secrets/
*.pem
*.key
OAuth token
service account private key
LLM API key
```

외부 접속이 가능한 API는 기본적으로 다음을 요구한다.

- 명시적 enable 옵션
- 인증 또는 access token
- CORS 제한
- User Directory path validation
- 내부 경로와 stack trace 비노출
