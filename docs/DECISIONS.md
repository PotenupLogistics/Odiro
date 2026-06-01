# Policy Card Generation Decisions

## UE5 Handoff Contract

- Only World Config payloads that pass schema validation, contract validation, and scenario reflection are eligible for UE5 handoff.
- UE5 handoff responses provide `worldConfig` together with metadata and diagnostics.
- UE5 should execute `worldConfig`; `diagnostics` and `postProcessing` are for debugging.
- Failed handoff responses do not include `worldConfig`.
- Sample JSON and fixture files are still not created.

## Scenario Post-Processing Before Fallback

- Schema-valid payloads that fail scenario reflection are first passed through deterministic scenario post-processing.
- Post-processing only fills missing elements directly implied by extracted scenario intent.
- If post-processing still fails, the scenario repair prompt flow is used.
- OpenAI fallback is considered only after local post-processing and repair fail.

## Ollama Manual Smoke Decisions

- Manual live Ollama verification uses `scripts/run_ollama_world_config_smoke.py`.
- Dry-run is the safe preflight path because it builds the prompt package without calling Ollama.
- Automated tests and harness checks must not call a real Ollama server.
- The smoke runner writes no report unless `--report` is explicitly provided.
- No sample JSON, fixture, vector DB, or embedding index is created by the smoke runner.
- Attempt-level diagnostics are recorded to analyze Ollama validation failures.
- Full raw output storage is disabled by default and requires `--include-raw-attempts`.
- OpenAI provider implementation is added after Ollama validation failure diagnostics.
- Prompt improvements, schema changes, or fallback decisions should be based on detailed diagnostics.
- Provider timeout is classified separately from `validation_failed`.
- Timeout tuning uses warm-up, increased timeout, reduced repair attempts, smaller context topK, and compact prompt before considering fallback.

## Ollama Provider Decisions

- Real provider implementation starts with Ollama.
- Ollama is the cost-saving first provider.
- Tests and harness checks do not call a real Ollama server.
- OpenAI provider is the first provider; Ollama remains fallback.
- Ollama output must pass JSON extraction and validation before becoming `generatedPayload`.

## LLM Provider Configuration Decisions

- OpenAI and Ollama are target providers.
- The provider chain is `openai, ollama`, with Ollama retained as fallback for cost control and resilience.
- OpenAI API key is read from `.env`.
- A real `.env` file is not committed.
- OpenAI fallback is allowed only under explicit limited conditions.
- Actual provider call implementation is deferred to a later step.

## Generation Endpoint Decisions

- A natural-language World Config generation endpoint is added.
- The default provider is `disabled`, and no external API call is made.
- Disabled provider responses are explicit failed results.
- Real LLM provider integration is deferred to a later step.
- `generatedPayload` is a UE5 handoff candidate only after validation passes.
- Sample JSON and fixture files are not created in this stage.

## World Config Generation Orchestrator Decisions

- The generation orchestrator is implemented before attaching a real LLM provider.
- The `disabled` provider returns an explicit failed result and no generated payload.
- LLM response content must pass JSON extraction and validation before it can become `generatedPayload`.
- The repair loop is driven by validation errors.
- The FastAPI generation endpoint is deferred to a later step.

## LLM Client Abstraction Decisions

- A provider abstraction is created before attaching any real LLM provider.
- The default provider for this stage is `disabled`.
- External API calls are deferred to a later explicit provider implementation step.
- API keys and secrets are not hardcoded in source code.
- LLM output must pass the validation layer before it can become a UE5 handoff candidate.

## API Shell Decisions

- Natural-language input can be accepted through the FastAPI shell.
- The current API returns only a World Config prompt package and does not call an external LLM.
- Actual World Config JSON generation is deferred to a later LLM client stage.
- The contract validation endpoint is a shell for validating JSON payloads sent by UE5 or developers.
- Sample JSON and fixture files are not created in this stage.

## World Config Prompt Builder Decisions

- Natural-language input will eventually arrive through an API or UI, but the current stage implements only internal prompt builder services.
- External LLM API calls are deferred until prompt packages and validation behavior are stable.
- World Config generation prompt packages use deterministic policy RAG retrieval context.
- Sample JSON files are not created in this stage.
- FastAPI endpoints are not created in this stage.

## RAG Retrieval Decisions

