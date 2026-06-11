# HARNESS_GUIDE

이 문서는 harness check가 무엇을 검증하는지 설명한다. harness와 pytest는 실제 OpenAI/Ollama 호출을 하지 않고, 문서/모델/서비스/CLI 연결 상태와 금지 산출물 생성을 확인한다.

## UE5 Handoff Check

`harness.checks.check_ue5_handoff`는 live Ollama server를 호출하지 않고 UE5 handoff model, service, endpoint, smoke runner, documentation을 검증한다. sample JSON, fixture, vector DB, embedding artifact가 생성되지 않았는지도 확인한다.

## Scenario Post-Processing Check

`harness.checks.check_scenario_post_processing`는 deterministic World Config scenario post-processing이 구현/문서화됐고 orchestrator에 연결됐는지 확인한다. OpenAI 호출, sample, fixture, vector DB, embedding artifact를 추가하지 않는 것도 검증한다.

## Ollama Live Smoke Tooling Check

`harness.checks.check_ollama_live_smoke_tooling`은 실제 Ollama server를 호출하지 않고 manual Ollama smoke runner의 구조를 검증한다.

검증 항목:

- `scripts/run_ollama_world_config_smoke.py` exists.
- `docs/providers/OLLAMA_LIVE_SMOKE_GUIDE.md` exists.
- `--help` works.
- `--dry-run` builds a prompt package and does not attempt a live Ollama request.
- No sample, fixture, vector DB, or embedding index artifacts are created.

manual live execution은 automated tests 밖에서만 수행한다.

## Ollama Failure Diagnostics Check

`harness.checks.check_ollama_failure_diagnostics`는 실패한 Ollama smoke run을 분석하기 위한 diagnostic tooling을 검증한다.

검증 항목:

- `docs/providers/OLLAMA_FAILURE_DIAGNOSTICS.md` exists.
- Live smoke script exposes `--include-raw-attempts`, `--include-extracted-json`, and `--raw-preview-chars`.
- `WorldConfigGenerationAttempt` includes attempt-level diagnostics fields.
- No OpenAI SDK import or call code is present.
- No sample, fixture, vector DB, or embedding index artifacts are created.

## Ollama Provider Check

`harness.checks.check_ollama_provider`는 Ollama provider 구현을 검증한다.

검증 항목:

- `app/services/llm_ollama_client.py` exists.
- Factory returns `OllamaLlmClient` for `provider=ollama`.
- `docs/providers/OLLAMA_PROVIDER_GUIDE.md` exists.
- No OpenAI SDK import or call code is present.
- No hardcoded API key or secret string is present.
- No sample JSON, fixture, vector DB, or embedding index artifacts are created.
- The harness does not call a real Ollama server.

## LLM Provider Config Check

`harness.checks.check_llm_provider_config`는 provider settings와 fallback policy setup을 검증한다.

검증 항목:

- `.env.example` exists.
- `OPENAI_API_KEY` is empty in `.env.example`.
- `app/core/settings.py` exists.
- `app/services/llm_provider_policy.py` exists.
- `docs/providers/LLM_PROVIDER_CONFIGURATION.md` exists.
- The harness does not perform a live external LLM call.
- No hardcoded API key or secret string is present.
- No sample JSON, fixture, vector DB, or embedding index artifacts are created.

## Generation Endpoint Check

`harness.checks.check_generation_endpoint`는 World Config generation endpoint shell을 검증한다.

검증 항목:

- `/api/v1/scenarios/generate` and `/api/v1/analysis/run` are the only public `/api/v1` endpoints.
- `/api/v1/scenarios/generate-artifacts` and `/api/v1/scenarios/generate-drive` are not registered.
- WorldConfig generation, prompt package, and contract validation remain covered through service/function or CLI tests.
- The harness does not perform a live external LLM call.
- No hardcoded API key or secret string is present.
- Policy card count remains 9.
- Policy RAG chunk count remains 9.
- No sample JSON, fixture, vector DB, or embedding index artifacts are created.

## World Config Generation Orchestrator Check

`harness.checks.check_world_config_generation_orchestrator`는 내부 generation orchestration layer를 검증한다.

검증 항목:

- `app/services/world_config_generation_orchestrator.py` exists.
- `app/services/json_output_extractor.py` exists.
- `docs/architecture/WORLD_CONFIG_GENERATION_ORCHESTRATOR.md` exists.
- The harness does not perform a live external LLM call.
- No hardcoded API key or secret string is present.
- FastAPI endpoint set is limited to `/health`, `/api/v1/scenarios/generate`, and `/api/v1/analysis/run`.
- Policy card count remains 9.
- Policy RAG chunk count remains 9.
- No sample JSON, fixture, vector DB, or embedding index artifacts are created.

## LLM Client Abstraction Check

`harness.checks.check_llm_client_abstraction`은 외부 LLM을 호출하지 않고 provider abstraction layer를 검증한다.

검증 항목:

- `app/models/llm.py` exists.
- `app/services/llm_client.py` exists.
- `app/services/llm_disabled_client.py` exists.
- `app/services/llm_client_factory.py` exists.
- `docs/providers/LLM_CLIENT_ABSTRACTION.md` exists.
- Providers include `disabled`, `openai`, `gemini`, `ollama`, and `custom`.
- The `disabled` provider returns `DisabledLlmClient`.
- FastAPI endpoint set is limited to `/health`, `/api/v1/scenarios/generate`, and `/api/v1/analysis/run`.
- The harness does not perform a live external LLM call.
- No hardcoded API key or secret string is present.
- No sample JSON, fixture, vector DB, or embedding index artifacts are created.

## API Shell Check

`harness.checks.check_api_shell`은 FastAPI public surface가 `/api/v1/scenarios/generate`와 `/api/v1/analysis/run`만 노출하고 제거된 `/api/v1` endpoint를 등록하지 않는지 검증한다.

검증 항목:

- `app/main.py` exists and exposes `app`.
- `app/api/routes.py` exists.
- `docs/tooling/API_SHELL_GUIDE.md` exists.
- `GET /health` is registered.
- `POST /api/v1/scenarios/generate` is registered.
- `POST /api/v1/analysis/run` is registered.
- `POST /api/v1/scenarios/generate-artifacts` is not registered.
- `POST /api/v1/scenarios/generate-drive` is not registered.
- The API shell does not expose provider/contract validation routes; harness checks do not perform live OpenAI/Ollama calls.
- No sample JSON, fixture, vector DB, or embedding index artifacts are created.

Run:

```bash
uv run uvicorn app.main:app --reload
```

Swagger:

```text
http://127.0.0.1:8000/docs
```

## World Config Prompt Builder Check

`harness.checks.check_world_config_prompt_builder`는 내부 natural-language-to-World-Config prompt package layer를 검증한다.

검증 항목:

- `app/models/generation.py` exists.
- `app/services/natural_language_normalizer.py` exists.
- `app/services/world_config_rag_context_builder.py` exists.
- `app/services/world_config_prompt_builder.py` exists.
- The prompt builder references `world_config.schema.json`.
- `data/rag/policy_rag_chunks.jsonl` still contains 9 chunks.
- Building a prompt package succeeds without an external LLM call.
- No API, sample JSON, fixture, vector DB, or embedding index artifacts are created.

## RAG Retrieval Check

`harness.checks.check_rag_retrieval`은 deterministic policy RAG retrieval layer를 검증한다.

검증 항목:

- `app/models/rag.py` exists.
- `app/services/policy_rag_retriever.py` exists.
- `scripts/search_policy_rag.py` exists.
- `docs/rag/RAG_RETRIEVAL_STRATEGY.md` exists.
- `data/rag/policy_rag_chunks.jsonl` exists and contains 9 chunks.
- Keyword search for `비상정지` returns at least one result.
- Action search for `EmergencyStop` returns at least one result.
- Category search for `speed_policy` returns at least one result.
- Policy parameter search for `emergencyStopDistanceCm` returns at least one result.
- No vector DB, embedding index, source document chunks, sample JSON, fixture, or API artifacts are created.

Example commands:

```bash
uv run python scripts/search_policy_rag.py --query "비상정지"
uv run python scripts/search_policy_rag.py --category speed_policy
uv run python scripts/search_policy_rag.py --action EmergencyStop
uv run python -m harness.checks.check_all
```

## 하네스 목적

