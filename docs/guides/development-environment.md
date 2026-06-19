# 개발 환경

- `task-setup.bat`: 프로젝트 의존성 설치
- `tools/check-prerequisites.ps1`: 개발 환경 도구 확인

## 대상

| 영역        | 필요 도구                                                   |
| ----------- | ----------------------------------------------------------- |
| `Agents`    | `uv`                                                        |
| `Client`    | Unreal Engine 5.7, Visual Studio 2022, Windows SDK, Git LFS |
| `Bridge`    | Go                                                          |
| Git 계정/PR | GitHub CLI                                                  |

## Git setup

처음 clone 후 실행:

```powershell
winget install --id GitHub.cli -e
gh auth login -h github.com
.\task-setup.bat
```

이미 `gh`가 설치되어 있으면 설치 명령은 생략한다. `winget`을 사용할 수 없으면 [GitHub CLI](https://cli.github.com/)에서 설치한다.

Git submodule 초기화, hook 설정, GitHub 계정, Git commit identity, LFS 인증, Unreal Editor LFS user 설정을 수행한다.
완료 후 Unreal asset에 read-only 상태가 재적용되고, Editor asset 수정 전 checkout prompt가 켜진다.

직접 재설정:

```powershell
gh auth login -h github.com
.\tools\set-git-config.ps1
```

`set-git-config.ps1`은 `gh api user`의 GitHub login을 기준으로 repo-local `user.name`과 Unreal Editor `LfsUserName`을 맞춘다. `gh auth setup-git --hostname github.com`으로 Git/LFS HTTPS credential helper도 `gh` 인증에 맞춘다. GitHub 공개 email이 없으면 `<id>+<login>@users.noreply.github.com`을 `user.email`로 사용한다.

```powershell
# commit email을 직접 지정
.\tools\set-git-config.ps1 -Email <GitHub commit email>
```

```powershell
git submodule update --init --recursive
.\tools\set-git-config.ps1

# 적용값
git config --local core.hooksPath .githooks
git config --local merge.ff false
git config --local pull.ff true
git config --local pull.rebase true
git config --local rebase.autoStash true
git config --local branch.autoSetupRebase always
git config --local lfs.locksverify true
git config --local lfs.setlockablereadonly true
git config --local user.name <GitHub login>
git config --local user.email <GitHub commit email>
```

Editor user 설정도 보정된다. Unreal Editor `LfsUserName`이 GitHub login 또는 LFS lock owner와 다르면 경고한다.
계정 경고가 뜨면 수동 수정 대신 `.\tools\set-git-config.ps1`를 다시 실행한다.

```ini
[/Script/UnrealEd.EditorLoadingSavingSettings]
bAutomaticallyCheckoutOnAssetModification=False
bPromptForCheckoutOnAssetModification=True
```

### 확인

```powershell
git check-attr lockable -- Client/Content/<sample>.uasset
.\tools\set-git-config.ps1
```

`lockable: set`이어야 한다. `set-git-config.ps1`은 필요한 설정 변경과 완료 문구만 출력하고, setup 완료 후 lock 전 Unreal asset에 read-only 상태와 Editor checkout prompt를 재적용한다. 이미 맞는 상태의 성공 검증은 출력하지 않는다.
GitHub CLI 인증이 없으면 실패한다. LFS lock owner가 GitHub login과 다르면 경고한다. Pull 후 hook에도 같은 실패 또는 경고가 표시될 수 있다.
`git fetch`에는 no-op fetch까지 항상 실행되는 표준 hook이 없으므로 fetch 시점 강제 검사는 사용하지 않는다.

GitHub repository 설정:

- Secret: `LFS_LOCK_BOT_TOKEN`
- Optional variable: `LFS_LOCK_OWNER_ALIASES`, 예: `github-login=Lock Owner Name,other-alias`

## uv

`Agents`는 Python 환경을 `uv`로 관리한다.

### 설치

```powershell
# 둘 중 하나 선택
winget install astral-sh.uv                 # WinGet
irm https://astral.sh/uv/install.ps1 | iex  # powershell
```

### 확인

```powershell
uv --version
```

공식 문서: [uv installation](https://docs.astral.sh/uv/getting-started/installation/)

## Unreal Engine

`Client` 실행을 위해 Unreal Engine 5.7이 필요하다.

### 설치

1. [Epic Games Launcher](https://dev.epicgames.com/documentation/unreal-engine/install-unreal-engine) 설치
2. `Unreal Engine > Library > Engine Versions`에서 `5.7` 추가

### 확인

```powershell
.\Client\Tools\CheckPrerequisites.ps1 -AllowMissing
```

기본 경로가 아니면 다음 중 하나를 설정한다.

```powershell
$env:UE_ENGINE_DIR = "D:\Epic Games\UE_5.7"
$env:UE_EDITOR_EXE = "D:\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor.exe"
```

공식 문서: [Install Unreal Engine](https://dev.epicgames.com/documentation/unreal-engine/install-unreal-engine), [Offline Installer](https://dev.epicgames.com/documentation/unreal-engine/offline-installer-of-unreal-engine)

### Source Control

ProjectBorealis UEGitPlugin를 사용한다.

- Editor source control provider: `Git LFS 2`
- 프로젝트 정책: `lockable`만 사용, LFS object 저장 금지
  - 허용: `Checkout`
  - 금지: `Submit`, `Push`, `Unlock`, repository initialize, auto-create `.gitattributes`

## Visual Studio 2022

UE 5.7은 Visual Studio 2022 17.8 이상을 지원하고 17.14를 권장한다.

### 설치

1. [Visual Studio Installer](https://aka.ms/vs/17/release/vs_community.exe) 설치 및 실행
2. 워크로드 탭에서 다음 항목 선택
   - 데스크톱 및 모바일
     - .NET 데스크톱 개발
     - WinUI 애플리케이션 개발
     - C++를 사용한 데스크톱 개발
   - 게임 개발
     - C++를 사용한 게임 개발


확인:

```powershell
.\Client\Tools\CheckPrerequisites.ps1 -AllowMissing
```

공식 문서: [Epic: Visual Studio 구성하기](https://dev.epicgames.com/documentation/unreal-engine/setting-up-visual-studio-development-environment-for-cplusplus-projects-in-unreal-engine), [Visual Studio Tools for Unreal Engine](https://learn.microsoft.com/en-us/visualstudio/gamedev/unreal/get-started/vs-tools-unreal-install)

## Go

`Bridge` 빌드를 위해 [Go](https://go.dev/)가 필요하다.

### 설치

- 공식 MSI Installer: [Go for Windows](https://go.dev/dl/)
- WinGet: `winget install --id GoLang.Go -e --source winget`

### 확인

```powershell
go version
```

공식 문서: [Download and install Go](https://go.dev/doc/install)
