# 결정 기록

이 문서는 프로젝트에서 확정한 구현/운영 결정을 모아 둔다. 기술 식별자와 API 경로는 원문을 유지하고, 설명은 팀 공유가 쉽도록 한국어 중심으로 정리한다.

## UE5 Handoff Contract 결정

- schema validation, contract validation, scenario reflection을 모두 통과한 `WorldConfig`만 UE5 handoff 후보가 된다.
- UE5 handoff 응답은 `worldConfig`와 metadata/diagnostics를 함께 제공한다.
- UE5는 `worldConfig`를 실행 대상으로 사용하고, `diagnostics`와 `postProcessing`은 디버깅 용도로만 사용한다.
- 실패한 handoff 응답에는 `worldConfig`를 포함하지 않는다.
- sample JSON과 fixture 파일은 자동 생성하지 않는다.

## Scenario Post-Processing Before Fallback 결정

- schema는 통과했지만 scenario reflection이 실패한 payload는 먼저 deterministic scenario post-processing을 거친다.
- post-processing은 추출된 scenario intent에서 직접 유도되는 누락 요소만 보강한다.
- post-processing 후에도 실패하면 scenario repair prompt flow를 사용한다.
- OpenAI fallback은 local post-processing과 repair가 실패한 뒤에만 검토한다.

## Ollama Manual Smoke 결정

- 수동 Ollama live 검증은 `scripts/run_ollama_world_config_smoke.py`를 사용한다.
- `--dry-run`은 Ollama를 호출하지 않고 prompt package만 구성하는 안전한 preflight 경로다.
- automated tests와 harness checks는 실제 Ollama server를 호출하지 않는다.
- smoke runner는 `--report`가 명시된 경우에만 report를 저장한다.
- smoke runner는 sample JSON, fixture, vector DB, embedding index를 생성하지 않는다.
- Ollama validation 실패 분석을 위해 attempt-level diagnostics를 기록한다.
- full raw output 저장은 기본 비활성화하며 `--include-raw-attempts`를 명시한 경우에만 허용한다.
- prompt 개선, schema 변경, fallback 결정은 detailed diagnostics를 기준으로 판단한다.
- provider timeout은 `validation_failed`와 별도로 분류한다.
- timeout tuning은 warm-up, timeout 증가, repair attempt 축소, context topK 축소, compact prompt를 먼저 적용한 뒤 fallback을 검토한다.

## Provider 결정

- OpenAI는 현재 1순위 provider다.
- Ollama는 비용 절감과 회복성을 위한 fallback provider로 유지한다.
- provider chain은 `openai, ollama`를 기준으로 한다.
- OpenAI API key는 환경 설정에서 읽으며 실제 `.env` 파일은 commit하지 않는다.
- API key와 secret은 source code, 문서, report에 하드코딩하지 않는다.
- LLM output은 JSON extraction과 validation을 통과한 뒤에만 `generatedPayload`가 될 수 있다.

## 사용자용 Scenario Generation API 결정

- 사용자용 API는 `POST /api/v1/scenarios/generate`다.
- 입력은 자연어 `prompt` 하나만 허용한다.
- 사용자가 EpisodeSetup / DeliveryBotSetup / RunQueue JSON을 직접 작성하는 구조가 아니다.
- AI와 backend가 내부적으로 `WorldConfig`를 만들고, deterministic variation/export 경로를 거쳐 UE 실행용 RunQueue JSON을 반환한다.
- 성공 응답은 wrapper field 없는 RunQueue JSON이며 최상위 필드는 `schema`, `version`, `runs`만 포함한다.
- EpisodeSetup / DeliveryBotSetup / RunQueue export는 null-free 정책을 따른다.
- 기존 `/api/v1/ue5/world-config/handoff` route는 FastAPI/OpenAPI에서 제거한다. `responseFormat=setup_pair` 기반 내부 service/export tooling은 archive와 CLI 검증 맥락에서만 유지한다.

## Policy Comparison 결정

