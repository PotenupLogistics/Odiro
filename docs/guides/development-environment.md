# 개발 환경

- `task-setup.bat`: 프로젝트 의존성 설치
- `tools/check-prerequisites.ps1`: 개발 환경 도구 확인

## 대상

| 영역     | 필요 도구                                                   |
| -------- | ----------------------------------------------------------- |
| `Agents` | `uv`                                                        |
| `Client` | Unreal Engine 5.7, Visual Studio 2022, Windows SDK, Git LFS |
| `Bridge` | Go                                                          |

## Git setup

처음 clone 후 실행:

```powershell
.\task-setup.bat
```

Git submodule 초기화, hook 설정, LFS lock 설정 수행.
완료 후 Unreal asset에 read-only 상태가 재적용된다.

```powershell
# 직접 재설정
git submodule update --init --recursive
.\tools\set-git-config.ps1

# 적용값
git config --local core.hooksPath .githooks
git config --local merge.ff false
git config --local lfs.locksverify true
git config --local lfs.setlockablereadonly true
```

### 확인

```powershell
git check-attr lockable -- Client/Content/<sample>.uasset
.\tools\set-git-config.ps1
```

`lockable: set`이어야 한다. `set-git-config.ps1`은 이미 적용된 설정은 `Already configured`로 표시하고, setup 완료 후 lock 전 Unreal asset에 read-only 상태를 재적용한다.

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
