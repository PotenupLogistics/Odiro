# Agents RAG Data Store Design

## 목적

Agents RAG 데이터 저장 구조를 PDF Vector Hybrid RAG 기준으로 정리한다. 새 runtime은 PDF 원문에서 생성한 parent-child corpus와 local Chroma cache를 사용한다. 기존 card 기반 JSONL은 legacy로 보존하지만 새 PDF RAG의 source 또는 fallback으로 사용하지 않는다.

## 현재 구조

| 경로 | 역할 | 런타임 사용 |
| --- | --- | --- |
| `data/sources/raw/...` | KOR-001~KOR-008, RSR-001~RSR-006 PDF 원문 | build 입력 |
| `data/sources/processed/...` | PDF page text/markdown | build 입력 |
| `data/sources/source_inventory.json` | PDF source metadata, hash, version status | build/validation |
| `data/rag/pdf_corpus/chunk_candidates.jsonl` | 자동 생성 candidate chunks | build 산출물 |
| `data/rag/pdf_corpus/validated_parent_child_chunks.jsonl` | validated parent-child corpus | runtime corpus |
| `data/rag/pdf_corpus/chunk_build_report.*` | 검수용 build report | review |
| `.cache/rag/chroma/pdf_corpus` | local vector cache와 manifest | runtime cache |
| `data/rag/policy_knowledge_cards.jsonl` | legacy card store | legacy only |
| `data/rag/policy_rag_chunks.jsonl` | legacy card chunk store | legacy only |

`data/rag/chroma`, `data/rag/embeddings`, `data/rag/vector_db`는 만들지 않는다. Vector cache는 `.cache/rag/chroma/pdf_corpus` 아래에만 둔다.

## Source Inventory

`data/sources/source_inventory.json`은 `pdf_vector_hybrid_rag_source_inventory` schema를 사용한다. 모든 KOR/RSR source는 아래 필드를 가진다.

- `source_id`
- `source_type`
- `authority_rank`
- `version_status`
- `raw_file_path`
- `processed_file_path`
- `source_hash`
- `original_format`
- `stored_format`
- `effective_date`

KOR-004는 PDF 기준이다.

```json
{
  "source_id": "KOR-004",
  "source_type": "official_notice",
  "authority_rank": 1,
  "original_format": "pdf",
  "stored_format": "pdf"
}
```

KOR-004에 `hwpx` 또는 `converted_by=user` metadata를 넣지 않는다.

## Build Flow

```text
PDF source
-> processed page text
-> parent-child chunk candidates
-> schema / metadata / route validation
-> validated_parent_child_chunks.jsonl
-> tmp Chroma index build
-> manifest/hash/count validation
-> retrieval smoke/golden test
-> active Chroma index promotion
```

명시 command로만 build한다.

```powershell
uv run python scripts/build_pdf_rag_corpus.py
uv run python scripts/validate_pdf_rag_corpus.py
uv run python scripts/build_pdf_rag_index.py
```

OpenAI embedding이 필요한 index build는 `OPENAI_API_KEY`가 설정된 opt-in 작업이다. unit test는 fake embedding client를 사용한다.

## Runtime Flow

새 runtime facade는 `app/services/pdf_rag_retriever.py`다. result-analysis, world-config, policy recommendation context builder는 이 facade를 호출한다.

Retrieval은 다음을 결합한다.

- query expansion
- route decision
- metadata filter
- Chroma vector search
- BM25 lexical search
- merge/rerank
- source diversity cap
- parent context expansion
- evidence snippet

legacy `app/services/policy_rag_retriever.py`는 남아 있지만 새 PDF RAG runtime 경로에서는 호출하지 않는다.

## Failure Policy

다음 상태에서는 legacy card JSONL로 fallback하지 않는다.

- Chroma index missing
- Chroma index stale
- query embedding failure
- embedding timeout
- API key missing
- embedding model mismatch
- Chroma query failure

이 경우 internal diagnostic에 `rag_unavailable`, `rag_error_type`, `route_name`, `embedding_error_type`을 기록한다. `/api/v2/analysis/run` public response는 기존 schema를 유지하고 실행 로그 기반 분석 결과를 반환한다.

## Public Boundary

Public response에는 source id, chunk id, page, score, RAG, Chroma, vector, embedding, retrieval backend 정보를 노출하지 않는다. Review artifact에는 sanitized citation만 저장할 수 있다.

허용 citation 필드:

- `source_title`
- `source_type`
- `page_range`
- `section_title`
- `evidence_summary`

## Legacy Card Store

`policy_knowledge_cards.jsonl`과 `policy_rag_chunks.jsonl`은 과거 card 기반 RAG와 CLI 회귀 테스트를 위해 남긴다. 새 PDF RAG corpus 생성, retrieval, unavailable fallback에 이 파일들을 사용하지 않는다.

## 검증 기준

```powershell
uv run pytest tests/test_*rag* -q
uv run pytest tests/test_*analysis* -q
uv run pytest tests/test_v2_analysis_run_api.py -q
uv run python -m harness.checks.check_all
```

PowerShell에서 `test_*rag*` glob가 직접 확장되지 않으면 다음처럼 실행한다.

```powershell
$files = Get-ChildItem tests -Filter 'test_*rag*.py' | ForEach-Object { $_.FullName }
uv run pytest @files -q
```
