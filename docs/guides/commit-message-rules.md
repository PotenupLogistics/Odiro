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

| Type       | 용도                                                                                             |
| ---------- | ------------------------------------------------------------------------------------------------ |
| `feat`     | feature, workflow, command, runtime behavior 추가                                                |
| `fix`      | bug, safety, routing, boundary, verification 수정                                                |
| `refactor` | 동작 변경 없는 구조 개선                                                                         |
| `test`     | 테스트, 검증 자산 추가/수정                                                                      |
| `spec`     | 요구사항, 기준, interface 정의                                                                   |
| `plan`     | 구현 방법, 순서, risk, verification 계획                                                         |
| `docs`     | README, guide, 운영/사용법, troubleshooting 설명 변경                                            |
| `asset`    | 코드가 아닌 정적 자산 변경: Unreal asset/map, sample, fixture, policy corpus, static UI resource |
| `build`    | build system, packaging, dependency 변경                                                         |
| `ci`       | CI/CD workflow 변경                                                                              |
| `chore`    | packaging, install, metadata, 기계적 유지보수                                                    |

## Scope

권장 scope 미사용 시 경고 출력한다.

| Scope      | 용도                                                |
| ---------- | --------------------------------------------------- |
| `client`   | Unreal Client                                       |
| `service`  | Go background service                               |
| `agents`   | Python Agent Server                                 |
| `contract` | 공유 schema, example, API contract                  |
| `tool`     | repository-wide setup, dev, verify, package tooling |
| `docs`     | 문서 구조와 문서 tooling                            |
| `ui`       | 사용자 화면, UMG, Dashboard UI                      |
| `sample`   | sample project, scenario, starter policy/example    |
| `git`      | Git ignore, hook, commit 규칙                       |

## 규칙

- Summary: 72자 이내, 마침표 없이, 영어 명령형
- Body: Summary 다음 줄 비우고 작성
- `AGENTS.md`, agent skill, hook은 실행/작업 동작을 바꾸므로 순수 설명 변경이 아니면 `docs`보다 `fix`, `feat`, `refactor`, `chore` 등 사용
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
.\setup.bat
```
