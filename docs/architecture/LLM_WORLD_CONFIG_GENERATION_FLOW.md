# LLM World Config Generation Flow

이 문서는 자연어 입력이 `WorldConfig` draft, validation, scenario reflection, RunQueue generation/export로 이어지는 흐름을 설명한다. 기술 식별자와 API path는 영어 원문을 유지한다.

schema validation을 통과한 뒤에는 scenario reflection validation을 수행한다. reflection이 실패하면 LLM에 scenario repair prompt를 다시 보내기 전에 deterministic scenario post-processing을 먼저 시도한다. Kickboard obstacle 삽입이나 pedestrian crossing behavior처럼 명확한 scenario 보강은 local rule과 schema 범위 안에서 처리한다.

검증된 generation result는 내부 변환 경로에서 EpisodeSetup + DeliveryBotSetup pair와 RunQueue로 변환된다. legacy UE5 handoff endpoint는 FastAPI/OpenAPI에서 제거되었다.

## FastAPI Generation Endpoint

`generate_world_config()` service 함수는 내부 orchestrator를 호출한다. 이전 `/api/v1/generation/world-config` endpoint는 public API에서 제거되었다.

현재 상태:

- 기본 provider는 `disabled`다.
- `disabled` provider는 `success=false`를 반환한다.
- OpenAI/Ollama provider client는 구현되어 있지만 automated tests와 harness에서는 실제 호출하지 않는다.
- `generatedPayload`를 사용하려면 validation이 필수다.
- UE 실행용 사용자 흐름은 `/api/v1/scenarios/generate`에서 RunQueue JSON으로 제공한다.

## Generation Orchestrator Status

내부 generation orchestrator는 service layer로 구현되어 있다.

- prompt package를 만든다.
- LLM client abstraction을 호출한다.
- 성공한 LLM content에서 JSON object를 추출한다.
- 추출한 payload를 `world_config` contract로 검증한다.
- validation error를 기준으로 repair attempt를 기록한다.
- `disabled` provider에서는 payload 없이 명확한 failed result를 반환한다.

automated tests와 harness check에서는 외부 LLM provider를 호출하지 않는다.

## LLM Client Abstraction

prompt package 생성 후에는 LLM client abstraction을 통해 provider별 client에 전달한다.

현재 상태:

- 기본 provider는 `disabled`다.
- `disabled` provider는 World Config payload를 생성하지 않는다.
- OpenAI/Ollama provider는 명시된 live smoke나 사용자가 허용한 실행에서만 실제 호출한다.
- provider output은 UE5 handoff 전에 validation layer를 반드시 통과해야 한다.

## API Shell Integration

현재 prompt package generation은 service 함수로만 노출한다.

```text
app.services.world_config_prompt_builder.build_world_config_prompt_package()
```

이 경로의 흐름:

Natural Language Prompt
-> prompt package service
-> prompt builder
-> deterministic RAG context
-> prompt package response

외부 LLM 실행과 generated World Config payload 반환은 service layer에서 처리한다. UE 실행용 RunQueue 흐름은 `/api/v1/scenarios/generate`를 사용한다.

## Current Implementation Scope

현재 구현 범위는 prompt package generation을 넘어 orchestrator, provider abstraction, validation, repair loop까지 포함한다. 다만 문서의 이 섹션은 초기 단계 기록도 포함하므로 현재 코드 기준 확인이 필요한 표현은 아래처럼 정리한다.

- `app/services/natural_language_normalizer.py`는 deterministic keyword rule로 natural-language prompt를 정규화한다.
- `app/services/world_config_rag_context_builder.py`는 `data/rag/policy_rag_chunks.jsonl`에서 policy context를 검색한다.
- `app/services/world_config_prompt_builder.py`는 system/user prompt package와 repair prompt package를 만든다.
- `app/services/world_config_generation_orchestrator.py`는 JSON extraction, validation, repair loop를 연결한다.
- sample JSON과 fixture 파일은 자동 생성하지 않는다.

repair prompt는 validation error에서 생성되며, 실제 LLM 호출은 provider 설정과 호출 허용 여부에 따라 수행된다.

## Deterministic Retrieval Before Embeddings

MVP generation flow는 `data/rag/policy_rag_chunks.jsonl`에 대한 deterministic retrieval을 사용한다.

retrieval input에는 다음 값이 포함될 수 있다.

- normalized prompt keywords
- mapped MVP situation
- target action
- policy parameter names
- 필요한 경우 source filters