- Deterministic retrieval is implemented before embedding or vector DB work.
- The current RAG retrieval target is the confirmed policy-card chunk set in `data/rag/policy_rag_chunks.jsonl`.
- Source document RAG remains a separate later track and does not use raw PDF or processed Markdown chunks in this stage.
- The natural-language World Config generator will use the retrieval layer to receive relevant policy chunks as prompt context.
- Retrieval is keyword, category, action, policy-parameter, and source based until embedding behavior is explicitly introduced in a later step.

- policy knowledge card는 manual confirmation에서 confirmed 처리된 후보만 생성 대상으로 삼는다.
- pending_manual_confirmation 후보와 rejected 후보는 policy knowledge card로 변환하지 않는다.
- confirmed 후보가 0개이면 policy_knowledge_cards.jsonl을 생성하지 않는다.
- policy knowledge card는 프로젝트 내부 정책 기준이며 공식 인증 준수를 의미하지 않는다.
- source registry에 없는 sourceId를 가진 confirmed 후보는 card 생성 실패로 처리한다.

# DECISIONS

## Source Management

- 한국 법·인증·운행 기준 원본 문서는 `data/sources/raw/korea`에 보관한다.
- 원본 PDF는 raw에 보관한다.
- 원본 PDF를 직접 RAG에 넣지 않는다.
- 먼저 source registry로 출처를 관리한다.
- processed Markdown은 사람이 검토 가능한 중간 산출물이다.
- RAG에는 processed Markdown 전체가 아니라 검토된 policy knowledge card만 넣는다.
- 이후 검토된 내용만 policy knowledge card로 변환한다.
- 추출 실패 문서는 임의 보완하지 않고 `needs_manual_review`로 남긴다.
- processed Markdown이 partial인 상태에서는 policy card를 생성하지 않는다.
- 원본 PDF와 processed Markdown을 수동 대조한 뒤 reviewed 처리한다.
- reviewed 처리된 source만 policy knowledge card 생성 대상으로 삼는다.
- 정책 카드 생성 전에는 review checklist를 거친다.
- processed Markdown에서 자동 추출한 정책 후보는 policy card가 아니다.
- 자동 후보는 원본 PDF와 수동 대조 후에만 confirmed 처리할 수 있다.
- confirmed 후보만 policy knowledge card 생성 대상으로 사용한다.
- policy card 생성 전까지 RAG에는 반영하지 않는다.
- 한국 법·인증 문서와 별도로 연구/방법론 문서는 RSR prefix로 관리한다.
- RSR 문서는 정책 직접 근거가 아니라 시나리오 설계, 실험 조합 축소, LLM 자동화 구조 근거로 사용한다.
- RSR 문서도 reviewed 처리 전에는 policy card로 변환하지 않는다.
- HTML source는 원문 전체를 저장하지 않고 URL registry로 관리한다.
- RSR-001은 local PDF로 processed 처리한다.
- RSR-002~RSR-006은 URL-only source로 관리하며, 원문을 확보하기 전에는 processed Markdown을 만들지 않는다.
- RSR 문서도 reviewed 전에는 knowledge card로 변환하지 않는다.
- 연구 문서는 정책 직접 근거뿐 아니라 시나리오 설계, 평가 지표, LLM 자동화 구조 근거로 사용한다.
- 자동 추출된 201개 후보는 바로 policy card로 만들지 않는다.
- MVP 정책과 직접 연결되는 후보를 high priority로 먼저 수동 검토한다.
- high priority라도 원본 PDF 대조 전에는 confirmed가 아니다.
- confirmed 후보만 다음 단계에서 policy knowledge card로 변환한다.
- High priority 후보도 원본 PDF 대조 전에는 confirmed가 아니다.
- High priority review queue는 policy card 생성 전 검토 작업공간이다.
- confirmed/rejected 판단은 자동화하지 않고 사람이 수행한다.
- confirmed 후보만 다음 단계에서 policy knowledge card로 변환한다.
- high priority 후보를 policy card로 만들기 전 manual confirmation 단계를 둔다.
- confirmed/rejected 판단은 자동화하지 않고 사람이 원본 PDF와 대조해 수행한다.
- confirmed 후보만 다음 단계에서 policy knowledge card 생성 대상으로 사용한다.
- pending 상태가 남아 있으면 policy card 생성 단계로 넘어가지 않는다.
- confirmed/rejected 수동 입력은 CLI로 지원한다.
- CLI는 자동 판단을 하지 않는다.
- --yes가 없으면 파일을 수정하지 않는다.
- confirmed 후보가 생겨도 policy card 생성은 별도 단계에서 수행한다.
- page hint는 원본 PDF 수동 검토를 돕는 보조 정보이다.
- page hint는 confirmed/rejected 판단을 대체하지 않는다.
- page hint 생성 후에도 사람이 원본 PDF를 직접 확인해야 한다.
- confirmed 후보만 다음 단계에서 policy knowledge card 생성 대상으로 사용한다.
- page hint 이후에도 자동 confirmed/rejected 판단은 하지 않는다.
- manual review pack은 사람이 원본 PDF 대조를 쉽게 하기 위한 보조 문서이다.
- policy card 생성은 confirmed 후보가 생긴 이후에만 수행한다.
- 샘플 JSON fixture는 아직 생성하지 않는다.
# Current Policy Card Decisions

