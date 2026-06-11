# File-Based RAG Data Store Design

## 목적

이 문서는 현재 프로젝트의 JSON/JSONL/MD 파일 기반 저장 구조를 file-based data store로 명확히 정리한다. 현재 단계에서는 PostgreSQL, SQLite, Chroma, vector DB, 별도 DB server를 도입하지 않는다.

현재 런타임 RAG DB 역할은 `data/rag/policy_rag_chunks.jsonl`이 담당한다. `app/services/policy_rag_retriever.py`는 이 JSONL 파일을 읽고 deterministic keyword/category/action/policy param/source filter scoring으로 chunk를 검색한다. 기존 API, JSONL retriever, `data/run_queue_exports` 산출물 구조는 그대로 유지한다.

## 현재 파일 기반 구조

| 경로 | 역할 | 런타임 직접 사용 여부 |
| --- | --- | --- |
| `data/sources/raw/...` | 원본 PDF 보관소 | 사용 안 함 |
| `data/sources/processed/...` | PDF 변환 Markdown/text 자료 | 사용 안 함 |
| `data/sources/review/...` | 후보 정책, 수동 확인 결과, 리뷰 상태 | 생성/검증용 |
| `data/rag/policy_knowledge_cards.jsonl` | 확정 정책 카드. runtime chunk 생성 전 단계 | 사용 안 함 |
| `data/rag/policy_rag_chunks.jsonl` | 실제 runtime RAG 검색에 사용하는 핵심 JSONL store | 사용 |
| `data/rag/*_report.json`, `data/rag/*.md` | 생성/검증 리포트 | 사용 안 함 |
| `data/run_queue_exports/...` | 생성된 EpisodeSetup, DeliveryBotSetup, RunQueue | UE 전달용 산출물 저장소 |
| `data/fine_tuning_candidates/...` | 후속 학습 후보 데이터 | 사용 안 함 |
| `test_sample/...` | 테스트/데모용 샘플 | 테스트/데모용 |

이 구조에서 파일은 단순 임시 산출물이 아니라 역할이 분리된 file-based database처럼 관리한다. 단, 모든 파일이 runtime DB인 것은 아니다. 현재 runtime active RAG store는 `policy_rag_chunks.jsonl` 하나로 제한한다.

## Runtime RAG Store

`data/rag/policy_rag_chunks.jsonl`은 현재 런타임 정책 검색의 기준 파일이다.

- 현재 chunk 수: 15개
- loader: `app/services/policy_rag_retriever.py::load_chunks`
- search entry point: `app/services/policy_rag_retriever.py::search_policy_chunks`
- 호출 경로 예: `app/services/world_config_rag_context_builder.py`, `app/services/policy_recommendation_rag_retriever.py`, `scripts/search_policy_rag.py`

Source count는 chunk 하나가 여러 `sourceIds`를 가질 수 있으므로 중복 카운트될 수 있다.

| Source | 현재 chunk count | 의미 |
| --- | ---: | --- |
| `KOR-003` | 13 | 현재 active 정책 chunk의 핵심 근거 |
| `PRJ-DOE` | 3 | 프로젝트 내부 시나리오 난이도/실험 설계 정책 |
| `PRJ-AGENT` | 2 | 프로젝트 내부 분석/정책 서버 로직 정책 |
| `PRJ-EVAL` | 1 | 프로젝트 내부 평가 기준 정책 |

## Policy Knowledge Cards

`data/rag/policy_knowledge_cards.jsonl`은 runtime retriever가 직접 읽는 파일이 아니다. 이 파일은 review를 통과한 정책 rule/card 단위의 중간 저장소이며, runtime chunk 생성의 입력으로 사용한다.

현재 상태:

- card 수: 9개
- source: 모두 `KOR-003`
- 역할: confirmed policy card lifecycle의 중간 산출물

정책 카드가 runtime 검색에 들어가려면 `policy_rag_chunks.jsonl`의 chunk로 승격되어야 한다. review 전 후보, manual confirmation 전 데이터, raw PDF 원문은 runtime chunk로 직접 승격하지 않는다.