- `narrow_sidewalk_obstacle_ahead_blocked_path`는 scene variation이 아니라 policy comparison 구조로 생성한다.
- 모든 run은 동일한 고정 EpisodeSetup을 참조한다.
- DeliveryBotSetup만 `baseline`, `short_stop`, `long_stop`, `early_slowdown`, `low_speed` 5개 policy별로 변경한다.
- 고정 EpisodeSetup은 robot `[0.0, 0.0]`, obstacle `[5.5, 0.0]`, goal `[10.5, 0.0]`, `pedestrians=[]`를 사용한다.
- DeliveryBotSetup tuning 값은 LLM이 임의 생성하지 않고 UE 문서 default catalog와 deterministic variation policy를 기준으로 생성한다.

## World Config Generation Orchestrator 결정

- generation orchestrator는 prompt package 생성, LLM client 호출, JSON extraction, contract validation, scenario reflection, post-processing, repair loop를 연결한다.
- `disabled` provider는 명시적인 실패 결과를 반환하며 generated payload를 만들지 않는다.
- LLM response content는 JSON extraction과 validation을 통과해야 `generatedPayload`가 된다.
- repair loop는 validation error와 scenario reflection 결과를 기준으로 동작한다.
- FastAPI generation endpoint는 내부 generation flow를 노출하되, automated tests와 harness에서는 실제 provider 호출을 하지 않는다.

## LLM Client Abstraction 결정

- provider abstraction은 실제 provider별 구현과 API endpoint를 분리한다.
- provider는 `disabled`, `openai`, `gemini`, `ollama`, `custom` 식별자를 유지한다.
- API key와 secret은 source code에 하드코딩하지 않는다.
- LLM output은 validation layer를 통과한 뒤에만 UE5 handoff 후보가 된다.

## API Shell 결정

- 사용자용 기본 FastAPI `/api/v1` surface는 `POST /api/v1/scenarios/generate`다.
- `POST /api/v1/scenarios/generate-artifacts`와 `POST /api/v1/scenarios/generate-drive`는 public API에서 제거한다.
- prompt package 생성과 JSON contract validation은 service/helper 함수와 CLI로 검증한다.
- sample JSON과 fixture 파일은 이 단계에서 생성하지 않는다.

## World Config Prompt Builder 결정

- 자연어 입력은 API 또는 UI를 통해 들어올 수 있지만, prompt builder는 내부 service layer로 유지한다.
- WorldConfig generation prompt package는 deterministic policy RAG retrieval context를 사용한다.
- sample JSON 파일은 자동 생성하지 않는다.

## RAG Retrieval 결정

- embedding/vector DB 작업 전에 deterministic retrieval을 먼저 구현한다.
- 현재 RAG retrieval 대상은 confirmed policy-card chunk set인 `data/rag/policy_rag_chunks.jsonl`이다.
- source document RAG는 별도 later track으로 유지하며, 이 단계에서는 raw PDF나 processed Markdown chunk를 직접 사용하지 않는다.
- 자연어 WorldConfig generator는 retrieval layer를 통해 관련 policy chunk를 prompt context로 받는다.
- embedding behavior가 별도로 도입되기 전까지 retrieval은 keyword, category, action, policy-parameter, source 기반으로 동작한다.

- policy knowledge card는 manual confirmation에서 confirmed 처리된 후보만 생성 대상으로 삼는다.
- pending_manual_confirmation 후보와 rejected 후보는 policy knowledge card로 변환하지 않는다.
- confirmed 후보가 0개이면 policy_knowledge_cards.jsonl을 생성하지 않는다.
- policy knowledge card는 프로젝트 내부 정책 기준이며 공식 인증 준수를 의미하지 않는다.
- source registry에 없는 sourceId를 가진 confirmed 후보는 card 생성 실패로 처리한다.

# 이전 결정 기록

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
# 현재 Policy Card 결정

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

- Ollama JSON extraction 실패와 validation 실패는 별도로 진단한다.
- JSON extraction은 성공했지만 `world_config` validation이 실패하면 OpenAI fallback 전에 prompt/schema summary hardening을 먼저 시도한다.
- prompt hardening은 기존 JSON Schema를 사용하며 schema file을 수정하지 않는다.
- repair prompt는 missing required field와 schema-extra field를 명시해야 한다.
- prompt hardening 중 sample JSON이나 fixture 파일은 생성하지 않는다.

## Decision: Separate Schema Validation And Scenario Reflection

- schema validation과 semantic scenario reflection validation은 별도 check다.
- schema-valid `world_config`라도 자연어 scenario 조건이 누락되면 성공으로 보지 않는다.
- Korean keyword expansion은 RAG retrieval 전에 실행한다.
- Scenario requirement는 World Config prompt package에 주입한다.
- scenario detail이 누락됐다고 곧바로 OpenAI fallback을 사용하지 않고, intent extraction, retrieval, prompt guidance를 먼저 개선한다.

