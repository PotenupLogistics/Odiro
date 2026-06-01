# UE5 Endpoint Usage for UE Team

## 1. 서버 실행

```powershell
uv run uvicorn app.main:app --reload
```

## 2. Health check

* `GET /health`

## 3. EpisodeSpec handoff 요청

EpisodeSpec JSON 계약의 기준 문서는 `docs/UE_EPISODE_SPEC_JSON_GUIDE.md`이다.

Endpoint:

* `POST /api/v1/ue5/world-config/handoff?provider=openai&responseFormat=episode_spec`

`responseFormat` 기본값은 `episode_spec`이다. UE 테스트에서는 `responseFormat=episode_spec`을 권장한다.
디버깅에서는 `responseFormat=both`를 사용하면 `worldConfig`와 `episodeSpec`을 함께 볼 수 있다.
`responseFormat=world_config`는 AI 내부 구조 확인용이며 이 경우 `episodeSpec`은 `null`일 수 있다.

Environment sampler 연동:

* `generationRequest.constraints.environmentSampling.enabled=true`
* `seed`와 `scenarioType`으로 deterministic numeric parameters 생성
* numeric constraints는 자연어보다 우선
* diagnostics에는 sampled numeric summary만 포함

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
uv run python scripts/export_ue5_handoff_payload.py --prompt "..." --provider openai --format episode_spec
```

* `--out`을 명시한 경우에만 파일 저장
* `--out` 없이 실행하면 콘솔 출력만 수행