## PDF Source Status

PDF source는 모두 동일한 비중으로 runtime RAG에 들어가지 않는다. source별 상태를 명확히 관리한다.

| 우선순위 | Source | Status | 반영 방향 |
| ---: | --- | --- | --- |
| 1 | `KOR-003` KIRIA 실외이동로봇 운행안전인증 가이드북 | `active` | 현재 runtime 정책 chunk의 핵심 근거로 유지 |
| 2 | `KOR-004` 산업부 운행안전인증 고시 | `candidate_active` | 공식 고시 근거 후보. 최신본 여부, `version_date`, `publisher` 확인 후 일부 반영 검토 |
| 3 | `RSR-001` METRANS Sidewalk ADR Interactions | `supporting_candidate` | 법적 근거가 아니라 보행자 상호작용, near-miss, 시나리오 난이도, 실험 설계 보조 근거 |
| 4 | `KOR-001` 지능형로봇법 | `reference_only` | 법적 배경/출처 근거 |
| 5 | `KOR-002` 도로교통법 실외이동로봇 관련 | `reference_only` | 보도/횡단보도/보행자 지위 관련 보조 근거 |
| 6 | `KOR-005` 도로교통법 하위법령 운행기준 참고 | `reference_only` 또는 `review_candidate` | 중복 가능성이 있으므로 보조 검토 |

## Source Inventory

Source 반영 상태는 문서 표에만 두지 않고 `data/sources/source_inventory.json`에도 기록한다. 이 파일은 runtime retriever가 직접 읽는 파일이 아니다. Source 상태, 실제 파일 경로, 내부 source 등록 여부를 validator가 기계적으로 확인하기 위한 metadata inventory다.

기본 구조:

```json
{
  "schema": "file_based_rag_source_inventory",
  "version": 1,
  "runtime_rag_store": "data/rag/policy_rag_chunks.jsonl",
  "knowledge_card_store": "data/rag/policy_knowledge_cards.jsonl",
  "sources": []
}
```

Status 의미:

| Status | 의미 |
| --- | --- |
| `active` | 현재 runtime RAG chunk의 직접 근거로 쓰는 외부 source |
| `candidate_active` | 공식성 또는 우선순위가 높지만 최신본/버전/발행자 확인 후 chunk 승격을 검토할 source |
| `supporting_candidate` | 법령/인증 근거가 아니라 실험 설계, near-miss, 보행자 상호작용 등 보조 근거로 검토할 source |
| `reference_only` | 법적 배경, 용어, 출처 확인용 참고 source이며 runtime chunk 직접 승격 대상이 아님 |
| `review_candidate` | 중복 가능성 또는 낮은 우선순위 때문에 추가 검토 후 상태를 결정할 source |
| `active_internal` | 프로젝트 내부 정책 source로, runtime chunk의 sourceIds에 쓰일 수 있음 |

현재 등록 source:

| Source | Status | Source type | 역할 | Usage |
| --- | --- | --- | --- | --- |
| `KOR-003` | `active` | `guidebook` | 현재 런타임 정책 chunk의 핵심 | `runtime_rag_chunk_source` |
| `KOR-004` | `candidate_active` | `official_notice` | 공식 고시 근거 후보 | `candidate for future policy chunk promotion` |
| `RSR-001` | `supporting_candidate` | `research_report` | 실험 설계 보조 근거 | `pedestrian interaction, near-miss, scenario difficulty support` |
| `KOR-001` | `reference_only` | `law` | 법적 배경/출처 근거 | `reference only` |
| `KOR-002` | `reference_only` | `law` | 도로교통법 관련 보조 근거 | `reference only` |
| `KOR-005` | `reference_only` | `reference` | 하위법령/운행기준 참고 | `reference or review candidate` |
| `PRJ-AGENT` | `active_internal` | `project_internal` | 결과 분석/정책 서버 로직 관련 내부 정책 | `runtime_rag_chunk_source` |
| `PRJ-DOE` | `active_internal` | `project_internal` | 시나리오 난이도/실험 설계 관련 내부 정책 | `runtime_rag_chunk_source` |
| `PRJ-EVAL` | `active_internal` | `project_internal` | 평가 기준 관련 내부 정책 | `runtime_rag_chunk_source` |