검색된 chunk는 World Config drafting을 위한 policy context가 된다. embedding search, vector DB storage, source document RAG는 후속 단계로 둔다.

## 1. 전체 흐름

Natural Language Prompt
-> Prompt Normalization
-> Policy/RAG Context Retrieval
-> LLM World Config Draft Generation
-> JSON Extraction
-> Contract Validation
-> Repair Loop
-> Validated World Config
-> RunQueue Generation / Export

## 2. 단계별 설명

| 단계 | 입력 | 출력 | 실패 처리 |
| --- | --- | --- | --- |
| Natural Language Prompt | 사용자 시나리오 설명 | 원문 prompt | 입력이 너무 모호하면 추가 질문 |
| Prompt Normalization | 원문 prompt | 구조화된 의도와 제약 | 금지 요청은 거부 또는 범위 축소 |
| Policy/RAG Context Retrieval | 구조화된 의도 | 관련 policy card context | 관련 card가 없으면 기본 World Config 제약만 사용 |
| LLM World Config Draft Generation | prompt, schema 요약, RAG context | World Config draft | JSON이 아니면 JSON Extraction에서 실패 |
| JSON Extraction | LLM 출력 | JSON object 후보 | JSON object가 없으면 repair 요청 |
| Contract Validation | JSON object 후보 | validation layer 결과 | schema/Pydantic 오류를 수집 |
| Repair Loop | validation error | 수정된 JSON object 후보 | 최대 2회까지 repair |
| Validated World Config | 검증 통과 payload | UE 실행 계약 변환 가능 payload | 검증 전에는 UE 전달 금지 |
| RunQueue Generation / Export | Validated World Config | EpisodeSetup + DeliveryBotSetup pair와 RunQueue | 생성 실패 시 로그와 payload id 기록 |

## 3. Validation 원칙

- LLM 출력은 반드시 `world_config.schema.json`을 통과해야 한다.
- Pydantic `WorldConfig` 모델 검증을 통과해야 한다.
- validation 실패 시 에러 메시지를 LLM에 전달해 최대 2회 repair를 시도한다.
- 2회 실패 시 사용자의 입력을 명확히 하도록 요청한다.
- validation 통과 전에는 UE5에 전달하지 않는다.

## 4. RAG 사용 방식

- `policy_knowledge_cards.jsonl`은 LLM이 생성할 World Config의 안전 제약과 시나리오 구성 근거로 사용한다.
- RAG는 공식 인증 준수를 주장하기 위한 용도가 아니다.
- RAG는 프로젝트 내부 정책 기준과 시나리오 제약을 제공하는 용도다.

## 5. 출력 원칙

- LLM은 World Config JSON만 생성한다.
- 설명 문장을 JSON 안에 섞지 않는다.
- schema에 없는 필드는 생성하지 않는다.
- 불명확한 값은 assumptions에 기록하거나 기본값 정책으로 처리한다.

## Prompt Hardening Update

generation flow는 LLM 호출 전에 schema-derived World Config checklist를 추가한다. initial prompt와 repair prompt에는 required nested fields, allowed top-level fields, unit guidance, schema-extra field 제거 규칙이 포함된다.

validation error는 missing required fields, schema-extra fields, enum errors, type errors로 그룹화한다. repair prompt는 이 그룹을 직접 사용해 model이 schema를 바꾸지 않고 누락 field를 보강하고 extra key를 제거하도록 한다.

## Scenario Reflection Update

flow는 schema validation과 semantic scenario reflection validation을 분리한다. World Config가 schema validation을 통과해도 Kickboard obstacle, path blocking, pedestrian crossing 같은 사용자 조건을 표현하지 못하면 실패할 수 있다.

natural-language prompt는 먼저 scenario intent와 scenario requirements로 변환된다. 이 requirement는 prompt package에 추가되고, RAG retrieval query expansion에 사용되며, schema validation 이후 다시 확인된다.

## Output Contract Update

generation 전에 prompt builder는 World Config output contract를 추가한다. repair loop는 같은 contract를 반복해서 valid part를 유지하고, missing required nested field를 추가하고, extra field를 제거하고, scenario requirement를 만족하도록 요청한다.

## Scenario Repair Loop

schema validation은 통과했지만 scenario reflection이 실패하면 다음 repair attempt는 schema repair가 아니라 scenario repair prompt를 사용한다. scenario repair prompt는 누락 requirement를 schema path별로 나열하고, valid JSON은 유지하면서 빠진 scenario detail만 추가하도록 요청한다.
