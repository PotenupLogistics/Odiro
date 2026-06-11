# Policy Chunk Candidate Promotion Workflow

## 목적

이 문서는 `KOR-004`, `RSR-001` 같은 candidate/supporting source를 나중에 runtime RAG chunk로 승격할 때 따라야 하는 수동 검토 절차를 정의한다. 현재 단계에서는 어떤 candidate도 `data/rag/policy_rag_chunks.jsonl`에 자동 추가하지 않는다.

현재 runtime RAG store는 계속 `data/rag/policy_rag_chunks.jsonl` 하나이며, `app/services/policy_rag_retriever.py`는 기존 JSONL retriever 동작을 유지한다.

## 기본 원칙

- Candidate chunk는 runtime RAG가 아니다.
- Candidate chunk는 `data/sources/review/candidates` 아래에서 검토 자료로 관리한다.
- `confirmed` 전에는 `data/rag/policy_rag_chunks.jsonl`에 넣지 않는다.
- `can_promote_to_runtime=true`는 자동 승격 명령이 아니다. 사람이 검토 후 별도 변경으로 반영해야 한다.
- Runtime chunk로 승격하려면 source status가 `active` 또는 `active_internal`이어야 한다.
- `KOR-004`, `RSR-001`은 현재 runtime RAG에 반영된 상태가 아니다.

## Workflow

1. Source를 `data/sources/source_inventory.json`에 등록한다.
2. Source status를 `candidate_active`, `supporting_candidate`, `reference_only`, `review_candidate` 중 하나로 관리한다.
3. Raw PDF, processed MD, review candidate를 근거로 candidate chunk를 작성한다.
4. Candidate chunk는 runtime RAG가 아닌 `data/sources/review/candidates` 아래에서 관리한다.
5. 사람이 근거 문장, category, relatedActions, relatedPolicyParams를 확인한다.
6. Review 결과가 `confirmed`가 되기 전까지는 `data/rag/policy_rag_chunks.jsonl`에 넣지 않는다.
7. Confirmed 후 source status와 chunk 승격 여부를 명시적으로 갱신한다.
8. Runtime chunk로 수동 승격한 뒤 `uv run python scripts/check_file_based_rag_readiness.py`를 실행한다.
9. Validator가 PASS 되어야 runtime 반영 완료로 인정한다.

## Candidate Template

작성 양식:

```text
data/sources/review/templates/policy_chunk_candidate_template.json
```

템플릿은 실제 runtime 데이터가 아니다. 실제 정책 문장을 지어내지 말고, raw/processed/review 자료에서 사람이 확인한 근거만 candidate 파일에 작성한다.

주요 필드:

- `schema`: `policy_chunk_candidate`
- `version`
- `source_id`
- `source_status_at_review`
- `candidate_id`
- `review_status`: `draft`, `candidate`, `needs_revision`, `confirmed`, `rejected`
- `category`
- `chunkText`
- `relatedActions`
- `relatedPolicyParams`
- `sourceEvidence`
- `reviewNotes`
- `promotionDecision`

## Candidate Validation

Candidate validator:

```powershell
uv run python scripts/validate_policy_chunk_candidates.py
```

일반적인 승격 전 최종 확인은 통합 readiness check를 먼저 실행한다.

```powershell
uv run python scripts/check_file_based_rag_readiness.py
```

로컬 통합 하네스도 readiness check를 포함한다.

```powershell
uv run python -m harness.checks.check_all
```

검증 대상:

- `data/sources/review/candidates` 아래 JSON 파일 중 `schema`가 `policy_chunk_candidate`인 파일
- `data/sources/review/templates` 아래 템플릿 파일은 검증 대상에서 제외
- 기존 candidate index/report JSON은 `policy_chunk_candidate` schema가 아니면 제외

검증 항목:

- Candidate file이 valid JSON object인지
- 필수 필드가 있는지
- `source_id`가 `data/sources/source_inventory.json`에 등록되어 있는지
- Candidate source status가 `candidate_active`, `supporting_candidate`, `reference_only`, `review_candidate` 중 하나인지
- `source_status_at_review`가 inventory의 현재 status와 일치하는지
- `review_status`가 허용 목록에 있는지
- `relatedActions`, `relatedPolicyParams`가 list 타입인지
- `chunkText`가 비어 있지 않은지
- `sourceEvidence`가 있는지
- `confirmed`인데 `promotionDecision.can_promote_to_runtime=false`이면 실패

`promotionDecision.can_promote_to_runtime=true`인 경우 validator는 경고만 출력한다. 이 값은 수동 승격 판단을 돕는 metadata이며, validator가 `policy_rag_chunks.jsonl`을 수정하지 않는다.

## Runtime Promotion Checklist

Runtime 승격 전에 확인할 항목:

- Source inventory의 source status 변경이 필요한지 검토했다.
- Candidate file의 `review_status`가 `confirmed`다.
- `promotionDecision.can_promote_to_runtime`이 `true`이고 decision reason이 있다.
- `chunkText`, `category`, `relatedActions`, `relatedPolicyParams`, `sourceEvidence`를 사람이 확인했다.
- `data/rag/policy_rag_chunks.jsonl` 변경은 별도 수동 변경으로 수행한다.
- 변경 후 `uv run python scripts/check_file_based_rag_readiness.py`가 PASS 한다.
- 로컬 통합 검증에서는 `uv run python -m harness.checks.check_all`도 PASS 또는 허용된 warning 상태여야 한다.
- 필요하면 `uv run python scripts/validate_file_based_rag_store.py`와 `uv run python scripts/validate_policy_chunk_candidates.py`로 세부 실패 원인을 확인한다.
- 기존 RAG 관련 pytest가 PASS 한다.

## 현재 비승격 Source

| Source | 현재 status | 현재 runtime 반영 |
| --- | --- | --- |
| `KOR-004` | `candidate_active` | 아님 |
| `RSR-001` | `supporting_candidate` | 아님 |
| `KOR-001` | `reference_only` | 아님 |
| `KOR-002` | `reference_only` | 아님 |
| `KOR-005` | `reference_only` | 아님 |
