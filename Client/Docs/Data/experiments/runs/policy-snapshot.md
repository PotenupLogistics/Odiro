# Run Policy Snapshot

경로:

```text
experiments/<Experiment>/runs/<RunId>/policy/
```

형식:

```text
copied policy package
```

상태: snapshot 원칙만 확정. include/exclude와 hash 산정 규칙은 별도 담당 범위에서 확정한다.

## 합의

- run 시작 시 `experiments/<Experiment>/policy/`를 복사한 snapshot이다.
- run 결과 해석과 재현성을 위해 실행 시점의 policy 상태를 보존한다.
- `summary.json`과 `result.json`의 `policy_snapshot_hash`가 이 snapshot을 참조한다.

## 현재 확정된 Schema

| 항목 | 합의 |
| --- | --- |
| source | `experiments/<Experiment>/policy/` |
| destination | `runs/<RunId>/policy/` |
| immutability | run 생성 후 수정하지 않는 결과물 |

## 추후 확정

| 항목 | 메모 |
| --- | --- |
| include/exclude rule | snapshot에 포함할 파일 규칙 |
| hash 산정 규칙 | canonical hash 규칙 |
| metadata | snapshot 생성 시각, source hash 등 기록 위치 |