하네스는 Source Registry가 프로젝트의 실제 원본 PDF와 일치하는지 검증하고, 다음 단계 작업 전에 출처 관리 상태를 점검하기 위한 최소 검증 계층이다.

## Source Registry 검증 방법

`harness.checks.check_sources`는 다음 항목을 검증한다.

- `data/sources/policy_source_registry.json` 존재 여부
- JSON 파싱 가능 여부
- `sourceId` 중복 여부
- 필수 필드 존재 여부
- `filePath` 실제 파일 존재 여부
- `status` 허용값 여부
- `sourceType` 허용값 여부
- `usagePurpose` 비어 있지 않은 리스트 여부

## 실행 명령

```bash
uv run python -m harness.checks.check_all
uv run pytest
```

## Processed Source 검증

`harness.checks.check_processed_sources`는 다음 항목을 검증한다.

- registry의 KOR-001~KOR-005에 대응하는 processed Markdown 파일 존재 여부
- processed Markdown의 `Source Metadata` 섹션 존재 여부
- processed Markdown 내용에 `sourceId` 포함 여부
- `extractionStatus` 허용값 여부
- `data/sources/processed/source_processing_report.json` 존재 여부

`partial`, `failed`, `needs_manual_review` 상태는 전체 실패가 아니라 warning으로 처리한다. processed 파일 자체가 없거나 필수 메타데이터가 없으면 실패로 처리한다.

## Review Readiness 검증

`harness.checks.check_review_readiness`는 다음 항목을 검증한다.

- `data/sources/review/review_status.json` 존재 여부
- KOR-001~KOR-005가 모두 review status에 포함되어 있는지 여부
- 각 sourceId에 대응하는 review checklist 파일 존재 여부
- 각 checklist의 `Source Metadata` 섹션 존재 여부
- 각 checklist의 `정책 추출 후보 영역` 존재 여부
- `reviewStatus` 허용값 여부
- reviewed 처리 전 policy card가 생성되지 않았는지 여부

현재 모든 문서가 `not_started`인 상태는 검토 대기 상태이므로 실패가 아니라 warning으로 처리한다.

## Policy Candidate 검증

`harness.checks.check_policy_candidates`는 다음 항목을 검증한다.

- `data/sources/review/candidates/policy_candidate_index.json` 존재 여부
- KOR-001~KOR-005가 candidate index에 모두 포함되어 있는지 여부
- 각 `candidateFilePath`의 실제 파일 존재 여부
- 각 candidate 파일의 `Source Metadata` 섹션 존재 여부
- 각 candidate 파일의 `정책 후보 목록` 섹션 존재 여부
- 각 candidate와 index의 `reviewStatus`가 `needs_pdf_check`인지 여부
- policy knowledge card 파일이 생성되었거나 내용이 있는지 여부

후보 수가 0인 문서는 warning으로 처리한다. 현재 단계에서 policy knowledge card에 내용이 있으면 실패로 처리한다.

## Policy Triage 검증

`harness.checks.check_policy_triage`는 다음 항목을 검증한다.

- `data/sources/review/triage/policy_candidate_triage.json` 존재 여부
- `data/sources/review/triage/policy_candidate_triage.md` 존재 여부
- `docs/manual_review/MANUAL_REVIEW_QUEUE.md` 존재 여부
- `totalCandidates`가 201인지 여부
- `reviewQueue` 존재 여부
- 모든 `reviewStatus`가 `needs_pdf_check`인지 여부
- `priority` 값이 `high`, `medium`, `low` 중 하나인지 여부
- high priority 후보 존재 여부
- policy knowledge card, sample JSON, fixture, JSON schema 생성 여부

triage는 후보 상태를 바꾸지 않고 수동 검토 순서만 정한다.

## High Priority Review 검증

`harness.checks.check_high_priority_review`는 다음 항목을 검증한다.

- `data/sources/review/high_priority/high_priority_review_queue.json` 존재 여부
- `data/sources/review/high_priority/high_priority_review_queue.md` 존재 여부
- `totalHighPriorityCandidates`가 43인지 여부
- `items`가 비어 있지 않은지 여부
- 모든 `manualReviewStatus`가 `pending_manual_confirmation`인지 여부
- KOR-001~KOR-005 source별 high priority review 파일 존재 여부
- policy knowledge card, sample JSON, fixture, JSON schema, API 파일 생성 여부