## Decision: Strengthen Output Contract Before Provider Fallback

- schema validation이 실패하면 OpenAI fallback 전에 output contract prompt hardening을 먼저 시도한다.
- prompt 안에서 scenario requirement를 schema path와 연결한다.
- prompt hardening을 위해 JSON Schema는 수정하지 않는다.
- smoke report는 output contract와 scenario requirement diagnostics를 기록한다.

## Decision: Add Scenario Repair Before Provider Fallback

- schema validation과 scenario reflection validation은 계속 별도 stage로 유지한다.
- schema validation은 통과했지만 scenario reflection이 실패하면 provider fallback을 검토하기 전에 scenario repair loop를 실행한다.
- scenario repair는 schema-valid JSON을 유지하고 누락된 semantic requirement만 보강한다.
- 최종 성공은 schema validation과 scenario reflection validation을 모두 통과해야 한다.
# UE5 EpisodeSpec adapter 결정

* AI 내부 계약은 `WorldConfig`로 유지한다.
* UE 실행 계약은 `EpisodeSpec`으로 변환한다.
* UE5 담당자 문서 기준으로 EpisodeSpec adapter를 둔다.
* Kickboard는 현재 catalog에 없으므로 임시 prop mapping을 사용하고 warning을 남긴다.
* UE 쪽에는 `obstacle.kickboard` prop 추가 여부 확인이 필요하다.
* JSON Schema, sample JSON, fixture, UE C++/Blueprint 코드는 이 단계에서 만들지 않는다.

# UE5 EpisodeSpec handoff smoke 결정

* 이전 UE 실행용 handoff smoke는 `EpisodeSpec` format을 우선 지원했다.
* `WorldConfig`는 AI 내부 검증/추론용 계약으로 유지한다.
* 현재 public UE 연동 API는 `/api/v1/scenarios/generate`다.
* `responseFormat=episode_spec`과 `responseFormat=both`는 archive/tooling 참고용으로만 남긴다.
* Kickboard는 임시 prop mapping을 사용하며 UE 쪽 catalog 추가를 요청한다.

# UE5 EpisodeSpec scenario reflection 결정

* EpisodeSpec validation과 EpisodeSpec scenario reflection을 분리한다.
* UE compiler readiness는 구조 validation뿐 아니라 scenario reflection까지 통과해야 true로 본다.
* Kickboard는 임시 mapping을 사용하되 `semantic_type`으로 원래 의도를 보존한다.
* pedestrian crossing은 `paths`와 `actors.pedestrians`의 연결로 검증한다.
# Root README 결정

* Root `README.md` is the project entry point.
* `docs/README.md` remains the detailed documentation index.
* Root `README.md` must state current scenario generation readiness, key endpoints, and execution commands.
* Sample JSON and fixture files are still not auto-generated.
* JSON Schema, Pydantic models, OpenAI provider code, vector DB, embedding index, and UE C++/Blueprint code are not changed by README documentation work.

# Handoff release readiness 결정

* Final UE delivery state is documented through a manifest, release notes, readiness checklist, and warning explanation.
* `PASS_WITH_WARNING` is acceptable when scenario generation/RunQueue checks pass and the remaining warnings are manual review workflow state.
* UE delivery uses `/api/v1/scenarios/generate` and RunQueue export as the recommended path.
* Legacy `responseFormat=episode_spec` and `responseFormat=both` are archive/tooling references only.
* `obstacle.kickboard` must still be confirmed by the UE team; temporary `obstacle.road_barrier_01` mapping remains documented.
* This release documentation does not introduce OpenAI calls, schema changes, sample JSON, fixtures, vector DB, embedding index, or UE code.

# Environment parameter 결정

* Environment scenario parameters are defined with concrete numeric values, not low/middle/high labels.
* Low/middle/high may appear in explanatory prose, but they must not be used as JSON contract values.
* Seed-based deterministic sampling must select from explicit numeric candidate lists.
* AI internal cm values are converted to meter values only at the EpisodeSpec adapter boundary when UE fields require meters.
* 환경 파라미터 샘플링은 seed 기반 deterministic 방식으로 구현한다.
* sampler는 numeric parameter set만 생성하고 WorldConfig/EpisodeSpec 생성은 후속 단계로 분리한다.
* fixed parameter는 allowedValues 안의 숫자만 허용한다.
* low/middle/high는 입력 값으로 허용하지 않는다.

