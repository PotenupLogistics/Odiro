# 개발 환경

- `task-setup.bat`: 프로젝트 의존성 설치
- `tools/check-prerequisites.ps1`: 개발 환경 도구 확인

Clone 후 실행:

```powershell
.\task-setup.bat
```

경고 발생 시 아래에서 누락된 도구를 설치하고 다시 실행한다.

## 대상

| 영역     | 필요 도구                                          |
| -------- | -------------------------------------------------- |
| `Agents` | `uv`                                               |
| `Client` | Unreal Engine 5.7, Visual Studio 2022, Windows SDK |
| `Bridge` | Go                                                 |
| 공통     | GitHub CLI (`gh`), Git LFS                         |

## Git Setup

LFS lock 작동을 위해 git config와 Unreal Editor LFS user 정보를 GitHub 계정과 맞춰야 한다.

```powershell
winget install GitHub.Cli    # GitHub CLI 설치
gh auth login -h github.com  # 로그인
```

## uv

`Agents`는 Python 환경을 `uv`로 관리한다.

```powershell
# 설치: 둘 중 하나 선택
winget install astral-sh.uv                 # WinGet
irm https://astral.sh/uv/install.ps1 | iex  # powershell

# 확인
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
.\Client\Tools\CheckPrerequisites.ps1
```

공식 문서: [Install Unreal Engine](https://dev.epicgames.com/documentation/unreal-engine/install-unreal-engine), [Offline Installer](https://dev.epicgames.com/documentation/unreal-engine/offline-installer-of-unreal-engine)

### Source Control 설정

Editor에서 checkout 기능을 위해 ProjectBorealis UEGitPlugin를 사용한다.

- 우측 하단 `Source Control` 클릭 > `Git LFS 2` 선택

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


### 확인

```powershell
.\Client\Tools\CheckPrerequisites.ps1
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