High priority 후보도 원본 PDF 대조 전에는 confirmed가 아니며, 이 검증은 수동 확인 workspace의 준비 상태만 확인한다.

## Manual Confirmation 검증

`harness.checks.check_manual_confirmation`은 다음 항목을 검증한다.

- `data/sources/review/confirmed/manual_confirmation_results.json` 존재 여부
- high priority 후보 43개가 모두 포함되어 있는지 여부
- candidateId 중복 여부
- `manualReviewStatus` 허용값 여부
- `confirmed` 항목의 필수 수동 입력 필드 작성 여부
- `rejected` 항목의 `decisionReason` 작성 여부
- `pending_manual_confirmation` 항목의 빈 필드 허용 여부
- policy knowledge card, sample JSON, fixture, JSON schema, API 파일 생성 여부

confirmed/rejected가 0개여도 통과한다. pending 상태가 남아 있으면 다음 단계는 policy card 생성이 아니라 원본 PDF 수동 대조다.

Manual confirmation 입력은 `scripts/manual_confirm.py`로 지원한다. CLI는 자동 판단을 하지 않고, `--yes`가 없으면 파일을 수정하지 않는다.

## Page Hint 검증

`harness.checks.check_page_hints`는 다음 항목을 검증한다.

- `high_priority_page_hints.json` 존재 여부
- `high_priority_page_hints.md` 존재 여부
- totalCandidates와 items가 43개인지 여부
- `hintStatus` 허용값 여부
- pageHints의 `pageNumber`가 양의 정수인지 여부
- KOR-001~KOR-005 source별 page hint Markdown 존재 여부
- manual confirmation 결과가 자동으로 confirmed/rejected로 바뀌지 않았는지 여부
- policy knowledge card, sample JSON, fixture, JSON schema, API 파일 생성 여부

page hint는 원본 PDF 수동 검토를 돕는 보조 정보이며 confirmed/rejected 판단을 대체하지 않는다.

## Manual Review Pack 검증

`harness.checks.check_manual_review_pack`은 다음 항목을 검증한다.

- `data/sources/review/manual_review_pack/` 폴더 존재 여부
- KOR-001~KOR-005 source별 review pack Markdown 존재 여부
- `docs/manual_review/MANUAL_REVIEW_EXECUTION_PLAN.md` 존재 여부
- `docs/manual_review/MANUAL_CONFIRMATION_INPUT_GUIDE.md` 존재 여부
- manual confirmation 결과가 여전히 pending 상태인지 여부
- policy knowledge card, sample JSON, fixture, JSON schema, API 파일 생성 여부

manual review pack은 사람이 원본 PDF 대조를 쉽게 하기 위한 보조 문서이며 자동 confirmed/rejected 판단을 수행하지 않는다.

## Research Source 검증

`harness.checks.check_research_sources`는 다음 항목을 검증한다.

- registry에 RSR-001~RSR-006이 모두 포함되어 있는지 여부
- RSR source의 `sourceType`, `accessType`, `usagePurpose` 존재 여부
- RSR source의 `url` 존재 여부
- RSR source의 `status`가 `to_review`인지 여부
- `filePath`가 있는 RSR source의 실제 파일 존재 여부
- URL-only HTML source의 빈 `filePath` 허용 여부
- RSR-001 PDF 저장 여부
- KOR-001~KOR-005가 유지되어 있는지 여부

RSR-001 PDF가 없으면 warning으로 처리한다. HTML source는 원문 전체를 저장하지 않고 URL-only source로 관리한다.

## Research Review 검증

`harness.checks.check_research_review`는 다음 항목을 검증한다.

- `data/sources/review/research_review_status.json` 존재 여부
- RSR-001~RSR-006이 모두 포함되어 있는지 여부
- RSR-001 checklist와 processed Markdown 존재 여부
- RSR-002~RSR-006 checklist 존재 여부
- RSR-002~RSR-006이 `url_only`로 표시되어 있는지 여부
- URL-only source가 processed Markdown이 없어도 통과되는지 여부
- RSR-001 `extractionStatus` 허용값 여부
- 모든 `reviewStatus`가 허용값인지 여부
- policy knowledge card 파일이 생성되었거나 내용이 있는지 여부

