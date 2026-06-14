# Policy Package

경로:

```text
experiments/<Experiment>/policy/
```

형식:

```text
Python package + config
```

상태: 폴더 구조만 확정. runtime interface와 config schema는 별도 담당 범위에서 확정한다.

## 합의

- experiment에서 실행할 행동 정책 원본이다.
- `__init__.py`가 runtime entrypoint다.
- 보조 Python 파일과 policy가 해석하는 설정 파일을 함께 둘 수 있다.
- run 시작 시 이 폴더는 `runs/<RunId>/policy/`로 snapshot된다.

## 현재 확정된 Schema

| 항목 | 합의 |
| --- | --- |
| `__init__.py` | 필수 entrypoint |
| `<subscript>.py` | 필요 시 파일 분리 가능 |
| `<config>.json` | policy가 자체 해석하는 설정 파일 |

## 추후 확정

| 항목 | 메모 |
| --- | --- |
| runtime interface | entrypoint 함수/class 계약 |
| config schema | policy config 파일 규칙 |
| dependency 규칙 | 외부 dependency 허용 범위 |
| snapshot hash | run 재현성에 사용할 hash 산정 규칙 |
