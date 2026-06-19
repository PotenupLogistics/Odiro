# status.json

Bridge가 소유하는 run process 생명주기 상태 파일이다.

## 경로

```text
runs/<RunId>/status.json
```

## schema

```json
"run_status"
```

## Root Fields

| 필드 | 타입 | 필수 | 설명 |
| --- | --- | --- | --- |
| `schema` | string | 예 | 고정값 `run_status`. |
| `version` | number | 예 | 고정값 `1`. |
| `run` | object | 예 | Project path, run id, status path. |
| `process` | object | 예 | Simulator process 정보. |
| `state` | string | 예 | Process 생명주기 상태. |
| `started_at` | string | 예 | UTC start timestamp. |
| `updated_at` | string | 예 | UTC update timestamp. |
| `exited_at` | string | 종료 상태면 예 | UTC exit timestamp. |
| `error` | string | 실패 상태면 예 | 사람이 읽는 실패 설명. |

## run

| 필드 | 타입 | 필수 | 설명 |
| --- | --- | --- | --- |
| `project_path` | string | 예 | User project root path. |
| `run_id` | string | 예 | 6자리 decimal run id. |
| `status_path` | string | 예 | 이 status file path. |

## process

| 필드 | 타입 | 필수 | 설명 |
| --- | --- | --- | --- |
| `executable` | string | 예 | Simulator executable path 또는 launch target. |
| `process_id` | number or null | process 시작 후 예 | Child process id. |
| `policy_port` | number or null | 예 | Policy server port. 없으면 `null`. |
| `exit_code` | number or null | 종료 상태면 예 | Process exit code. 실행 중이면 `null`. |

## state

| 값 | 설명 |
| --- | --- |
| `starting` | Bridge가 process 시작 전 status file을 생성한 상태. |
| `running` | Child process 시작 성공. |
| `stopping` | Bridge가 종료 요청을 전송한 상태. |
| `exited` | Child process가 exit code 0으로 종료. |
| `failed` | Process 시작 또는 실행 실패. |

## 사용 규칙

- Bridge가 `status.json`을 생성하고 갱신한다.
- Simulator 결과 판단은 `summary.json`과 episode `result.json`을 기준으로 한다.
- `failed` 상태에서는 `error`를 사람이 읽을 수 있게 기록한다.