현재 단계에서 RSR-002~RSR-006은 URL-only source이므로 processed Markdown이 없어야 한다.

## 다음 단계로 넘어가기 전에 통과해야 하는 이유

Source Registry가 통과해야만 원본 문서가 실제로 등록되어 있고, 이후 문서 검토·가공·정책 카드 작성이 잘못된 출처나 누락된 파일 위에서 진행되지 않음을 보장할 수 있다.
## Policy Card Generation 검증

`harness.checks.check_policy_cards`는 `policy_knowledge_cards.jsonl`이 존재하는 경우 JSONL 파싱, `cardId` 중복, registry에 없는 `sourceIds`, 필수 field 누락, caution 문구, confirmed 후보 기반 생성 여부를 검증한다.

confirmed 후보가 0개이고 `policy_knowledge_cards.jsonl`이 없으면 PASS다. confirmed 후보가 1개 이상인데 JSONL이 없으면 WARNING으로 처리한다.
## Policy Card 검증

`harness.checks.check_policy_cards`는 confirmed 후보 수와 `policy_knowledge_cards.jsonl`의 card 수가 일치하는지 검증한다. 현재 confirmed 후보가 9개이면 card도 9개여야 한다.

검증 항목:

- JSONL 파싱 가능 여부
- `cardId` 중복 여부
- `sourceIds`가 registry에 존재하는지 여부
- required field 누락 여부
- `evidenceText`와 `evidenceLocation` 존재 여부
- pending/rejected 후보가 card로 생성되지 않았는지 여부
- caution 문구 포함 여부
- 과장된 인증/준수 보장 표현이 없는지 여부

## Policy Mapping Docs 검증

`harness.checks.check_policy_mapping_docs`는 policy card 9개를 기반으로 작성한 매핑 문서와 coverage report를 검증한다.

검증 항목:

- `policy_knowledge_cards.jsonl` 존재 여부와 card 수 9개 여부
- `docs/policy/POLICY_CARD_COVERAGE.md` 존재 여부
- `docs/policy/POLICY_PARAMETER_CATALOG.md` 존재 여부
- `docs/policy/DECISION_ACTION_MAPPING.md` 존재 여부
- `docs/policy/DECISION_REQUEST_FIELD_MAPPING.md` 존재 여부
- `data/rag/policy_card_coverage_report.json` 존재 여부
- `data/rag/policy_card_coverage_report.md` 존재 여부
- 각 문서가 `CARD-KOR-003` cardId를 참조하는지 여부
- sample JSON, fixture, JSON schema, API가 생성되지 않았는지 여부

## JSON Schema 검증

`harness.checks.check_json_schemas`는 UE5와 AI 서버가 주고받을 JSON 계약의 schema와 Pydantic 모델을 검증한다.

검증 항목:

- `schemas/*.schema.json` 6개 존재 여부
- 각 schema JSON 파싱 가능 여부
- 각 schema의 `schemaVersion` 정의 여부
- 각 schema의 `required` 필드 존재 여부
- Pydantic 모델 파일 존재 여부
- policy card 9개 유지 여부
- sample JSON, fixture, API가 생성되지 않았는지 여부

## Contract Validation 검증

`harness.checks.check_contract_validation`는 공통 validation layer와 CLI 구성이 존재하는지 검증한다.

검증 항목:

- `app/core/contract_types.py` 존재 여부
- `app/services/json_contract_validator.py` 존재 여부
- `scripts/validate_contract.py` 존재 여부
- `ContractType` 6개 계약 타입 존재 여부
- 각 contract type의 schema/model mapping 존재 여부
- policy card 9개 유지 여부
- sample JSON, fixture, API가 생성되지 않았는지 여부

## Natural Language Plan 검증

`harness.checks.check_natural_language_plan`은 자연어 World Config 생성 설계 문서가 준비되었는지 검증한다.

검증 항목:

- `docs/json_contracts/NATURAL_LANGUAGE_INPUT_PLAN.md` 존재 여부
- `docs/architecture/LLM_WORLD_CONFIG_GENERATION_FLOW.md` 존재 여부
- `docs/architecture/WORLD_CONFIG_PROMPT_SPEC.md` 존재 여부
- `docs/json_contracts/NATURAL_LANGUAGE_GENERATION_CONTRACT.md` 존재 여부
- 각 문서가 World Config 또는 `world_config`를 언급하는지 여부
- API, sample JSON, fixture가 생성되지 않았는지 여부
- policy card 9개 유지 여부