# OpenAI provider 결정

* LLM provider 우선순위는 OpenAI first, Ollama fallback으로 변경한다.
* OpenAI는 정확도 우선 provider로 사용한다.
* Ollama는 비용 절감 및 fallback provider로 유지한다.
* OpenAI API key는 `.env`에서 읽고 코드에 하드코딩하지 않는다.
* 테스트와 하네스는 실제 OpenAI API를 호출하지 않는다.

# Report serialization 결정

* Smoke report는 공통 serialization helper를 통해 저장한다.
* API key와 full generatedPayload/full episodeSpec은 report에 저장하지 않는다.
* OpenAI live smoke 재실행 전 report serialization을 먼저 안정화한다.

# OpenAI-first handoff 결정

* OpenAI-first provider chain에서 WorldConfig generation과 EpisodeSpec handoff smoke가 통과했다.
* `providerUsed=openai`, `fallbackUsed=false` 결과를 handoff readiness 문서에 기록한다.
* Ollama는 fallback provider로 유지한다.
* API key와 full WorldConfig/full EpisodeSpec은 report에 저장하지 않는다.
* UE 실제 actor spawn, parser integration, route injection은 UE 팀 확인 단계로 둔다.

# Generic obstacle scenario 결정

* Generic obstacle prompt도 scenario intent/reflection/post-processing 대상으로 처리한다.
* 명시된 수치 값은 LLM 출력보다 우선한다.
* no pedestrian intent는 pedestrians가 없는 EpisodeSpec을 정상으로 허용한다.
* Legacy UE handoff model 기본 responseFormat은 archive/tooling 호환을 위해 `episode_spec`으로 남긴다.
* `responseFormat=world_config`는 archive/tooling의 AI 내부 구조 확인용이며 public route로 노출하지 않는다.

# Handoff smoke reporting 결정

* live smoke report는 API 응답 전체가 아니라 summary를 저장한다.
* generic obstacle smoke는 `handoff_response_summary` helper로 분석한다.
* OpenAI 재호출 전 report parsing 로직을 먼저 안정화한다.

# Environment sampler generation integration 결정

* environment sampler 결과를 WorldConfig generation constraints에 연결한다.
* numeric constraints는 LLM 출력보다 우선한다.
* sampler integration은 단일 요청용이며 DOE matrix는 후속 단계로 분리한다.
* low/middle/high는 JSON 값으로 사용하지 않는다.

## Environment Sampling Handoff Enforcement 결정

* environmentSampling numeric constraints are applied even when the initial LLM payload is schema-valid.
* sampled/fixed numeric values override LLM output during deterministic post-processing.
* Obstacle/path-blocking requirements require EpisodeSpec `actors.static_obstacles` and `properties.blocking_ratio`.
* If required static obstacles or blocking ratio are missing, EpisodeSpec scenario reflection fails and UE handoff returns `success=false`.

## Environment Sampling Single Request Handoff 결정

* environmentSampling numeric constraints are connected to single WorldConfig/EpisodeSpec generation.
* numeric constraints have priority over vague natural language.
* environmentSampling is currently for single request generation, not DOE matrix generation.
* batch scenario generation is deferred until UE controlled integration succeeds.

## Route-relative Placement 결정

* 경로 중앙/중간 같은 상대 좌표 표현은 LLM 추론에 맡기지 않고 deterministic midpoint 계산으로 처리한다.
* 명시 좌표가 있으면 명시 좌표가 midpoint보다 우선한다.
* EpisodeSpec 변환 후에도 midpoint 근처 배치인지 검증한다.
* LLM이 obstacle을 goal 근처에 둔 경우에도 route_midpoint intent가 있으면 deterministic post-processing이 midpoint로 보정한다.
* live Swagger 응답이 in-memory 회귀 테스트와 다르면 stale FastAPI 서버 여부를 먼저 확인한다.

## UE EpisodeSpec Guide Alignment 결정

