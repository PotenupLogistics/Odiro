# API Shell Guide

이 문서는 현재 FastAPI에서 외부에 노출되는 API와 내부 service/helper로만 유지하는 검증 기능을 구분한다.

## 1. 실행

```bash
uv run uvicorn app.main:app --reload
```

Swagger UI:

```text
http://127.0.0.1:8000/docs
```

OpenAPI JSON:

```text
http://127.0.0.1:8000/openapi.json
```

## 2. 현재 endpoint

```text
GET /health
POST /api/v1/scenarios/generate
POST /api/v1/scenarios/generate-drive
POST /api/v1/scenarios/generate-artifacts
```

사용자용 기본 `/api/v1` endpoint는 `POST /api/v1/scenarios/generate`다. Google Drive 동기화 기반 UE 전달이 필요하면 `POST /api/v1/scenarios/generate-drive`를 사용한다. `POST /api/v1/scenarios/generate-artifacts`는 같은 request model을 쓰는 debug/test artifact zip 다운로드 endpoint다.

## 3. 사용자용 scenario generation API

```text
POST /api/v1/scenarios/generate
```

이 endpoint는 새 사용자용 entrypoint다. 사용자는 자연어 `prompt`를 필수로 입력하고, 선택적으로 `episode_count`로 생성할 episode/run 개수를 지정할 수 있다.

사용자가 `EpisodeSetup`, `DeliveryBotSetup`, `RunQueue` JSON을 직접 작성하는 구조가 아니다. JSON은 AI와 backend가 내부적으로 생성한 UE 실행 산출물이다.

응답은 UE 계약 그대로의 RunQueue JSON이다. 최상위 wrapper, diagnostics, raw LLM response, rawContent, API key 값은 포함하지 않는다.
`episode_count`가 없으면 `SCENARIO_EPISODE_DEFAULT_COUNT` 환경 변수 값을 사용한다.

핵심 검증 기준:

* request body에서 `prompt`는 required이고, `episode_count`는 optional이다.
* `episode_count`가 제공되면 1 이상 `SCENARIO_EPISODE_MAX_COUNT` 이하의 strict integer만 허용한다.
* 최상위 필드는 `schema`, `version`, `runs`만 사용한다.
* explicit `null`은 출력하지 않는다.
* `0`, `false`, `""`, `[]` 같은 의미 있는 값은 삭제하지 않는다.
* 좁은 보도 장애물 policy comparison queue는 동일 EpisodeSetup을 공유하고 DeliveryBotSetup만 policy별로 다르게 만든다.

## 4. Google Drive artifact upload API

```text
POST /api/v1/scenarios/generate-drive
```

이 endpoint는 `/api/v1/scenarios/generate`와 같은 `ScenarioGenerateRequest` body를 받는다. backend는 기존 scenario generation flow를 재사용해 `EpisodeRunQueue`, `EpisodeSetup`, `DeliveryBotSetup` JSON artifact를 만들고, `.env`에 설정된 Google Drive 폴더로 각 JSON 파일을 `application/json`으로 업로드한다.

응답은 artifact 본문이나 zip 파일이 아니라 Drive metadata JSON이다.

```json
{
  "status": "success",
  "schema": "scenario_drive_artifact_response",
  "version": 1,
  "drive_folder_id": "configured-folder-id",
  "run_queue_file": "EpisodeRunQueue_example.json",
  "files": [
    {
      "kind": "episode_run_queue",
      "filename": "EpisodeRunQueue_example.json",
      "drive_file_id": "drive-file-id",
      "drive_url": "https://drive.google.com/file/d/drive-file-id/view"
    }
  ]
}
```

Google Drive 방식에서는 AI 서버가 지정 폴더에 JSON artifact를 업로드하고, 언리얼은 Google Drive 동기화 폴더에서 `run_queue_file`을 읽는다. Drive 동기화 지연이 있을 수 있으므로, 언리얼 쪽에서는 해당 파일이 생겼는지 확인한 뒤 실행하는 것이 안전하다.

Drive 폴더와 credentials/token 경로는 요청 body로 받지 않는다. 서버는 `GOOGLE_DRIVE_UPLOAD_ENABLED`, `GOOGLE_DRIVE_FOLDER_ID`, `GOOGLE_DRIVE_AUTH_MODE` 설정을 읽는다.

`GOOGLE_DRIVE_AUTH_MODE=service_account`는 Shared Drive 업로드에 권장한다. 이 mode는 `GOOGLE_DRIVE_SERVICE_ACCOUNT_FILE`을 사용하며 Shared Drive 대응을 위해 Drive API upload에 `supportsAllDrives=True`를 전달한다.

`GOOGLE_DRIVE_AUTH_MODE=oauth`는 사용자의 My Drive 공유 폴더 업로드에 사용한다. service account는 My Drive에서 `Service Accounts do not have storage quota`로 실패할 수 있다. OAuth mode는 `GOOGLE_DRIVE_OAUTH_CLIENT_FILE`에서 client 설정을 읽고, `GOOGLE_DRIVE_OAUTH_TOKEN_FILE`에 사용자 token을 저장/재사용한다.