- confirmed 후보만 policy knowledge card로 변환한다.
- pending/rejected 후보는 RAG에 넣지 않는다.
- policy card는 공식 인증 준수가 아니라 프로젝트 내부 정책 기준이다.
- policy card 생성 후에도 sample JSON과 API는 별도 단계에서 진행한다.
- Policy card 생성 이후 바로 JSON Schema로 가지 않고, 먼저 card-to-policy mapping 문서를 만든다.
- Policy Config 파라미터는 policy card 근거와 연결된 항목부터 정의한다.
- UE5 Decision Request 필드는 policy card와 MVP 액션에 필요한 필드 중심으로 정리한다.
- JSON Schema, sample fixture, API는 다음 단계에서 진행한다.
- policy card 9개를 기반으로 JSON 계약을 설계한다.
- 단위는 cm, kmh, sec, degree를 사용한다.
- sample JSON fixture는 schema 검증 이후 별도 단계에서 작성한다.
- FastAPI API는 schema와 모델 검증 이후 별도 단계에서 작성한다.
- schema/model 생성 후 바로 API로 가지 않고 contract validation layer를 먼저 만든다.
- sample JSON은 아직 생성하지 않는다.
- 테스트는 in-memory dict 또는 임시 파일만 사용한다.
- UE5가 제공하는 JSON은 validate_contract.py 또는 validation service로 먼저 검증한다.
- validate_contract.py CLI는 자연어 입력용이 아니라 JSON 계약 검증용이다.
- 자연어 입력은 후속 API 또는 UI에서 받는다.
- MVP 자연어 생성 대상은 World Config JSON으로 제한한다.
- LLM이 생성한 JSON은 validation layer를 통과한 뒤에만 UE5에 전달한다.
- API 구현은 자연어 입력 설계 문서가 정리된 뒤 별도 단계에서 진행한다.
- RAG MVP는 원본 PDF가 아니라 confirmed policy card 기반으로 구성한다.
- 1 policy card = 1 RAG chunk 전략을 사용한다.
- 원본 문서 RAG와 정책 카드 RAG는 분리한다.
- embedding/vector DB 생성은 chunk 검증 이후 별도 단계에서 진행한다.

## Decision: Harden World Config Prompt Before Provider Fallback

- Ollama JSON extraction failures and validation failures are diagnosed separately.
- If JSON extraction succeeds but `world_config` validation fails, prompt/schema summary hardening is attempted before OpenAI fallback.
- Prompt hardening uses the existing JSON Schema and does not modify schema files.
- The repair prompt must explicitly list missing required fields and schema-extra fields.
- No sample JSON or fixture files are created during prompt hardening.

## Decision: Separate Schema Validation And Scenario Reflection

- Schema validation and semantic scenario reflection validation are separate checks.
- A schema-valid `world_config` is not considered successful if required natural-language scenario conditions are missing.
- Korean keyword expansion runs before RAG retrieval.
- Scenario requirements are injected into World Config prompt packages.
- OpenAI fallback is not the first response to missing scenario details; intent extraction, retrieval, and prompt guidance are improved first.

## Decision: Strengthen Output Contract Before Provider Fallback