* UE EpisodeSpec contract source는 `docs/archive/previous_episode_spec/UE_EPISODE_SPEC_JSON_GUIDE.md`이다.
* WorldConfig -> EpisodeSpec adapter는 이 guide와 일치해야 한다.
* UE guide가 바뀌면 guide 문서를 먼저 갱신하고 adapter/test를 갱신한다.
* Guide 비호환 `ground_model.regions[].penalties`는 EpisodeSpec output에 포함하지 않고 validator에서 invalid로 처리한다.

## Map Generation Data Source 결정

* 법령 RAG는 좌표 생성 근거가 아니라 정책/안전 근거로 사용한다.
* 좌표와 배치는 사용자 입력, environmentSampling, UE EpisodeSpec guide, deterministic placement rule을 기준으로 결정한다.

## Map Generation Trace 결정

* Map generation trace는 full payload가 아니라 summary evidence만 저장한다.
* API key, rawContent, full WorldConfig, full EpisodeSpec은 trace에 저장하지 않는다.
* 법령 RAG는 좌표 생성 근거가 아니라 정책/안전 근거로 기록한다.
* `includeDiagnostics=true`일 때만 handoff diagnostics에 `generationTrace`를 포함한다.
* generationTrace 생성 실패는 UE handoff 성공/실패 판정을 깨뜨리지 않고 `diagnostics.generationTraceError`로 분리한다.
* 성공 경로의 generationTrace에는 `episode_spec_adapter`와 `scenario_reflection` 근거를 포함한다.
* 실패 경로의 generationTrace는 `failureStage`와 `errorSummary`를 기록한다.
* `success=false` handoff 응답은 `failureStage`를 null로 남기지 않고, 원인을 특정하지 못하면 `unknown`으로 기록한다.

## Research Alignment 결정

* Scenic is research inspiration only at this stage.
* Proto-AI currently uses WorldConfig/EpisodeSpec, not Scenic DSL.
* Placement and sampling rules may evolve toward Scenic-like constraint-based scene generation.
* Scenic runtime dependency is not introduced at this stage.

## UE Contract Migration 결정

* 최신 UE 계약 문서는 `docs/ue_contracts/` 아래 문서를 기준으로 한다.
* Scenario는 추상적인 상황 유형이고 Episode는 실제 한 번 실행되는 구체 시뮬레이션 인스턴스다.
* `scenario_id`는 생성 의도/유형 식별자로 사용하고 실제 실행 단위는 `pair_id`, `run_id`, `requestId` 등으로 추적한다.
* 현재 구현된 EpisodeSpec handoff service/model은 legacy archive/tooling 목적으로 유지하되 public route는 제거한다.
* 향후 UE 입력 계약은 EpisodeSetup + DeliveryBotSetup pair로 전환한다.
* EpisodeSetup은 맵/액터/경로/로봇 배치/로봇 목적지를 담당한다.
* DeliveryBotSetup은 `drive`, `path_follow`, `lidar` 튜닝값을 담당하고 로봇 배치/route/run 정보는 포함하지 않는다.
* EpisodeSetup / DeliveryBotSetup 모델, validator, WorldConfig 변환 adapter는 API 통합 전 내부 구현으로 먼저 추가한다.
* setup pair trace helper는 diagnostics의 `setupPairTrace`로 연결하고 full WorldConfig/EpisodeSetup/DeliveryBotSetup은 저장하지 않는다.
* 이전 `responseFormat=setup_pair` smoke는 EpisodeSetup + DeliveryBotSetup pair 생성을 검증했다. 현재 public route는 `/api/v1/scenarios/generate`다.
* EvaluationReport 기반 결과 분석과 Result Analysis Agent 구현은 이번 문서 정리 범위가 아니며 별도 담당 범위로 분리한다.
* setup pair live smoke는 OpenAI provider로 성공했으며 EpisodeSetup/DeliveryBotSetup validation을 모두 통과했다.
* setup pair fine-tuning candidate full JSON은 `data/fine_tuning_candidates/`에 로컬 보관하며 git commit 대상이 아니다.
* 기존 `episode_spec` route는 public API에서 제거하고 archive/tooling 참고로만 유지한다.
* EvaluationReport 분석 구현은 setup pair handoff 문서화 범위에 포함하지 않는다.

## Docs Inventory And Cleanup 결정

