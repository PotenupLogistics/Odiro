# PDF Vector Hybrid RAG Retrieval Strategy

## 1. 목적

새 RAG runtime은 PDF 기반 Vector Hybrid RAG다. 순수 vector topK 검색만 사용하지 않고, Chroma vector search, BM25 lexical search, metadata routing, rerank, parent context expansion, evidence snippet 생성을 함께 사용한다.

## 2. Runtime 관계

- 새 runtime source: `data/rag/pdf_corpus/validated_parent_child_chunks.jsonl`
- 새 vector cache: `.cache/rag/chroma/pdf_corpus`
- legacy card store: `data/rag/policy_rag_chunks.jsonl`

legacy card store는 보존하지만 PDF RAG source 또는 fallback으로 사용하지 않는다. Chroma index missing/stale, query embedding failure, timeout, API key 누락, model mismatch, Chroma query failure가 발생해도 legacy JSONL로 fallback하지 않는다.

루트 `.\task-setup.bat`는 Agents 의존성 설치 후 `.cache/rag/chroma/pdf_corpus` 상태를 확인하고, index가 없거나 stale이면 local Chroma index 생성을 시도한다. 이 단계는 process env 또는 `Agents/.env`에 `OPENAI_API_KEY`가 있을 때만 OpenAI embedding을 호출하며, 우선순위는 process env, `Agents/.env`, code default 순서다. key가 없으면 warning을 출력하고 setup은 계속되며, strict mode인 `ODIRO_REQUIRE_PDF_RAG_INDEX=1`에서만 setup 실패로 처리한다. `ODIRO_SKIP_PDF_RAG_INDEX=1`이면 check/build를 모두 건너뛴다. 실제 `.env`와 active Chroma index는 commit 대상이 아니다.

수동 상태 확인과 생성 명령:

```powershell
uv run python scripts/build_pdf_rag_index.py --check-only
uv run python scripts/build_pdf_rag_index.py
```

## 3. Retrieval Pipeline

```text
query expansion
-> route decision
-> metadata filter
-> Chroma vector search top 30
-> BM25 lexical search top 30
-> merge
-> rerank
-> source diversity cap
-> parent context expansion
-> evidence snippet
```

runtime query embedding은 필수 전제다. embedding on/off 설정은 두지 않는다. 기본 model은 `text-embedding-3-small`이고, env override는 `PDF_RAG_EMBEDDING_MODEL`로만 허용한다. query embedding timeout 기본값은 5초, retry는 최대 1회다.

## 4. Route 우선순위

- 안전/인증/법규: `KOR-003`, `KOR-004`, `KOR-008`
- 도로/보도/횡단: `KOR-002`, `KOR-008`, `KOR-003`, `KOR-004`
- 실험 조합/coverage: `RSR-003`, `RSR-002`
- 시나리오 생성: `RSR-002`, `RSR-004`
- reward/sim-to-real: `RSR-005`, `RSR-006`

공식 판단 route에서 RSR 문서가 top evidence로 나오면 실패로 본다.

## 5. BM25 Lexical Search

초기 구현은 외부 검색 서버 없이 validated child chunk 대상의 local BM25 scorer를 사용한다. 단순 term overlap이 아니라 document frequency 기반 IDF, term frequency saturation, document length normalization을 적용한다.

한국어는 whitespace split만 사용하지 않고 synonym 확장, 주요 복합명사, 2~3글자 char n-gram을 BM25 term으로 함께 사용한다. BM25 대상은 child chunk이며 parent chunk는 vector/BM25 embedding 대상이 아니라 context expansion 대상으로만 사용한다.

기본 synonym은 `static_obstacle_collision`, `near_miss`, `slowdown`, `crosswalk`, `blocked_region_violation`, `timeout`, `repath`를 한국어 정책/실험 용어로 확장한다.

## 6. Context Budget

- 최종 evidence item: 기본 3~5개
- route별 child hit: 최대 5개
- 동일 parent sibling child: 기본 2개
- parent full text: 기본 비활성화
- 전체 retrieval context: 4,000~6,000 tokens

우선순위는 child hit 원문, parent summary, selected sibling child, parent full text 순서다.

## 7. Public Boundary

`/api/v2/analysis/run` request/response schema는 변경하지 않는다. public response에 source id, chunk id, page, score, RAG, Chroma, vector, embedding, retrieval_backend 같은 내부 구현 정보를 추가하지 않는다.

review artifact에는 sanitized citation만 저장할 수 있으며, 필드는 `source_title`, `source_type`, `page_range`, `section_title`, `evidence_summary`로 제한한다.

## 8. Failure Policy

Vector RAG unavailable이면 internal diagnostic에 `rag_unavailable`, `rag_error_type`, `route_name`, embedding 관련 error type을 기록한다. public API는 실패시키지 않고 기존과 동일한 schema로 실행 로그 기반 분석 결과를 반환한다.