- When schema validation fails, output contract prompt hardening is attempted before OpenAI fallback.
- Scenario requirements are linked to schema paths in the prompt.
- JSON Schema is not modified for prompt hardening.
- Smoke reports record output contract and scenario requirement diagnostics.

## Decision: Add Scenario Repair Before Provider Fallback

- Schema validation and scenario reflection validation remain separate stages.
- If schema validation passes but scenario reflection fails, a scenario repair loop runs before considering provider fallback.
- Scenario repair preserves schema-valid JSON and repairs only missing semantic requirements.
- Final success requires both schema validation and scenario reflection validation.
# UE5 EpisodeSpec adapter decisions

* AI 내부 계약은 `WorldConfig`로 유지한다.
* UE 실행 계약은 `EpisodeSpec`으로 변환한다.
* UE5 담당자 문서 기준으로 EpisodeSpec adapter를 둔다.
* Kickboard는 현재 catalog에 없으므로 임시 prop mapping을 사용하고 warning을 남긴다.
* UE 쪽에는 `obstacle.kickboard` prop 추가 여부 확인이 필요하다.
* JSON Schema, sample JSON, fixture, UE C++/Blueprint 코드는 이 단계에서 만들지 않는다.

# UE5 EpisodeSpec handoff smoke decisions

* UE 실행용 handoff는 `EpisodeSpec` format을 우선 지원한다.
* `WorldConfig`는 AI 내부 검증/추론용 계약으로 유지한다.
* `responseFormat=episode_spec`은 UE 연동 테스트 기본값으로 사용한다.
* `responseFormat=both`는 디버깅용으로 사용한다.
* Kickboard는 임시 prop mapping을 사용하며 UE 쪽 catalog 추가를 요청한다.

# UE5 EpisodeSpec scenario reflection decisions

* EpisodeSpec validation과 EpisodeSpec scenario reflection을 분리한다.
* UE compiler readiness는 구조 validation뿐 아니라 scenario reflection까지 통과해야 true로 본다.
* Kickboard는 임시 mapping을 사용하되 `semantic_type`으로 원래 의도를 보존한다.
* pedestrian crossing은 `paths`와 `actors.pedestrians`의 연결로 검증한다.
# Root README decisions

* Root `README.md` is the project entry point.
* `docs/README.md` remains the detailed documentation index.
* Root `README.md` must state current UE handoff readiness, key endpoints, and execution commands.
* Sample JSON and fixture files are still not auto-generated.
* JSON Schema, Pydantic models, OpenAI provider code, vector DB, embedding index, and UE C++/Blueprint code are not changed by README documentation work.

# Handoff release readiness decisions

* Final UE delivery state is documented through a manifest, release notes, readiness checklist, and warning explanation.
* `PASS_WITH_WARNING` is acceptable for handoff because UE handoff checks pass and the remaining warnings are manual review workflow state.
* UE delivery continues to use `responseFormat=episode_spec` as the recommended endpoint option.
* `responseFormat=both` remains a debug option.
* `obstacle.kickboard` must still be confirmed by the UE team; temporary `obstacle.road_barrier_01` mapping remains documented.
* This release documentation does not introduce OpenAI calls, schema changes, sample JSON, fixtures, vector DB, embedding index, or UE code.

# Environment parameter decisions

* Environment scenario parameters are defined with concrete numeric values, not low/middle/high labels.
* Low/middle/high may appear in explanatory prose, but they must not be used as JSON contract values.
* Seed-based deterministic sampling must select from explicit numeric candidate lists.
* AI internal cm values are converted to meter values only at the EpisodeSpec adapter boundary when UE fields require meters.
* 환경 파라미터 샘플링은 seed 기반 deterministic 방식으로 구현한다.
* sampler는 numeric parameter set만 생성하고 WorldConfig/EpisodeSpec 생성은 후속 단계로 분리한다.
* fixed parameter는 allowedValues 안의 숫자만 허용한다.
* low/middle/high는 입력 값으로 허용하지 않는다.

# OpenAI provider decisions

* LLM provider 우선순위는 OpenAI first, Ollama fallback으로 변경한다.
* OpenAI는 정확도 우선 provider로 사용한다.
* Ollama는 비용 절감 및 fallback provider로 유지한다.
* OpenAI API key는 `.env`에서 읽고 코드에 하드코딩하지 않는다.
* 테스트와 하네스는 실제 OpenAI API를 호출하지 않는다.