`GOOGLE_DRIVE_BACKUP_BEFORE_UPLOAD=true`이면 새 artifact 업로드 전에 대상 폴더의 기존 직계 child 항목을 백업 폴더로 이동한다. `GOOGLE_DRIVE_BACKUP_FOLDER_ID`가 있으면 해당 ID를 사용하고, 없으면 대상 폴더 안에서 `GOOGLE_DRIVE_BACKUP_FOLDER_NAME` 값(기본값 `백업`)과 일치하는 폴더를 찾는다. 백업 폴더 자체와 백업 폴더 내부 기존 파일은 이동하지 않는다. 백업 폴더가 없거나 이름이 중복되거나 기존 항목 이동에 실패하면 새 artifact 업로드를 시작하지 않고 500 오류를 반환한다.

`secrets/oauth_client.json`, `secrets/google_drive_token.json`, `secrets/credentials.json`은 Git에 올리면 안 된다. OAuth client secret, OAuth token, service account private key, credentials 내용은 로그에 남기면 안 된다.

## 5. Debug/test artifact download API

```text
POST /api/v1/scenarios/generate-artifacts
```

이 endpoint는 `/api/v1/scenarios/generate`와 같은 request body를 받는다. 응답은 JSON body가 아니라 `Content-Type: application/zip`인 `scenario_artifacts.zip` 파일이다. Google Drive 전달 방식이 필요한 UE 흐름에서는 `/api/v1/scenarios/generate-drive`를 사용하고, 이 zip endpoint는 debug/test 또는 legacy 확인용으로 유지한다.

zip 내부에는 다음 파일을 포함한다.

* `response.json`: `/api/v1/scenarios/generate`가 반환하는 wrapper 없는 RunQueue JSON과 같은 구조
* `EpisodeRunQueue_*.json`
* `EpisodeSetup_*.json`
* `DeliveryBotSetup_*.json`

이 endpoint는 UE 통신/수동 확인을 위한 debug/test 경로다. 기존 `/api/v1/scenarios/generate` 응답 schema는 변경하지 않는다.

## 6. WorldConfig generation service

WorldConfig generation은 더 이상 `/api/v1/generation/world-config` HTTP endpoint로 노출하지 않는다. 내부 구현은 `app.services.world_config_generation_orchestrator.generate_world_config()` service 함수로 유지한다.

provider 값:

* `disabled`: 실제 외부 LLM 호출 없이 provider disabled 결과를 반환한다.
* `openai`: `OPENAI_API_KEY`가 설정된 경우 OpenAI client를 사용한다.
* `ollama`: local Ollama server를 사용한다.
* `gemini`, `custom`: 테스트 주입 또는 후속 구현용 경로다.

자동 테스트와 harness check는 service/function 단위에서 실제 OpenAI/Ollama 호출 없이 검증한다. live smoke는 명시적으로 허용된 경우에만 최소 횟수로 수행한다.

`generatedPayload`는 LLM 응답에서 JSON 추출과 `world_config` validation이 통과한 뒤에만 채워진다.

## 7. Removed legacy UE5 handoff API

```text
POST /api/v1/ue5/world-config/handoff
```

이 legacy endpoint는 현재 FastAPI route와 OpenAPI에서 제거되었다. 해당 URL 요청은 정상 API로 처리되지 않고 route not found로 남아야 한다.

RunQueue가 필요한 사용자/UE 흐름은 `/api/v1/scenarios/generate`를 사용한다. 이전 `responseFormat=episode_spec`, `responseFormat=setup_pair`, `responseFormat=both`, `responseFormat=world_config` 설명은 archive 문서와 CLI tooling 참고용이다.

## 8. Prompt package service

Prompt package 생성은 더 이상 HTTP endpoint로 노출하지 않는다. `app.services.world_config_prompt_builder.build_world_config_prompt_package()` 함수가 deterministic RAG context, schema-derived checklist, scenario requirements, repair guidance에 필요한 prompt package를 반환한다.

prompt package builder 자체는 LLM을 호출하지 않고 WorldConfig payload도 생성하지 않는다.

## 9. Contract validation service / CLI

```text
uv run python scripts/validate_contract.py --type world_config --file path/to/world.json
```

Contract validation은 더 이상 HTTP endpoint로 노출하지 않는다. 제출된 JSON payload가 특정 contract type에 맞는지는 `app.services.json_contract_validator.validate_payload()` 또는 `scripts/validate_contract.py` CLI로 검증한다. 이 경로는 payload를 생성하지 않고, sample/fixture JSON 파일도 만들지 않는다.

## 10. Environment sampling

`generationRequest.constraints.environmentSampling`을 사용하면 seed, scenarioType, fixedParameters 기반 numeric constraints를 prompt와 deterministic post-processing에 연결한다.

sampling 결과는 numeric summary로 diagnostics에 기록한다. `low`, `middle`, `high` 같은 자연어 값을 UE JSON 값으로 직접 쓰지 않는다.

## 11. 구현하지 않는 것

현재 public API와 service/helper 경로는 다음을 자동으로 수행하지 않는다.

* UE C++ / Blueprint 코드 생성
* vector DB 또는 embedding index 생성
* repo sample JSON / fixture JSON 생성
* raw OpenAI/Ollama response 전문 저장
* API key 또는 인증값 저장
