# 모노리포 이전 계획

## 목적

기존 Unreal 프로젝트와 Agent 프로젝트를 Odiro monorepo의 초기 코드베이스로 편입한다.

이 계획은 Odiro 제품의 목적이 아니라 1회성 repository 구성 작업이다. Odiro의 지속적인 구조와 규칙은 `docs/specs`와 `docs/guides`를 기준으로 한다.

## 입력과 대상

```text
Unreal origin:   https://github.com/PotenupLogistics/Proto-Unreal.git
Agent origin:    https://github.com/PotenupLogistics/Proto-AI.git
Target origin:   https://github.com/PotenupLogistics/Odiro.git
Unreal worktree: X:\Temp\Proto-Unreal
Agent worktree:  X:\Temp\Proto-AI
Target repo:     X:\Odiro
```

```text
X:\Temp\Proto-Unreal -> Odiro/Client
X:\Temp\Proto-AI     -> Odiro/Agents
```

## 원칙

- 실제 이전은 Git history 보존을 위해 subtree 또는 prefix rewrite 후 merge 방식으로 수행한다.
- 단순 파일 복사로 history 이전을 대체하지 않는다.
- 기존 작업 중인 repository는 migration rewrite 대상으로 사용하지 않는다.
- `X:\UE5\Proto-Unreal`과 `X:\Proto-AI`는 이 작업에 관여하지 않는다.
- `X:\Temp\Proto-Unreal`과 `X:\Temp\Proto-AI`는 origin server에서 직접 clone한 별도 작업 repo다.
- migration 시작 기준은 각 Temp repo의 `main`에 `migrated-1` 같은 tag를 붙여 기록한다.
- integration 임시 branch는 Odiro repo에서 `migration/client`, `migration/agents`를 사용한다.
- merge 시 `--allow-unrelated-histories`를 사용할 수 있다.
- history 보존을 위해 squash merge는 사용하지 않는다.
- `git subtree` 사용은 허용한다.
- `git filter-repo`는 필수 도구가 아니지만, 이미 history에 들어간 생성물과 cache를 제거해야 할 때 사용한다.
- 이전 전후에 파일 소유권이 `Client`, `Agents`, `contracts`, `tools`, `docs` 중 어디인지 명확히 한다.
- 둘 이상의 프로젝트가 실제 runtime, test, API에서 참조하거나 외부 노출이 필요한 기계적 인터페이스는 우선 `Client` 또는 `Agents`에 원형 보존하고, 별도 commit에서 `contracts`로 승격한다.
- 한 프로젝트에서만 쓰는 파일은 해당 프로젝트 내부에 둔다.
- 프로젝트 전용 내용은 루트로 복제하지 않고, 루트에서는 위치를 찾기 위한 힌트만 둔다.
- `Client`와 `Agents` 양쪽에 중복된 제품 개념 문서는 루트에 새 canonical 문서를 만들고 기존 파일은 삭제 판단 전까지 보존한다.
- ProtoRobotSim/OdiroSim renaming은 migration과 분리해 별도 작업으로 진행한다.
- Git LFS 설정과 exclusive lock 정책은 migration과 분리해 별도 작업으로 진행한다.
- `.uasset`은 현재 계획에서 LFS object로 관리하지 않는다.
- 문서 이동과 정리는 원형 migration 후 별도 commit으로 진행한다.

## 제외 대상

```text
.git/
.venv/
__pycache__/
Binaries/
DerivedDataCache/
Intermediate/
Saved/
node_modules/
dist/
build/
Release/
local secrets
```

## 기술 계획

