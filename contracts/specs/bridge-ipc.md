# Bridge IPC 규약

Client, Agents, Simulator 간 JSON-lines IPC 송수신 규약.
사용자 project 파일 형식: [User Project Data Contract](./user-project-data.md)

## 전송

| OS           | 전송               | 주소                                                                                 |
| ------------ | ------------------ | ------------------------------------------------------------------------------------ |
| Windows      | Named pipe         | `\\.\pipe\odiro-bridge`                                                              |
| Linux, macOS | Unix domain socket | `$XDG_RUNTIME_DIR/odiro/odiro-bridge.sock` 또는 `/tmp/odiro-<uid>/odiro-bridge.sock` |

## 메시지 구분

- 인코딩: UTF-8 JSON
- 구분 방식: newline-delimited JSON
- 버전: `1`

## 요청

```json
{
  "version": 1,
  "id": "request-1",
  "method": "ping",
  "params": {}
}
```

| 항목      | 형식   | 필수 | 설명      |
| --------- | ------ | ---- | --------- |
| `version` | number | yes  | 현재 `1`  |
| `id`      | string | yes  | 요청 ID   |
| `method`  | string | yes  | 명령 이름 |
| `params`  | object | no   | payload   |

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

오류 응답:

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

| 항목      | 형식    | 필수 | 설명                                      |
| --------- | ------- | ---- | ----------------------------------------- |
| `version` | number  | yes  | 현재 `1`                                  |
| `id`      | string  | yes  | 요청 ID 반환                              |
| `ok`      | boolean | yes  | `true`면 `result`, `false`면 `error` 사용 |
| `result`  | object  | no   | method별 result                           |
| `error`   | object  | no   | 실패 시 안정적인 오류 형식                |

## Methods

### `ping`

상태 확인.

- CLI `--ping`: 실행 중인 Bridge endpoint에 `ping` 전송

요청:

```json
{"version":1,"id":"request-1","method":"ping"}
```

응답:

```json
{"version":1,"id":"request-1","ok":true,"result":{"status":"ok"}}
```

### `workspace.validateProject`

사용자 project root 구조 검증.

요청:

```json
{
  "version": 1,
  "id": "request-1",
  "method": "workspace.validateProject",
  "params": {
    "projectPath": "X:/Projects/DeliveryBotA"
  }
}
```

응답 result:

```json
{
  "projectPath": "X:/Projects/DeliveryBotA",
  "settingPath": "X:/Projects/DeliveryBotA/setting.json",
  "profilePath": "X:/Projects/DeliveryBotA/profile.json",
  "scenarioPath": "X:/Projects/DeliveryBotA/scenario.json",
  "policyPath": "X:/Projects/DeliveryBotA/policy",
  "runsPath": "X:/Projects/DeliveryBotA/runs"
}
```

검증 기준:

- 필수 file: `setting.json`, `profile.json`, `scenario.json`
- project input JSON: [User Project Data Contract](./user-project-data.md)의 최상위 형식 검증
- 필수 policy 진입점: `policy/__init__.py:create_policy`
- 필수 path: symlink 불가

### `workspace.listProjectPresets`

사용 가능한 project preset id 목록 조회.

요청:

```json
{
  "version": 1,
  "id": "request-1",
  "method": "workspace.listProjectPresets"
}
```

응답 result:

```json
{
  "scenarioPresetIds": ["blank", "barricade", "curved", "s-curve"],
  "profilePresetIds": ["basic", "full"],
  "policyPresetIds": ["blank", "demo"],
  "scenarioPresets": [
    {
      "id": "blank",
      "kind": "scenario",
      "title": "기초 구성",
      "subtitle": "짧은 직선 보도",
      "description": "새 프로젝트를 시작하기 위한 최소 시나리오입니다.",
      "thumbnailPath": "X:/Odiro/static/presets/scenario/blank/thumbnail.png",
      "sortOrder": 10
    }
  ],
  "profilePresets": [],
  "policyPresets": []
}
```

규칙:

- preset id source: 실행 resource의 `presets/` category
- 개발 source: `static/presets/{scenario/<PresetId>,profile/<PresetId>,policy/<PresetId>}/`
- release resource: `resources/presets/{scenario/<PresetId>,profile/<PresetId>,policy/<PresetId>}/`
- preset id: 각 category의 직접 하위 folder 이름
- `manifest.json`: UI metadata, `id`는 folder 이름과 일치
- `thumbnail.png`: 같은 preset folder의 optional card thumbnail
- preset id: 안전한 단일 경로 조각
- preset id 금지값: 경로 구분자, `..`, 절대 경로
- `workspace.createProject`: 선택 preset 조합 계약 검증