# Report serialization decisions

* Smoke report는 공통 serialization helper를 통해 저장한다.
* API key와 full generatedPayload/full episodeSpec은 report에 저장하지 않는다.
* OpenAI live smoke 재실행 전 report serialization을 먼저 안정화한다.

# OpenAI-first handoff decisions

* OpenAI-first provider chain에서 WorldConfig generation과 EpisodeSpec handoff smoke가 통과했다.
* `providerUsed=openai`, `fallbackUsed=false` 결과를 handoff readiness 문서에 기록한다.
* Ollama는 fallback provider로 유지한다.
* API key와 full WorldConfig/full EpisodeSpec은 report에 저장하지 않는다.
* UE 실제 actor spawn, parser integration, route injection은 UE 팀 확인 단계로 둔다.

# Generic obstacle scenario decisions

* Generic obstacle prompt도 scenario intent/reflection/post-processing 대상으로 처리한다.
* 명시된 수치 값은 LLM 출력보다 우선한다.
* no pedestrian intent는 pedestrians가 없는 EpisodeSpec을 정상으로 허용한다.
* UE handoff endpoint 기본 responseFormat은 `episode_spec`으로 둔다.
* `responseFormat=world_config`는 AI 내부 구조 확인용이며 이 경우 `episodeSpec`은 `null`일 수 있다.

# Handoff smoke reporting decisions

* live smoke report는 API 응답 전체가 아니라 summary를 저장한다.
* generic obstacle smoke는 `handoff_response_summary` helper로 분석한다.
* OpenAI 재호출 전 report parsing 로직을 먼저 안정화한다.

# Environment sampler generation integration decisions

* environment sampler 결과를 WorldConfig generation constraints에 연결한다.
* numeric constraints는 LLM 출력보다 우선한다.
* sampler integration은 단일 요청용이며 DOE matrix는 후속 단계로 분리한다.
* low/middle/high는 JSON 값으로 사용하지 않는다.

## Environment Sampling Handoff Enforcement

* environmentSampling numeric constraints are applied even when the initial LLM payload is schema-valid.
* sampled/fixed numeric values override LLM output during deterministic post-processing.
* Obstacle/path-blocking requirements require EpisodeSpec `actors.static_obstacles` and `properties.blocking_ratio`.
* If required static obstacles or blocking ratio are missing, EpisodeSpec scenario reflection fails and UE handoff returns `success=false`.

## Environment Sampling Single Request Handoff

* environmentSampling numeric constraints are connected to single WorldConfig/EpisodeSpec generation.
* numeric constraints have priority over vague natural language.
* environmentSampling is currently for single request generation, not DOE matrix generation.
* batch scenario generation is deferred until UE controlled integration succeeds.

## Route-relative Placement Decisions

* 경로 중앙/중간 같은 상대 좌표 표현은 LLM 추론에 맡기지 않고 deterministic midpoint 계산으로 처리한다.
* 명시 좌표가 있으면 명시 좌표가 midpoint보다 우선한다.
* EpisodeSpec 변환 후에도 midpoint 근처 배치인지 검증한다.
* LLM이 obstacle을 goal 근처에 둔 경우에도 route_midpoint intent가 있으면 deterministic post-processing이 midpoint로 보정한다.
* live Swagger 응답이 in-memory 회귀 테스트와 다르면 stale FastAPI 서버 여부를 먼저 확인한다.

## UE EpisodeSpec Guide Alignment

* UE EpisodeSpec contract source는 `docs/UE_EPISODE_SPEC_JSON_GUIDE.md`이다.
* WorldConfig -> EpisodeSpec adapter는 이 guide와 일치해야 한다.
* UE guide가 바뀌면 guide 문서를 먼저 갱신하고 adapter/test를 갱신한다.
* Guide 비호환 `ground_model.regions[].penalties`는 EpisodeSpec output에 포함하지 않고 validator에서 invalid로 처리한다.

## Map Generation Data Source Decisions

* 법령 RAG는 좌표 생성 근거가 아니라 정책/안전 근거로 사용한다.
* 좌표와 배치는 사용자 입력, environmentSampling, UE EpisodeSpec guide, deterministic placement rule을 기준으로 결정한다.