- 대상 branch는 별도 지시가 없으면 `X:\Odiro`의 `main`으로 둔다.
- Temp repo의 `main` 현재 commit에 `migrated-1` tag를 붙여 migration 시작점을 표시한다.
- `git subtree add --prefix=<target>`를 기본 통합 방식으로 사용한다.
- `git filter-repo`는 Temp repo history 안에 제외 대상이 이미 들어간 경우에만 사용한다.
- `Client`와 `Agents`는 원형 통합을 먼저 완료하고, contracts 승격과 문서 이동은 후속 commit으로 분리한다.
- `ProtoRobotSim` rename, Git LFS lock 설정, Bridge skeleton, packaging은 이 migration plan의 구현 범위가 아니다.

## 작업 과정

### T01 준비 상태 확인 [x]

목표: migration을 시작해도 되는 clean baseline을 확인한다.

상세 작업:

- `X:\Odiro`에서 현재 branch와 dirty tree를 확인한다.
- `X:\Temp\Proto-Unreal`과 `X:\Temp\Proto-AI`가 각각 기대한 origin URL을 바라보는지 확인한다.
- 두 Temp repo가 `main...origin/main` 기준인지 확인한다.
- `migration/client`, `migration/agents`, `migrated-*` 이름 충돌이 없는지 확인한다.
- `X:\Odiro`에 문서 작업 등 unrelated dirty work가 있으면 migration 전에 별도 commit 또는 명시적 보류 상태로 정리한다.

검증:

```powershell
git -C X:\Odiro status --short --branch
git -C X:\Temp\Proto-Unreal status --short --branch
git -C X:\Temp\Proto-Unreal remote -v
git -C X:\Temp\Proto-AI status --short --branch
git -C X:\Temp\Proto-AI remote -v
git -C X:\Temp\Proto-Unreal tag --list migrated-*
git -C X:\Temp\Proto-AI tag --list migrated-*
git -C X:\Odiro branch --list migration/client migration/agents
```

### T02 시작점 tag 기록 [x]

목표: 각 source repo의 어느 commit까지 Odiro에 편입했는지 추적 가능하게 만든다.

상세 작업:

- `X:\Temp\Proto-Unreal`의 `main` HEAD에 `migrated-1` tag를 생성한다.
- `X:\Temp\Proto-AI`의 `main` HEAD에 `migrated-1` tag를 생성한다.
- tag는 local 기록으로 시작한다. remote push는 별도 명시 요청이 있을 때만 한다.

검증:

```powershell
git -C X:\Temp\Proto-Unreal rev-parse main
git -C X:\Temp\Proto-Unreal rev-parse migrated-1
git -C X:\Temp\Proto-AI rev-parse main
git -C X:\Temp\Proto-AI rev-parse migrated-1
```

### T03 history 제외 대상 점검 [x]

목표: 통합 전 history에 생성물, cache, secret 후보가 있는지 확인한다.

상세 작업:

- 두 Temp repo에서 전체 history의 path 목록을 검사한다.
- 제외 대상이 현재 tree에만 있으면 `.gitignore` 기준으로 제외하고 통합하지 않는다.
- 제외 대상이 과거 commit history에 있으면 해당 Temp repo에서 `git filter-repo` 등으로 제거한 뒤 진행한다.
- secret 후보가 발견되면 migration을 중단하고 사용자에게 파일명과 commit 범위를 보고한다.

검증:

```powershell
git -C X:\Temp\Proto-Unreal rev-list --objects --all
git -C X:\Temp\Proto-AI rev-list --objects --all
```

판정 기준:

- `Binaries/`, `DerivedDataCache/`, `Intermediate/`, `Saved/`, `.venv/`, `__pycache__/`, `node_modules/`, `dist/`, `build/`, `Release/`가 history에 없거나 제거되어 있다.
- `.env`, `*.pem`, `*.key`, token, private key 후보가 history에 없다.

### T04 Client 원형 통합 [x]

목표: `Proto-Unreal` history를 보존한 채 `Client/` 아래에 통합한다.

상세 작업:

- `X:\Odiro`에서 `migration/client` branch를 만든다.
- `X:\Temp\Proto-Unreal`을 local remote로 추가하거나 직접 path로 참조한다.
- `git subtree add --prefix=Client` 방식으로 Temp repo의 `main`을 통합한다.
- 통합 후 `Client` 아래에 Unreal 프로젝트 원형이 유지되는지 확인한다.
- `ProtoRobotSim` rename이나 문서 재배치는 하지 않는다.

검증:

```powershell
git -C X:\Odiro status --short
git -C X:\Odiro log --oneline --decorate --graph --max-count=20
git -C X:\Odiro ls-tree --name-only HEAD Client
```

### T05 Agents 원형 통합 [x]

목표: `Proto-AI` history를 보존한 채 `Agents/` 아래에 통합한다.

상세 작업:

- `X:\Odiro`의 target baseline에서 `migration/agents` branch를 만든다.
- `X:\Temp\Proto-AI`를 local remote로 추가하거나 직접 path로 참조한다.
- `git subtree add --prefix=Agents` 방식으로 Temp repo의 `main`을 통합한다.
- 통합 후 `Agents` 아래에 Agent 프로젝트 원형이 유지되는지 확인한다.
- contracts 승격이나 문서 재배치는 하지 않는다.

검증:

```powershell
git -C X:\Odiro status --short
git -C X:\Odiro log --oneline --decorate --graph --max-count=20
git -C X:\Odiro ls-tree --name-only HEAD Agents
```

### T06 migration branch 병합 [x]

목표: `Client`와 `Agents` 통합 결과를 target branch에 history 보존 방식으로 합친다.

상세 작업:

- target branch를 확인한다.
- `migration/client`를 `--allow-unrelated-histories`로 merge한다.
- `migration/agents`를 `--allow-unrelated-histories`로 merge한다.
- squash merge는 사용하지 않는다.
- 충돌이 있으면 원형 보존을 우선하고, 구조 변경은 후속 commit으로 분리한다.

검증:

```powershell
git -C X:\Odiro log --oneline --decorate --graph --max-count=40
git -C X:\Odiro status --short
```

### T07 통합 후 구조 점검 [x]

목표: 원형 통합 결과가 Odiro 구조 규칙과 크게 어긋나지 않는지 확인한다.

상세 작업:

- `Client` 내부 Unreal 경로, `.uproject`, `Source`, `Content`, `Config` 존재를 확인한다.
- `Agents` 내부 Python project 파일, app/test/script 경로를 확인한다.
- `.gitignore`가 통합된 생성물을 계속 제외하는지 확인한다.
- map 문서와 `AGENTS.md`가 monorepo 기준으로 맞는지 확인한다.

검증:

```powershell
rg --files X:\Odiro\Client
rg --files X:\Odiro\Agents
git -C X:\Odiro check-ignore -v Client\Binaries\ Client\Intermediate\ Client\Saved\ build\
```

### T08 최소 실행 검증 [x]

목표: migration 직후 실행 가능한 검증과 구현 전 검증을 구분해 기록한다.

상세 작업:

- `Agents` 테스트가 가능한 상태면 실행한다.
- `Bridge`가 아직 없으면 `go test ./...`는 "not implemented"로 기록한다.
- `Client/Task-RunPreview.bat --dry-run`이 없거나 미구현이면 "not implemented"로 기록한다.
- 실패는 숨기지 않고 명령, exit code, 원인을 기록한다.

검증:

```powershell
cd X:\Odiro\Agents
uv run pytest

cd X:\Odiro\Bridge
go test ./...

cd X:\Odiro
Client\Task-RunPreview.bat --dry-run
```

## 진행 기록