### `workspace.createProject`

새 project root 또는 빈 directory에 선택한 project preset 조합 복사.

요청:

```json
{
  "version": 1,
  "id": "request-1",
  "method": "workspace.createProject",
  "params": {
    "projectPath": "X:/Projects/DeliveryBotA",
    "presetSelection": {
      "scenarioPresetId": "blank",
      "profilePresetId": "basic",
      "policyPresetId": "blank"
    }
  }
}
```

응답 result:

```json
{
  "project": {
    "presetSelection": {
      "scenarioPresetId": "blank",
      "profilePresetId": "basic",
      "policyPresetId": "blank"
    },
    "projectPath": "X:/Projects/DeliveryBotA",
    "settingPath": "X:/Projects/DeliveryBotA/setting.json",
    "profilePath": "X:/Projects/DeliveryBotA/profile.json",
    "scenarioPath": "X:/Projects/DeliveryBotA/scenario.json",
    "policyPath": "X:/Projects/DeliveryBotA/policy",
    "runsPath": "X:/Projects/DeliveryBotA/runs"
  },
  "createdPaths": [
    "policy/__init__.py",
    "profile.json",
    "scenario.json",
    "setting.json"
  ]
}
```

`createdPaths`: 복사된 모든 file의 project-relative path. 예시는 축약형.

규칙:

- target directory: 비어 있지 않으면 거부
- `presetSelection.scenarioPresetId`, `profilePresetId`, `policyPresetId` 필수
- 지원 preset id 목록: `workspace.listProjectPresets`
- target project root: preset source와 같거나 서로의 하위 경로이면 `INVALID_REQUEST`
- preset source: `static/presets/` 또는 `resources/presets/`
- preset 검증: 복사 전 [User Project Data Contract](./user-project-data.md)의 project preset contract
- preset 누락/불완전: `PROJECT_PRESET_INVALID`, target project 미생성
- preset 금지 file: `Client/Resources/policy-runtime.py` 같은 runtime file 또는 tool 문서
- preset 금지 항목 위반: `PROJECT_PRESET_INVALID`, target project 미생성
- `runs/`: project 생성 시 빈 directory 생성
- copy 제외: `manifest.json`, `thumbnail.png`, `__pycache__`, `.pyc`, `.pyo`

### `workspace.createRun`

다음 run id 생성, run 기본 폴더와 project 입력 snapshot 작성.

요청:

```json
{
  "version": 1,
  "id": "request-1",
  "method": "workspace.createRun",
  "params": {
    "projectPath": "X:/Projects/DeliveryBotA"
  }
}
```

응답 result:

```json
{
  "projectPath": "X:/Projects/DeliveryBotA",
  "runId": "000001",
  "runPath": "X:/Projects/DeliveryBotA/runs/000001",
  "snapshotPath": "X:/Projects/DeliveryBotA/runs/000001/snapshot",
  "statusPath": "X:/Projects/DeliveryBotA/runs/000001/status.json",
  "summaryPath": "X:/Projects/DeliveryBotA/runs/000001/summary.json",
  "reviewPath": "X:/Projects/DeliveryBotA/runs/000001/review",
  "episodesPath": "X:/Projects/DeliveryBotA/runs/000001/episodes",
  "snapshotPaths": [
    "X:/Projects/DeliveryBotA/runs/000001/snapshot/policy/__init__.py",
    "X:/Projects/DeliveryBotA/runs/000001/snapshot/profile.json",
    "X:/Projects/DeliveryBotA/runs/000001/snapshot/scenario.json",
    "X:/Projects/DeliveryBotA/runs/000001/snapshot/setting.json"
  ]
}
```

규칙:

- run id: 6자리 decimal, 기존 run directory max + 1
- run id overflow: `999999` 이후 `PROJECT_INVALID`
- 새 run directory: static run 기본 폴더 먼저 복사
- run 기본 폴더 개발/release 위치와 허용/금지 내용: [User Project Data Contract](./user-project-data.md)의 Run 기본 폴더 섹션
- snapshot 대상: `setting.json`, `profile.json`, `scenario.json`, `policy/`
- snapshot `policy/` 필수 진입점: `__init__.py:create_policy`
- `statusPath`: Bridge가 `process.startSimulator`에서 갱신할 run status 파일 위치. `workspace.createRun` 생성 대상 아님
- `summaryPath`: Simulator가 run 종료 시 작성할 run summary. `workspace.createRun` 생성 대상 아님
- `snapshotPaths`: 복사된 snapshot file의 absolute path 목록
- snapshot `policy/` copy 제외: `__pycache__`, `.pyc`, `.pyo`
- run 기본 폴더 copy 제외: `.gitkeep`
- 이전 queue 파일 생성 없음