* `docs/ue_contracts/`는 공식 UE 계약 문서 경로로 유지하고 이동/rename하지 않는다.
* `docs/policy_server/POLICY_DECISION_JSON_GUIDE.md`는 공식 Policy Decision 계약 문서 경로로 유지하고 이동/rename하지 않는다.
* 기존 EpisodeSpec 문서는 파일 상단에 legacy 문구를 추가하지 않고 `docs/archive/previous_episode_spec/` 위치로 구분한다.
* 영어 설명이 많은 문서는 별도 계획 문서가 아니라 실제 문서 본문에서 단계적으로 한국어화한다.
* `DOCS_INVENTORY.md`, `KOREAN_DOCS_CONVERSION_PLAN.md`, `DOCUMENT_CLEANUP_PLAN.md`는 정리 완료 후 삭제하는 임시 관리 문서로 본다.

## RunQueue Export 결정

* 자연어 시나리오 1개에서 기본 5개 EpisodeSetup + DeliveryBotSetup pair를 생성한다.
* episode variation은 LLM을 episode 수만큼 재호출하지 않고 deterministic variation과 environment sampling 값을 사용한다.
* RunQueue JSON은 `docs/ue_contracts/RUN_QUEUE_JSON.md` 계약 그대로 `schema`, `version`, `runs`만 포함한다.
* RunQueue JSON에는 `success`, `responseFormat`, `diagnostics`, `setupPairs`, `episodeSetup`, `deliveryBotSetup`, `validation`, `trace`를 넣지 않는다.
* diagnostics, validation, trace summary는 UE용 RunQueue JSON과 분리해 local ignored report로만 저장한다.
* RunQueue export는 `scripts/export_ue5_run_queue_package.py`와 사용자용 `POST /api/v1/scenarios/generate` 경로에서 제공한다.
* `/api/v1/scenarios/generate`는 자연어 prompt만 입력받고, 사용자가 EpisodeSetup / DeliveryBotSetup / RunQueue JSON을 직접 작성하지 않는다.
* API 응답 RunQueue JSON은 AI와 backend가 내부 생성한 UE 실행 결과물이다.
* export 산출물은 `data/run_queue_exports/` 아래에 저장하고 repository에 포함하지 않는다.
* 기존 export target에 `Json/Input/*.json`이 있으면 삭제하거나 덮어쓰지 않고 `_backup`으로 이동한다.

## Null-free UE JSON 결정

* EpisodeSetup / DeliveryBotSetup / RunQueue JSON에는 `null`을 출력하지 않는다.
* optional field는 값이 없으면 생략한다.
* optional field는 policy, deterministic variation, adapter가 실제 값을 지정한 경우에만 출력한다.
* 생략된 DeliveryBotSetup field는 UE C++ 구조체 기본값 fallback에 맡긴다.
* DeliveryBotSetup tuning 값은 LLM이 직접 임의 수치로 생성하지 않는다.
* DeliveryBotSetup tuning 값은 `docs/ue_contracts/DELIVERY_BOT_SETUP_JSON.md` default catalog와 deterministic variation policy 기준으로 생성한다.
* 사용자용 `POST /api/v1/scenarios/generate`는 prompt 하나만 받고 RunQueue JSON 계약 그대로 반환한다.

## v2 Agent API 결정

* v2 Agent API는 v1 API 계약을 변경하지 않는 신규 경로로 유지한다.
* `/api/v2/scenarios/generate`는 prompt만 받고 `scenario.template.json` 형태의 template을 반환한다.
* v2 scenario generation은 `episode_count`, `count`, `iterations`, `run_count`, seed, RunQueue 생성을 담당하지 않는다.
* `scenario.template.json`은 range/choice/random 요소를 가질 수 있는 원본 template이고, `scenario.json`은 template sampling 결과로 확정값을 가진 실행 sample이다.
* `/api/v2/analysis/run`은 현재 파라미터 없이 experiments root 전체를 분석한다.
* v2 Agent 기본값은 deterministic/rule-based mode이며 외부 API key 없이 테스트가 통과해야 한다.
* `V2_AGENT_LLM_ENABLED=true`일 때만 optional LLM JSON 경로를 사용한다.
* LLM 호출 실패, JSON 파싱 실패, validator 실패, 잘못된 evidence는 API 500이 아니라 fallback response와 warning으로 처리한다.
* LLM은 수치 계산을 하지 않고 해석과 추천만 담당한다. raw log 전체는 LLM에 전달하지 않는다.
