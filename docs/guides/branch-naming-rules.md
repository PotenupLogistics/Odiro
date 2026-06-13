# Branch 이름 규칙

Branch 이름은 무엇을 다루는 작업인지 알 수 있게 작성한다.

## Format

- lowercase kebab-case로 작성, segment는 `/`로 구분.
- 예약된 키워드는 단독 branch로 만들 수 없다.

```text
<topic>
<scope>/<topic>
<kind>/<topic>
<kind>/<scope>/<topic>
```

## Kind

작업 성격. 없을 경우 feature로 간주한다.

| Kind       | 용도                |
| ---------- | ------------------- |
| `fix`      | 버그 수정           |
| `refactor` | 동작 유지 구조 변경 |
| `hotfix`   | 긴급 수정           |
| `temp`     | 임시/폐기 가능 작업 |

## Scope

소유 영역.

| Scope      | 용도                               |
| ---------- | ---------------------------------- |
| `client`   | Unreal Client                      |
| `agent`    | Agent runtime                      |
| `contract` | shared schema, API contract        |
| `tool`     | repository-wide dev tool           |
| `bridge`   | orchestration, background workflow |

## Topic

작업 대상.

좋은 예시:

```text
robot
scenario
login-timeout
project-structure
context-engineering
```

나쁜 예시:

```text
wip
misc
update
changes
stuff
```

## 예

```text
client/robot
agent/scenario
bridge/simulator-manager
contract/log
context-engineering
```

특수 kind 작업:

```text
fix/login-timeout
fix/client/ui-crash
refactor/project-structure
hotfix/bridge/startup
temp/agent/experimental-feature
```

## Hook 설정

Clone 직후 실행:

```powershell
.\task-setup.bat
```