### `process.startSimulator`

생성된 run snapshot을 simulator 자식 process로 실행.

요청:

```json
{
  "version": 1,
  "id": "request-1",
  "method": "process.startSimulator",
  "params": {
    "projectPath": "X:/Projects/DeliveryBotA",
    "runId": "000001",
    "policyPort": 18124
  }
}
```

Simulator command:

```powershell
OdiroSim.exe -OdiroProject="X:/Projects/DeliveryBotA" -RunId="000001" -PolicyPort=18124
```

`policyPort`: 선택 field. 생략 시 simulator 기본 DeliveryBot Python policy port 사용.

응답 result:

```json
{
  "runId": "000001",
  "projectPath": "X:/Projects/DeliveryBotA",
  "executable": "X:/Odiro/build/Release/Client/WindowsNoEditor/OdiroSim.exe",
  "processId": 12000,
  "policyPort": 18124,
  "statusPath": "X:/Projects/DeliveryBotA/runs/000001/status.json",
  "state": "running",
  "startedAt": "2026-06-16T00:00:00Z",
  "updatedAt": "2026-06-16T00:00:00Z"
}
```

- `statusPath`: Bridge가 쓰는 run status 파일. 형식: [User Project Data Contract](./user-project-data.md)의 `run_status`
- `runId`: `workspace.createRun`이 반환한 6자리 decimal string만 허용
- `runId` 금지값: 경로 구분자, `..`, 임의 이름. 위반 시 `INVALID_REQUEST`
- process 시작 전 검증 기준: 이미 생성된 run snapshot
- process 시작 전 검증 제외: project root 원본 입력 재읽기
- 필수 항목: `runs/<RunId>/snapshot/setting.json`, `profile.json`, `scenario.json`, `snapshot/policy/__init__.py:create_policy`, `review/`, `episodes/`
- snapshot JSON 검증: [User Project Data Contract](./user-project-data.md)의 root contract
- 실패 조건: 누락, symlink, invalid JSON, schema mismatch, 지원하지 않는 version, root field 누락
- 실패 결과: `PROJECT_INVALID`, `status.json` 미생성

### `process.getRunStatus`

Bridge 추적 중 simulator process 상태 조회.

요청:

```json
{
  "version": 1,
  "id": "request-1",
  "method": "process.getRunStatus",
  "params": {
    "projectPath": "X:/Projects/DeliveryBotA",
    "runId": "000001"
  }
}
```

- 응답 result: `process.startSimulator` 응답 result와 같은 형식
- `runId` 형식 규칙: `process.startSimulator`와 동일

### `process.stopSimulator`

Bridge 추적 중 simulator process 종료 요청.

요청:

```json
{
  "version": 1,
  "id": "request-1",
  "method": "process.stopSimulator",
  "params": {
    "projectPath": "X:/Projects/DeliveryBotA",
    "runId": "000001"
  }
}
```

- 응답 result: `process.startSimulator` 응답 result와 같은 형식
- `runId` 형식 규칙: `process.startSimulator`와 동일

## 오류 코드

| Code                            | Meaning                                    |
| ------------------------------- | ------------------------------------------ |
| `INVALID_VERSION`               | 지원하지 않는 protocol version             |
| `INVALID_REQUEST`               | 필수 field 누락 등 경계 검증 실패          |
| `UNKNOWN_METHOD`                | 알 수 없는 method                          |
| `PROJECT_INVALID`               | project 구조 검증 실패                     |
| `PROJECT_EXISTS`                | project 생성 target이 비어 있지 않음       |
| `PROJECT_PRESET_INVALID`        | project preset 사용 불가                   |
| `SIMULATOR_EXECUTABLE_REQUIRED` | simulator executable 미설정 또는 접근 불가 |
| `PROCESS_START_FAILED`          | simulator 자식 process 시작 실패           |
| `RUN_ALREADY_TRACKED`           | 같은 run id process가 이미 추적 중         |
| `RUN_NOT_TRACKED`               | Bridge가 추적하지 않는 run id              |