- T01: done. `X:\Odiro` clean baseline을 `5d5ee01`로 커밋한 뒤 migration을 시작했다.
- T02: done. `X:\Temp\Proto-Unreal` `main`/`migrated-1` = `b0835c4a17f9f5eeaacc42bb911ceeefbd9d0dd8`; `X:\Temp\Proto-AI` `main`/`migrated-1` = `4dcffeb9af79c2e1aaafeeefa409093ef69a9852`.
- T03: done. `X:\Temp\Proto-Unreal` history의 `__pycache__` generated files는 `git filter-repo`로 제거했다. `X:\Temp\Proto-AI`에는 같은 제외 대상 history 항목이 없었다.
- T04: done. `migration/client` branch에서 `Proto-Unreal`을 `Client/` prefix로 subtree import했다. Import commit: `4c4f566`.
- T05: done. `migration/agents` branch에서 `Proto-AI`를 `Agents/` prefix로 subtree import했다. Import commit: `15095d8`.
- T06: done. `migration/client`, `migration/agents`를 `main`에 merge했다. Merge commits: `936a508`, `99e7525`.
- T07: done. 공통 JSON Schema는 `contracts/schemas`로 이동했고, UE 계약 문서는 `contracts/specs`로 이동했다. Follow-up commits: `3b6150f`, `0c572eb`, `69a6bbe`.
- T08: done with recorded limitations. Focused `Agents` pytest와 docs harness는 통과했다. Full `uv run pytest`는 현재 environment dependency issue인 `ModuleNotFoundError: No module named 'trio'`로 collection 단계에서 중단된다. `harness.checks.check_json_schemas`는 imported baseline data가 policy card 11개인 반면 harness 기대값이 9개라 실패한다.

## 검증

```powershell
git status --short --branch

cd Agents
$env:PYTEST_DISABLE_PLUGIN_AUTOLOAD='1'
uv run pytest tests/test_json_schemas.py tests/test_json_contract_validator.py tests/test_docs_inventory.py tests/test_ue_contract_docs.py
uv run python -m harness.checks.check_docs_inventory
uv run python -m harness.checks.check_ue_contract_docs
uv run python -m harness.checks.check_json_schemas
uv run pytest

cd ..\Bridge
go test ./...

cd ..
Client\Task-RunPreview.bat --dry-run
```

`Bridge`가 아직 없거나 `Client/Task-RunPreview.bat --dry-run`이 구현 전이면 해당 검증은 "not implemented"로 기록한다.

현재 검증 결과:

- `git status --short --branch`: clean on `main`.
- `$env:PYTEST_DISABLE_PLUGIN_AUTOLOAD='1'; uv run pytest tests/test_json_schemas.py tests/test_json_contract_validator.py tests/test_docs_inventory.py tests/test_ue_contract_docs.py`: `29 passed`.
- `uv run python -m harness.checks.check_docs_inventory`: passed.
- `uv run python -m harness.checks.check_ue_contract_docs`: passed.
- `uv run python -m harness.checks.check_json_schemas`: failed, `policy card count must remain 9` while current imported baseline has 11 cards.
- `$env:PYTEST_DISABLE_PLUGIN_AUTOLOAD='1'; uv run pytest`: failed during collection, `ModuleNotFoundError: No module named 'trio'` from FastAPI/anyio import path.
- `Bridge` and `Client\Task-RunPreview.bat --dry-run`: not implemented in current migration scope.

## 완료 기준

- `Client`와 `Agents`가 history를 보존한 상태로 monorepo에 들어온다.
- 자동 생성물, cache, build output, local secret이 포함되지 않는다.
- 각 Temp repo의 migration 시작 commit이 `migrated-1` 같은 tag로 표시된다.
- migration commit은 `Client` 원형 통합과 `Agents` 원형 통합을 구분한다.
- project-wide 규칙은 `docs/specs`와 `docs/guides`에 남고, migration 절차는 이 plan에만 남는다.
- contracts 승격 재검토, 중복 문서 canonical화, ProtoRobotSim renaming, Git LFS lock 설정, `task-run.bat` preview, `task-dev.bat` development loop, `Bridge` skeleton, packaging 작업은 별도 commit 또는 별도 plan으로 진행할 수 있다.