Inventory 관리 규칙:

- `source_id`, `status`, `role`, `usage`는 모든 source에 필요하다.
- 외부 source의 `raw_file_path`, `processed_file_path`는 실제 repo에 있는 경로만 기록한다.
- 파일 경로를 확인할 수 없으면 경로를 지어내지 않고 `path_status`로 `not_found` 또는 `not_verified`를 표현한다.
- 내부 PRJ source는 파일 경로 대신 `path_status: internal`로 관리할 수 있다.
- `policy_rag_chunks.jsonl`의 모든 `sourceIds`는 inventory에 등록되어 있어야 한다.
- `KOR-004`와 `RSR-001`은 현재 runtime active chunk로 강제하지 않는다. 각각 `candidate_active`, `supporting_candidate` 상태를 유지하며 검토 후보로만 관리한다.

## Runtime Source Status Guardrail

Runtime RAG chunk에 들어갈 수 있는 source status는 `active`, `active_internal` 두 가지로 제한한다.

허용:

- `active`: 현재 runtime RAG chunk의 직접 근거로 승인된 외부 source
- `active_internal`: runtime chunk에 사용할 수 있는 프로젝트 내부 source

Inventory에는 존재할 수 있지만 runtime chunk source로는 금지:

- `candidate_active`
- `supporting_candidate`
- `reference_only`
- `review_candidate`

현재 허용되는 runtime source:

| Source | Status | Runtime chunk 사용 |
| --- | --- | --- |
| `KOR-003` | `active` | 허용 |
| `PRJ-AGENT` | `active_internal` | 허용 |
| `PRJ-DOE` | `active_internal` | 허용 |
| `PRJ-EVAL` | `active_internal` | 허용 |

현재 inventory에는 있으나 runtime chunk source로 쓰면 안 되는 source:

| Source | Status | Runtime chunk 사용 |
| --- | --- | --- |
| `KOR-004` | `candidate_active` | 금지 |
| `RSR-001` | `supporting_candidate` | 금지 |
| `KOR-001` | `reference_only` | 금지 |
| `KOR-002` | `reference_only` | 금지 |
| `KOR-005` | `reference_only` 또는 `review_candidate` | 금지 |

`KOR-004` 또는 `RSR-001`을 runtime RAG chunk에 반영하려면 먼저 source inventory의 status와 review 결과를 명시적으로 업데이트해야 한다. 단순히 `policy_rag_chunks.jsonl`의 `sourceIds`에 추가하는 방식은 validator에서 실패해야 한다.

Candidate chunk 작성, 검토, 수동 승격 절차는 [Policy Chunk Candidate Promotion Workflow](POLICY_CHUNK_PROMOTION_WORKFLOW.md)를 따른다.

요약:

- `KOR-004`, `RSR-001`은 아직 runtime RAG에 반영된 상태가 아니다.
- Candidate chunk는 `data/sources/review/candidates` 아래에서 관리한다.
- `confirmed` 전에는 `policy_rag_chunks.jsonl`에 넣지 않는다.
- Runtime 승격은 source status 변경과 validator PASS가 필요하다.
- 자동 승격은 하지 않는다.

## RAG Data Flow

현재 정책 RAG 데이터는 아래 순서로 승격된다.

1. Raw PDF: `data/sources/raw/*.pdf`
2. Processed MD: `data/sources/processed/*.md`
3. Review candidates: `data/sources/review/candidates/*`
4. Manual confirmation: `data/sources/review/confirmed/manual_confirmation_results.json`
5. Policy knowledge cards: `data/rag/policy_knowledge_cards.jsonl`
6. Runtime policy chunks: `data/rag/policy_rag_chunks.jsonl`
7. Runtime retriever: `app/services/policy_rag_retriever.py`

핵심 규칙은 `review -> confirmed card -> runtime chunk` 흐름을 유지하는 것이다. PDF 원문이나 processed MD를 runtime retriever가 직접 검색하지 않는다.

