# Bridge IPC 규약

Client, Agents, Simulator 간 IPC 통신을 위한 JSON-lines 송수신 규약.

## Transport

| OS           | Transport          | Address                                                                              |
| ------------ | ------------------ | ------------------------------------------------------------------------------------ |
| Windows      | Named pipe         | `\\.\pipe\odiro-bridge`                                                              |
| Linux, macOS | Unix domain socket | `$XDG_RUNTIME_DIR/odiro/odiro-bridge.sock` 또는 `/tmp/odiro-<uid>/odiro-bridge.sock` |

## Framing

- Encoding: UTF-8 JSON
- Framing: newline-delimited JSON
- Version: `1`

## 요청

```json
{
  "version": 1,
  "id": "request-1",
  "method": "ping",
  "params": {}
}
```

| Field     | Type   | Required | Notes     |
| --------- | ------ | -------- | --------- |
| `version` | number | yes      | 현재 `1`  |
| `id`      | string | yes      | 요청 ID   |
| `method`  | string | yes      | 명령 이름 |
| `params`  | object | no       | payload   |

## 응답

```json
{
  "version": 1,
  "id": "request-1",
  "ok": true,
  "result": {
    "status": "ok"
  }
}
```

Error response:

```json
{
  "version": 1,
  "id": "request-1",
  "ok": false,
  "error": {
    "code": "UNKNOWN_METHOD",
    "message": "unknown method \"missing\""
  }
}
```

| Field     | Type    | Required | Notes                                     |
| --------- | ------- | -------- | ----------------------------------------- |
| `version` | number  | yes      | 현재 `1`                                  |
| `id`      | string  | yes      | 요청 ID 반환                              |
| `ok`      | boolean | yes      | `true`면 `result`, `false`면 `error` 사용 |
| `result`  | object  | no       | method별 result                           |
| `error`   | object  | no       | 실패 시 안정적인 error shape              |

## Methods

### `ping`

Health check. CLI의 `--ping` 진단 옵션은 실행 중인 Bridge endpoint에 이 method를 보낸다.

Request:

```json
{"version":1,"id":"request-1","method":"ping"}
```

Response:

```json
{"version":1,"id":"request-1","ok":true,"result":{"status":"ok"}}
```

## Error Codes

| Code              | Meaning                                     |
| ----------------- | ------------------------------------------- |
| `INVALID_VERSION` | 지원하지 않는 protocol version              |
| `INVALID_REQUEST` | 필수 field 누락 등 boundary validation 실패 |
| `UNKNOWN_METHOD`  | 알 수 없는 method                           |
