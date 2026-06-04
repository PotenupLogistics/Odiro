# API Shell Guide

이 문서는 현재 FastAPI shell에서 노출되는 API와 provider 동작 범위를 정리한다. 사용자용 자연어 생성 API, UE handoff API, contract validation API를 구분해서 읽어야 한다.

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
POST /api/v1/generation/world-config/prompt-package
POST /api/v1/generation/world-config
POST /api/v1/contracts/validate/{contract_type}
POST /api/v1/ue5/world-config/handoff
POST /api/v1/scenarios/generate
```

## 3. 사용자용 scenario generation API

```text
POST /api/v1/scenarios/generate
```

이 endpoint는 새 사용자용 entrypoint다. 사용자는 자연어 `prompt`만 입력한다.

사용자가 `EpisodeSetup`, `DeliveryBotSetup`, `RunQueue` JSON을 직접 작성하는 구조가 아니다. JSON은 AI와 backend가 내부적으로 생성한 UE 실행 산출물이다.

응답은 UE 계약 그대로의 RunQueue JSON이다. 최상위 wrapper, diagnostics, raw LLM response, rawContent, API key 값은 포함하지 않는다.

핵심 검증 기준:

* 최상위 필드는 `schema`, `version`, `runs`만 사용한다.
* explicit `null`은 출력하지 않는다.
* `0`, `false`, `""`, `[]` 같은 의미 있는 값은 삭제하지 않는다.
* 좁은 보도 장애물 policy comparison queue는 동일 EpisodeSetup을 공유하고 DeliveryBotSetup만 policy별로 다르게 만든다.

## 4. WorldConfig generation API

```text
POST /api/v1/generation/world-config?provider=disabled
POST /api/v1/generation/world-config?provider=openai
POST /api/v1/generation/world-config?provider=ollama
```

`provider` 값:

* `disabled`: 실제 외부 LLM 호출 없이 provider disabled 결과를 반환한다.
* `openai`: `OPENAI_API_KEY`가 설정된 경우 OpenAI client를 사용한다.
* `ollama`: local Ollama server를 사용한다.
* `gemini`, `custom`: 테스트 주입 또는 후속 구현용 경로다.

자동 테스트와 harness check는 실제 OpenAI/Ollama 호출을 하지 않는다. live smoke는 명시적으로 허용된 경우에만 최소 횟수로 수행한다.

`generatedPayload`는 LLM 응답에서 JSON 추출과 `world_config` validation이 통과한 뒤에만 채워진다.

## 5. UE5 handoff API

```text
POST /api/v1/ue5/world-config/handoff
```

이 endpoint는 UE handoff와 내부 검증용 경로다. validated WorldConfig를 metadata, validation summary, scenario reflection, post-processing diagnostics, warning과 함께 감싼다.

지원하는 `responseFormat`:

* `episode_spec`: legacy EpisodeSpec 응답
* `setup_pair`: 최신 EpisodeSetup + DeliveryBotSetup pair 응답
* `both`: legacy와 setup pair를 함께 확인하는 debugging 응답
* `world_config`: AI 내부 WorldConfig inspection 응답

`/api/v1/ue5/world-config/handoff`의 `responseFormat=run_queue`는 현재 public handoff 옵션이 아니다. RunQueue가 필요한 사용자 흐름은 `/api/v1/scenarios/generate`를 사용한다.

`includeDiagnostics=true`일 때 diagnostics에는 generationTrace, setupPairTrace 같은 요약 근거를 담을 수 있다. trace에는 rawContent, full raw response, API key, 인증값을 저장하지 않는다.

## 6. Prompt package API

```text
POST /api/v1/generation/world-config/prompt-package
```

이 endpoint는 prompt builder 결과를 확인하기 위한 경로다. deterministic RAG context, schema-derived checklist, scenario requirements, repair guidance에 필요한 prompt package를 반환한다.

prompt package endpoint 자체는 LLM을 호출하지 않고 WorldConfig payload도 생성하지 않는다.

## 7. Contract validation API

```text
POST /api/v1/contracts/validate/{contract_type}
```

제출된 JSON payload가 특정 contract type에 맞는지 검증한다. 이 endpoint는 payload를 생성하지 않고, sample/fixture JSON 파일도 만들지 않는다.

## 8. Environment sampling

`generationRequest.constraints.environmentSampling`을 사용하면 seed, scenarioType, fixedParameters 기반 numeric constraints를 prompt와 deterministic post-processing에 연결한다.

sampling 결과는 numeric summary로 diagnostics에 기록한다. `low`, `middle`, `high` 같은 자연어 값을 UE JSON 값으로 직접 쓰지 않는다.

## 9. 구현하지 않는 것

현재 API shell은 다음을 자동으로 수행하지 않는다.

* UE C++ / Blueprint 코드 생성
* vector DB 또는 embedding index 생성
* repo sample JSON / fixture JSON 생성
* raw OpenAI/Ollama response 전문 저장
* API key 또는 인증값 저장
