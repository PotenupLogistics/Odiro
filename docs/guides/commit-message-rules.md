# 커밋 메시지 규칙

한 commit은 하나의 논리적 변경을 담당하며, 사용자/개발자 관점에서 그에 대한 메세지를 담는다.

## 형식

기본 형식:

```text
<type>: <imperative summary>

<why/context/tradeoff if useful>
```

Scope가 필요하면 type 뒤에 lowercase kebab-case로 붙인다.

```text
<type>(<scope>): <imperative summary>
```

## Type

Type은 지정된 형식만 허용한다.

| Type       | 용도                                                         |
| ---------- | ------------------------------------------------------------ |
| `feat`     | 기능 추가                                                    |
| `fix`      | 버그 수정                                                    |
| `refactor` | 동작 변경 없는 구조 개선                                     |
| `test`     | 테스트, 검증 추가/수정                                       |
| `spec`     | 요구사항, 기준, 인터페이스 정의                              |
| `plan`     | 구현 계획,                                                   |
| `docs`     | 문서 작업                                                    |
| `asset`    | 코드가 아닌 정적 파일 수정: `.uasset`, `.umap`, 이미지, 샘플 |
| `build`    | 빌드 시스템, 패키징, 의존성 변경                             |
| `ci`       | CI/CD 수정                                                   |
| `chore`    | 메타데이터, 기타                                             |

## Scope

권장 scope 미사용 시 경고 출력한다.

| Scope      | 용도                        |
| ---------- | --------------------------- |
| `agents`   | Agent 프로젝트              |
| `client`   | Unreal 프로젝트             |
| `bridge`   | 백그라운드 호스트           |
| `contract` | 공유 스키마, 예제, API 계약 |
| `tool`     | 도구                        |
| `docs`     | 문서                        |
| `ui`       | UI, UMG                     |
| `sample`   | 샘플 파일                   |
| `git`      | Git 설정                    |

## 규칙

- Summary: 72자 이내, 마침표 없이, 명령형
- Body: Summary 다음 줄 비우고 작성
- Git이 생성하는 `Merge ...`, `Revert "...` 는 예외로 허용

## 예

```text
docs: define project file naming rules
chore(git): enforce commit message format
spec(contract): define episode setup schema
plan(tool): outline setup dependency checks
asset(sample): add default pathfinding policy
refactor(docs): move migration procedure into plan
```

## Hook 설정

Clone 직후 실행:

```powershell
.\task-setup.bat
```