## 현재 검색 방식

현재 검색은 vector DB나 Chroma 기반이 아니다.

- JSONL 파일을 직접 읽는다.
- embedding store를 사용하지 않는다.
- `data/rag/embeddings`, `data/rag/vector_db`, `data/rag/chroma`는 현재 runtime 구조에 없다.
- `chunkText` token match, category match, category/action/policy param/source filter를 이용해 deterministic score를 계산한다.
- score와 `chunkId` 정렬로 topK 결과를 결정한다.

이 방식은 corpus가 작고 정책 chunk가 사람이 검토한 카드에서 생성되는 현재 단계에 적합하다. 검색 결과가 재현 가능하고 테스트로 보호하기 쉽기 때문이다.

## File-Based Data Store 관리 규칙

1. `raw`, `processed`, `review`, `rag`, `runtime output`의 역할을 분리한다.
2. Runtime active RAG chunk는 `data/rag/policy_rag_chunks.jsonl`만 사용한다.
3. `data/rag/policy_knowledge_cards.jsonl`은 confirmed card 저장소이지 runtime retriever 입력이 아니다.
4. PDF 원문과 processed MD를 바로 runtime 검색 대상으로 넣지 않는다.
5. Review 확정 전 후보 데이터는 runtime chunk로 승격하지 않는다.
6. Runtime chunk 변경은 chunk count, required fields, source/category/filter 검색 테스트를 통과해야 한다.
7. 생성 산출물은 `data/run_queue_exports`에 유지한다.
8. UE 전달에 필요한 EpisodeSetup, DeliveryBotSetup, RunQueue JSON 파일 산출 구조는 유지한다.
9. 생성/검증 리포트, run output, local sample, API key, token, `.env`는 repository 관리 대상으로 승격하지 않는다.

## 지켜야 할 기존 동작

아래 동작은 file-based data store 설계의 호환성 기준이다.

- `POST /api/v1/scenarios/generate`는 기존 응답 계약을 유지한다.
- `app/services/policy_rag_retriever.py`는 계속 `data/rag/policy_rag_chunks.jsonl`을 읽는다.
- `scripts/search_policy_rag.py`는 기존 JSONL retriever를 사용한다.
- `data/run_queue_exports`는 UE 전달용 산출물 저장소로 유지한다.
- `data/rag/policy_rag_chunks.jsonl`은 현재 15개 chunk 기준 테스트를 통과해야 한다.
- vector DB 관련 폴더가 생기지 않아야 한다.

## Validation / Guardrails

파일 기반 RAG store가 의도치 않게 DB server/vector DB 구조로 바뀌거나 runtime 입력 파일이 깨지는 것을 막기 위해 validator를 둔다.

일반 개발자는 먼저 통합 readiness check를 실행한다.

```powershell
uv run python scripts/check_file_based_rag_readiness.py
```

Readiness check는 runtime RAG store, source inventory, runtime source status guard, candidate chunk, vector DB 디렉터리 부재를 한 번에 확인한다. 세부 원인 확인이 필요할 때 아래 개별 validator를 실행한다.

현재 repo에는 `.github/workflows`가 없으므로 GitHub Actions workflow는 추가하지 않는다. 대신 로컬 통합 검증 진입점인 `uv run python -m harness.checks.check_all`에 file-based RAG readiness check를 포함한다.

실행 명령:

```powershell
uv run python scripts/validate_file_based_rag_store.py
uv run python scripts/validate_policy_chunk_candidates.py
```

정상 출력 예:

```text
runtime chunk file: OK
chunk count: 15
knowledge card file: OK
knowledge card count: 9
vector db directories: absent
result: PASS
```

Validator가 확인하는 항목:

