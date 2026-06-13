# ODIRO: 주행 로봇 시뮬레이션 · AI 분석 플랫폼

자연어 기반 시나리오 생성, Unreal 시뮬레이션 실행, 결과 분석, 정책 개선을 제공하는 시뮬레이션 플랫폼 프로젝트.

- Presentation: [[Proto]](https://canva.link/qd65gbp78g2tny7)

## 리포지토리 구성

| 구성 요소                        | 유형                   | 설명                                                  |
| ---------------------------- | ---------------------- | ----------------------------------------------------- |
| [`Client`](Client/README.md)     | Unreal 프로젝트        | 시뮬레이터, 시나리오 에디터, 클라이언트 구현          |
| [`Agents`](Agents/README.md)     | Python 서버            | 자연어 시나리오 생성, 시뮬 레이션 결과 분석 기능      |
| [`Bridge`](Bridge/README.md) | Go 백그라운드 프로세스 | 각 구현체 연결, 서브 프로세스 생성 및 추적, 파일 관리 |


[상세 구조](docs/specs/project-structure.md), [규칙](docs/specs/project-rules.md), [agent index](.agents/index/INDEX.md) 참고.

## 개발

### Setup

- Windows 10/11 64-bit
- `Client`: Unreal Engine 5.7, Visual Studio 2022
- `Agents`: `uv` with Python 3.12, OpenAI API key
- `Bridge`: Go (1.21 권장)

자세한 내용은 [개발 환경 설치 안내](docs/guides/development-environment.md) 참고.

```powershell
.\task-setup.bat   # 개발 환경 확인, git 설정, 의존성 설치
```

### 빌드 및 실행

```powershell
.\task-build.bat   # 전체 빌드
.\task-run.bat     # 프리뷰 세션 실행
.\task-dev.bat     # 개발 세션 (Hot Reload 지원 예정)
```
