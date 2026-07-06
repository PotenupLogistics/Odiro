# PDF Vector Hybrid RAG Chunking Strategy

## 1. 목적

Agents RAG runtime은 PDF 원문 기반의 parent-child corpus를 사용한다. 기존 `data/rag/policy_knowledge_cards.jsonl` 및 `data/rag/policy_rag_chunks.jsonl`은 legacy card store로 남기지만, 새 PDF RAG의 source나 fallback으로 사용하지 않는다.

## 2. 산출물 구조

```text
PDF source
-> processed page text
-> parent-child chunk candidates
-> schema / metadata / route validation
-> data/rag/pdf_corpus/validated_parent_child_chunks.jsonl
-> .cache/rag/chroma/pdf_corpus
```

`validated_parent_child_chunks.jsonl`은 사람이 전수 작성하는 파일이 아니라 자동 chunking과 validator를 통과한 runtime corpus artifact다. 사람 검수는 `chunk_build_report.json` / `chunk_build_report.md`와 low-confidence/sample chunk 확인으로 제한한다.

## 3. Chunk 계층

- Source Document: KOR-001~KOR-008, RSR-001~RSR-006 PDF.
- Parent Chunk: 조문, 인증/시험 항목, 논문 section처럼 의미가 완결되는 큰 단위.
- Child Chunk: vector search와 BM25/keyword search 대상.
- Evidence Snippet: retrieval 이후 LLM context와 review artifact에만 쓰는 짧은 내부 근거.

모든 child는 `parent_chunk_id`를 가진다. `chunk_id`는 `source_id`, `hierarchy_path`, `section_title`, normalized text hash 기반으로 생성해 같은 원문/구조에서는 재생성해도 유지한다.

## 4. 문서별 기준

- KOR 법령 (`KOR-001`, `KOR-002`, `KOR-006`, `KOR-007`, `KOR-008`): parent는 `제N조` 전체, child는 항/호/목 단위다. 조문 번호, 제목, 법령명, 시행일은 child metadata에 복사한다.
- KOR-003 KIRIA 가이드북: parent는 인증/시험 항목, child는 내부 요구사항, 표 행, 평가 조건이다.
- KOR-004 산업부 고시 PDF: parent는 별표/인증기준 평가 항목, child는 세부 기준, 시험 방법, 예외 조건이다. KOR-004는 `original_format=pdf`, `stored_format=pdf`이며 `hwpx` 또는 `converted_by=user` metadata를 쓰지 않는다.
- RSR 연구 문서: parent는 section/subsection, child는 방법론 단락, 실험 설계 단락, 알고리즘 설명이다. references, author bio, acknowledgements, citation list는 제외하거나 낮은 priority로 둔다.

RSR 문서는 법규/인증 판단 route에서 차단한다. 특히 RSR-003은 `experiment_design`, `coverage_gap`, `next_run_recommendation` 용도로만 사용한다.

## 5. Metadata

validated child chunk는 최소 다음 metadata를 가진다.

```json
{
  "chunk_id": "KOR-004-chunk-...",
  "parent_chunk_id": "KOR-004-chunk-...",
  "source_id": "KOR-004",
  "source_type": "official_notice",
  "authority_rank": 1,
  "version_status": "active",
  "page_start": 8,
  "page_end": 9,
  "section_title": "운행속도",
  "hierarchy_path": ["별표", "운행속도"],
  "topic_tags": ["speed_policy"],
  "use_scope": ["certification_requirement", "safety_rule"],
  "route_names": ["safety_certification"],
  "chunk_kind": "child",
  "language": "ko",
  "review_status": "auto_validated",
  "extraction_confidence": "high"
}
```

허용 값은 `app/services/pdf_rag_corpus.py`에서 관리한다. candidate 단계에서는 warning으로 볼 수 있지만, validated corpus 단계에서는 정의되지 않은 값이 있으면 실패한다.

## 6. Table QA

KOR-003/KOR-004는 표 기반 기준이 중요하므로 표 title, column, row value, unit이 child text에 남아야 한다. 속도, 질량, 거리, 각도, dB 등 숫자/단위가 깨지면 `extraction_confidence=low` 또는 source별 parser override 대상으로 표시한다.
