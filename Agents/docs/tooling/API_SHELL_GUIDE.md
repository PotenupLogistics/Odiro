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
POST /api/v1/analysis/run
POST /api/v2/scenarios/generate
POST /api/v2/analysis/run
```

`POST /api/v1/scenarios/generate`는 현재 `410 RUN_QUEUE_REMOVED` 안내만 반환한다. User project scenario 생성은 `POST /api/v2/scenarios/generate`를 사용한다. User project run 분석은 `POST /api/v2/analysis/run`을 사용한다.

## 3. Legacy scenario generation API

```text
POST /api/v1/scenarios/generate
```

이 endpoint는 제거 안내만 반환한다.

```json
{
  "code": "RUN_QUEUE_REMOVED"
}
```

이전 RunQueue 응답은 legacy tooling 기록으로만 남긴다.

## 3.1 User project scenario generation API

```text
POST /api/v2/scenarios/generate
```

이 endpoint는 자연어 `prompt`를 받고 `<UserProject>/scenario.json`에 저장 가능한 `scenario` JSON을 반환한다. 실행 개수, seed, scenario sample, RunQueue 생성은 담당하지 않는다.

## 4. WorldConfig generation service

WorldConfig generation은 더 이상 `/api/v1/generation/world-config` HTTP endpoint로 노출하지 않는다. 내부 구현은 `app.services.world_config_generation_orchestrator.generate_world_config()` service 함수로 유지한다.

provider 값:

* `disabled`: 실제 외부 LLM 호출 없이 provider disabled 결과를 반환한다.
* `openai`: `OPENAI_API_KEY`가 설정된 경우 OpenAI client를 사용한다.
* `ollama`: local Ollama server를 사용한다.
* `gemini`, `custom`: 테스트 주입 또는 후속 구현용 경로다.

자동 테스트와 harness check는 service/function 단위에서 실제 OpenAI/Ollama 호출 없이 검증한다. live smoke는 명시적으로 허용된 경우에만 최소 횟수로 수행한다.

`generatedPayload`는 LLM 응답에서 JSON 추출과 `world_config` validation이 통과한 뒤에만 채워진다.

## 5. Removed legacy UE5 handoff API

```text
POST /api/v1/ue5/world-config/handoff
```

이 legacy endpoint는 현재 FastAPI route와 OpenAPI에서 제거되었다. 해당 URL 요청은 정상 API로 처리되지 않고 route not found로 남아야 한다.

RunQueue 생성은 사용자용 API가 아니다. 이전 `responseFormat=episode_spec`, `responseFormat=setup_pair`, `responseFormat=both`, `responseFormat=world_config` 설명은 archive 문서와 CLI tooling 참고용이다.

## 7. Prompt package service

Prompt package 생성은 더 이상 HTTP endpoint로 노출하지 않는다. `app.services.world_config_prompt_builder.build_world_config_prompt_package()` 함수가 deterministic RAG context, schema-derived checklist, scenario requirements, repair guidance에 필요한 prompt package를 반환한다.

prompt package builder 자체는 LLM을 호출하지 않고 WorldConfig payload도 생성하지 않는다.

## 8. Contract validation service / CLI

```text
uv run python scripts/validate_contract.py --type world_config --file path/to/world.json
```

Contract validation은 더 이상 HTTP endpoint로 노출하지 않는다. 제출된 JSON payload가 특정 contract type에 맞는지는 `app.services.json_contract_validator.validate_payload()` 또는 `scripts/validate_contract.py` CLI로 검증한다. 이 경로는 payload를 생성하지 않고, sample/fixture JSON 파일도 만들지 않는다.

## 9. Environment sampling

`generationRequest.constraints.environmentSampling`을 사용하면 seed, scenarioType, fixedParameters 기반 numeric constraints를 prompt와 deterministic post-processing에 연결한다.

sampling 결과는 numeric summary로 diagnostics에 기록한다. `low`, `middle`, `high` 같은 자연어 값을 UE JSON 값으로 직접 쓰지 않는다.

## 10. 구현하지 않는 것

현재 public API와 service/helper 경로는 다음을 자동으로 수행하지 않는다.

* UE C++ / Blueprint 코드 생성
* vector DB 또는 embedding index 생성
* repo sample JSON / fixture JSON 생성
* raw OpenAI/Ollama response 전문 저장
* API key 또는 인증값 저장