- `data/rag/policy_rag_chunks.jsonl` 존재 여부
- runtime chunk JSONL의 각 line이 valid JSON object인지
- runtime chunk JSONL에 빈 line이 없는지
- runtime chunk count가 현재 기준 15개인지
- chunk top-level field `chunkId`, `cardId`, `chunkText`, `metadata` 존재 여부
- chunk metadata field `sourceIds`, `category`, `relatedPolicyParams`, `relatedRequestFields`, `relatedActions`, `relatedMetrics`, `evidenceLocation`, `createdFromCandidateId`, `status` 존재 여부
- `sourceIds`, `category`, `chunkText`가 비어 있지 않은지
- `relatedActions`, `relatedPolicyParams`가 list 타입인지
- `data/rag/policy_knowledge_cards.jsonl` 존재 여부
- policy card JSONL의 각 line이 valid JSON object인지
- policy card count가 현재 기준 9개인지
- 현재 policy card가 모두 `KOR-003` 기반인지
- `data/sources/source_inventory.json` 존재 여부
- source inventory가 valid JSON object인지
- source inventory의 `schema`, `version`, `sources` 구조가 맞는지
- `sources`에 중복 `source_id`가 없는지
- 필수 source 9개가 모두 등록되어 있는지
- 각 source의 `source_id`, `status`, `role`, `usage`가 비어 있지 않은지
- source status가 허용 목록(`active`, `candidate_active`, `supporting_candidate`, `reference_only`, `review_candidate`, `active_internal`)에 들어 있는지
- `policy_rag_chunks.jsonl`의 모든 `sourceIds`가 source inventory에 등록되어 있는지
- runtime chunk의 `sourceIds`가 `active` 또는 `active_internal` status만 사용하는지
- `candidate_active`, `supporting_candidate`, `reference_only`, `review_candidate` source가 runtime chunk에 들어가지 않았는지
- `data/rag/embeddings`, `data/rag/vector_db`, `data/rag/chroma` 디렉터리가 없는지

운영 규칙:

- Runtime active RAG store는 `data/rag/policy_rag_chunks.jsonl`만 사용한다.
- `data/rag/policy_knowledge_cards.jsonl`은 중간 정책 카드 저장소이며 runtime retriever 입력이 아니다.
- Raw PDF, processed MD, review candidates는 런타임에서 직접 검색하지 않는다.
- Source 반영 상태는 `data/sources/source_inventory.json`에서 기계 검증 가능한 metadata로 관리한다.
- Runtime chunk에 사용할 수 있는 source status는 `active`, `active_internal`뿐이다.
- Candidate/reference source는 inventory에는 남기되 review와 status 변경 전에는 runtime chunk로 승격하지 않는다.
- Vector DB 디렉터리는 현재 runtime 구조에 없어야 한다.
- DB server는 현재 도입하지 않는다.
- Validator 기본 실행은 report나 generated output 파일을 만들지 않는다.

## 향후 확장 후보

현재 단계에서는 DB server와 vector DB를 도입하지 않는다. 다만 아래 조건이 생기면 별도 설계로 검토할 수 있다.

- Chunk 수가 크게 늘어나 JSONL 선형 검색이 병목이 되는 경우 SQLite 또는 PostgreSQL 검토
- Run log, analysis result, recommendation evidence가 많이 쌓여 조회/통계/추적 요구가 커지는 경우 SQLite 또는 PostgreSQL 검토
- Semantic search가 필요하고 deterministic keyword/filter scoring만으로 부족해지는 경우 `pgvector` 또는 vector DB 검토
- Source version, checksum, review status, runtime status를 대규모로 감사해야 하는 경우 별도 metadata index 검토

이 확장은 현재 구현 계획이 아니다. 현재 runtime은 no DB server, no vector DB, JSONL runtime RAG store 구조를 유지한다.

## 검증 기준

파일 기반 구조를 유지하는 변경은 최소한 아래 테스트를 통과해야 한다.

```powershell
uv run python scripts/check_file_based_rag_readiness.py
uv run python -m harness.checks.check_all
uv run pytest tests/test_policy_rag_retriever.py tests/test_rag_chunks.py tests/test_policy_rag_supplement.py tests/test_search_policy_rag_cli.py tests/test_docs_inventory.py
uv run pytest tests/test_file_based_rag_readiness.py
uv run pytest tests/test_file_based_rag_store_validator.py
uv run pytest tests/test_policy_chunk_candidate_validator.py
```
