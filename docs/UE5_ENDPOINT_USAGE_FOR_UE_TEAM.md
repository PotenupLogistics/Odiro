# UE5 Endpoint Usage for UE Team

## 1. 서버 실행

```powershell
uv run uvicorn app.main:app --reload
```

## 2. Health check

* `GET /health`

## 3. EpisodeSpec handoff 요청

Endpoint:

* `POST /api/v1/ue5/world-config/handoff?provider=ollama&responseFormat=episode_spec`

요청 body 구조:

* `schemaVersion`
* `requestId`
* `handoffTarget`
* `includeDiagnostics`
* `generationRequest`
* `generationRequest.schemaVersion`
* `generationRequest.requestId`
* `generationRequest.generationType`
* `generationRequest.targetContractType`
* `generationRequest.prompt`
* `generationRequest.policyId`
* `generationRequest.maxRepairAttempts`
* `generationRequest.constraints`

주의: 이 문서는 요청 구조를 설명한다. sample JSON 파일은 생성하지 않는다.

## 4. 응답에서 UE가 읽어야 하는 부분

* `response.success`
* `response.episodeSpec`
* `response.episodeValidation`
* `response.episodeScenarioReflection`

## 5. UE가 실행하지 말아야 하는 응답

* `success=false`
* `episodeSpec=null`
* `episodeValidation.valid=false`
* `episodeScenarioReflection.passed=false`

## 6. Export CLI 사용법

```powershell
uv run python scripts/export_ue5_handoff_payload.py --prompt "..." --provider ollama --format episode_spec
```

* `--out`을 명시한 경우에만 파일 저장
* `--out` 없이 실행하면 콘솔 출력만 수행

