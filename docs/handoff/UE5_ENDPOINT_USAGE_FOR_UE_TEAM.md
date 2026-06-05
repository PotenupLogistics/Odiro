# UE5 API Usage for UE Team

## 1. 서버 실행

```powershell
uv run uvicorn app.main:app --reload
```

## 2. Health check

* `GET /health`

## 3. 권장 API: scenario generation

Endpoint:

* `POST /api/v1/scenarios/generate`

이 endpoint는 사용자의 자연어 `prompt`를 필수로 받고, 선택적으로 `episode_count`를 허용한다. 사용자가 EpisodeSetup / DeliveryBotSetup / RunQueue JSON을 직접 작성하는 구조가 아니다. AI와 backend가 내부적으로 EpisodeSetup + DeliveryBotSetup pair와 RunQueue JSON을 생성한다.

성공 응답은 wrapper 없는 RunQueue JSON이며 최상위 필드는 `schema`, `version`, `runs`만 포함한다. EpisodeSetup / DeliveryBotSetup / RunQueue export는 null-free 정책을 따른다.

## 3.1 Removed legacy EpisodeSpec handoff endpoint

Endpoint:

* `POST /api/v1/ue5/world-config/handoff?provider=openai&responseFormat=episode_spec`

이 legacy endpoint는 현재 FastAPI route와 OpenAPI에서 제거되었다. 이 URL은 현재 UE 연동용 정상 API가 아니다.

EpisodeSpec JSON 계약의 기준 문서는 `docs/archive/previous_episode_spec/UE_EPISODE_SPEC_JSON_GUIDE.md`이다. 이전 `responseFormat=episode_spec`, `responseFormat=both`, `responseFormat=world_config` 설명은 archive/tooling 참고용이다.

## 3.1 최신 EpisodeSetup + DeliveryBotSetup pair 요청

최신 UE 계약 기준 문서는 `docs/ue_contracts/` 아래 문서다.

Removed endpoint:

* `POST /api/v1/ue5/world-config/handoff?provider=openai&responseFormat=setup_pair`

이 legacy endpoint 역시 현재 route/OpenAPI에서 제거되었다. 최신 setup pair 생성은 `/api/v1/scenarios/generate`와 RunQueue export 경로를 기준으로 한다.

UE가 읽어야 하는 setup pair 필드:

* `response.episodeSetup`
* `response.deliveryBotSetup`
* `response.episodeSetupValidation`
* `response.deliveryBotSetupValidation`
* `response.diagnostics.setupPairTrace`

`responseFormat=both`는 기존 worldConfig + EpisodeSpec 디버그 응답 의미를 유지하며 setup pair까지 포함하지 않는다.

Environment sampler 연동:

* `generationRequest.constraints.environmentSampling.enabled=true`
* `seed`와 `scenarioType`으로 deterministic numeric parameters 생성
* numeric constraints는 자연어보다 우선
* diagnostics에는 sampled numeric summary만 포함

Legacy 요청 body 구조:

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
uv run python scripts/export_ue5_handoff_payload.py --prompt "..." --provider openai --format setup_pair
```

* `--out`을 명시한 경우에만 파일 저장
* `--out` 없이 실행하면 콘솔 출력만 수행