## RAG Chunk 검증

`harness.checks.check_rag_chunks`는 confirmed policy card 기반 RAG chunk 산출물을 검증한다.

검증 항목:

- `policy_knowledge_cards.jsonl` 존재 여부
- `policy_rag_chunks.jsonl` 존재 여부
- policy card 수와 chunk 수 일치 여부
- `chunkId` 중복 여부
- `chunkText` 비어 있음 여부
- metadata 필수 필드 존재 여부
- metadata status가 `confirmed_policy_card`인지 여부
- source PDF/processed Markdown chunk 산출물, vector DB, embedding index가 생성되지 않았는지 여부

## World Config Prompt Hardening Check

Run the full harness with:

```bash
uv run python -m harness.checks.check_all
```

The prompt hardening check verifies that the schema summary builder exists, the prompt builder includes required field and extra key guidance, policy cards and RAG chunks remain unchanged, and no OpenAI call, sample JSON, fixture, vector DB, or embedding index artifact was introduced.
# EpisodeSpec adapter check

EpisodeSpec adapter 검증은 다음 명령에 포함된다.

```powershell
uv run python -m harness.checks.check_all
```

단독 확인:

```powershell
uv run python -m harness.checks.check_episode_spec_adapter
```

검증 범위는 EpisodeSpec 모델, WorldConfig 변환 adapter, EpisodeSpec validator, handoff `responseFormat`, export CLI `--format`, 금지 산출물 생성 여부이다.

EpisodeSpec handoff smoke report 검증:

```powershell
uv run python -m harness.checks.check_ue5_episode_spec_handoff_smoke
```

이 체크는 live Ollama를 호출하지 않고, 수동 smoke report의 존재와 구조만 확인한다.

EpisodeSpec scenario reflection 검증:

```powershell
uv run python -m harness.checks.check_episode_spec_scenario_reflection
```

이 체크는 reflection service, controlled smoke runner, controlled scenario smoke report 구조를 확인한다. live Ollama 호출은 하지 않는다.
## Root README Check

Run the full harness with:

```powershell
uv run python -m harness.checks.check_all
```

Run the README check directly with:

```powershell
uv run python -m harness.checks.check_root_readme
```

The root README check verifies that `README.md` exists, is not empty, includes the project overview, mentions `WorldConfig`, `EpisodeSpec`, `UE handoff`, `responseFormat=episode_spec`, scenario generation with required `prompt` and optional `episode_count`, and includes the core harness/pytest commands. It also checks that forbidden sample, fixture, vector DB, embedding index, and UE code artifacts were not introduced.

## Handoff Release Readiness Check

Run the handoff release readiness check directly with:

```powershell
uv run python -m harness.checks.check_handoff_release_readiness
```

## Report Serialization Check

`harness.checks.check_report_serialization` verifies that smoke report writers use the common serialization helper and that report tooling does not create sample JSON, fixture, vector DB, embedding index, or UE code artifacts.

Run:

```powershell
uv run python -m harness.checks.check_report_serialization
```

## Handoff Response Summary

`harness.checks.check_handoff_response_summary` verifies that generic obstacle handoff smoke reporting uses a summary helper instead of storing full `worldConfig` or full `episodeSpec` payloads.

Run:

```powershell
uv run python -m harness.checks.check_handoff_response_summary
```

This check reads local source and documentation only. It does not call OpenAI, Ollama, or a live API server.

This check verifies final delivery documents, README links, `responseFormat=episode_spec`, `obstacle.kickboard` confirmation requests, and the absence of live provider calls from harness checks, sample files, fixtures, vector DB, embedding index, and UE code artifacts.

## Environment Parameter Spec Check

Run the environment parameter spec check directly with:

```powershell
uv run python -m harness.checks.check_environment_parameter_spec
```

This check verifies `docs/environment/ENVIRONMENT_PARAMETER_SPEC.md`, required numeric environment parameters, low/middle/high exclusion as JSON values, EpisodeSpec unit conversion documentation, and the absence of sample, fixture, vector DB, embedding index, and UE code artifacts.
