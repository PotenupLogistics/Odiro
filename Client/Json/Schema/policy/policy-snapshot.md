# policy/

Project policy package와 run snapshot policy package의 계약이다.

## 경로

```text
<UserProject>/policy/
runs/<RunId>/snapshot/policy/
```

## 필수 진입점

```text
policy/__init__.py:create_policy
```

## 진입점 규칙

| 항목 | 설명 |
| --- | --- |
| `create_policy()` | 인자 없이 호출 가능해야 한다. |
| 반환값 | `Client/Resources/policy-runtime.py`가 호출하는 policy object. |
| `start` | Scenario 시작 입력을 JSON 직렬화 가능한 dict로 받는다. |
| `decide` | Observation 입력을 JSON 직렬화 가능한 dict로 받고 action dict를 반환한다. |
| `end` | Episode 종료 입력을 JSON 직렬화 가능한 dict로 받는다. |

## Snapshot 규칙

- Run 시작 시 `<UserProject>/policy/` 전체를 `runs/<RunId>/snapshot/policy/`로 복사한다.
- Symlink는 금지한다.
- `__pycache__`, `.pyc`, `.pyo`는 snapshot에서 제외한다.
- `Client/Resources/policy-runtime.py`는 사용자 policy package에 포함하지 않는다.
- `summary.json`과 episode `result.json`의 `policy_snapshot_hash`가 실행 시점 policy 상태를 참조한다.
