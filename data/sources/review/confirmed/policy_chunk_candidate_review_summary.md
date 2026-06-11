# Policy Chunk Candidate Review Summary

Review date: 2026-06-11
Reviewer: codex_source_review

## Scope

Reviewed four policy chunk candidates under `data/sources/review/candidates`.
After review, the two confirmed `KOR-004` candidates were promoted to the file-based runtime RAG store on 2026-06-11. `KOR-002` and `RSR-001` remain `needs_revision`.

## Reviewed Candidates

| Candidate | Source | Status after review | Decision |
| --- | --- | --- | --- |
| `CAND-KOR-004-SPEED-001` | `KOR-004` | `confirmed` | Promoted to `policy_knowledge_cards.jsonl` and `policy_rag_chunks.jsonl`. |
| `CAND-KOR-004-CROSSWALK-001` | `KOR-004` | `confirmed` | Promoted to `policy_knowledge_cards.jsonl` and `policy_rag_chunks.jsonl`. |
| `CAND-KOR-002-SIDEWALK-001` | `KOR-002` | `needs_revision` | Evidence exists, but action/parameter mapping is too broad for a legal definition source. |
| `CAND-RSR-001-PEDESTRIAN-001` | `RSR-001` | `needs_revision` | Evidence exists, but robot action/parameter mapping should be narrowed to experiment/near-miss support. |

## Confirmed Candidates

### CAND-KOR-004-SPEED-001

- Source status at review: `candidate_active`
- Source status after runtime promotion: `active`
- Evidence check: `sourceEvidence.evidence_text` exists in `data/sources/processed/korea/KOR-004_산업통상자원부_운행안전인증_절차_및_기준_고시.md` at processed line 82.
- Reason: The source text directly states maximum operating speed criteria by robot mass and 5 km/h compliance in child/elderly/disabled protection zones.
- Runtime promotion: Added `CARD-KOR-004-speed_policy-001` and `CHUNK-CARD-KOR-004-speed_policy-001`.
- Runtime caution: This promotion covers only the confirmed speed policy candidate, not the whole KOR-004 document.

### CAND-KOR-004-CROSSWALK-001

- Source status at review: `candidate_active`
- Source status after runtime promotion: `active`
- Evidence check: `sourceEvidence.evidence_text` exists in `data/sources/processed/korea/KOR-004_산업통상자원부_운행안전인증_절차_및_기준_고시.md` at processed line 97.
- Reason: The source text directly states crosswalk waiting, signal recognition, stop/wait behavior, no contact with pedestrians/vehicles/obstacles, and completion before pedestrian signal end.
- Runtime promotion: Added `CARD-KOR-004-crosswalk_operation-001` and `CHUNK-CARD-KOR-004-crosswalk_operation-001`.
- Runtime caution: This promotion covers only the confirmed crosswalk operation candidate, not the whole KOR-004 document.

## Needs Revision

### CAND-KOR-002-SIDEWALK-001

- Source status at review: `reference_only`
- Evidence check: The evidence exists in `data/sources/processed/korea/KOR-002_도로교통법_실외이동로봇.md` at processed lines 90-92 after preserving original line breaks.
- Reason: The legal definition supports that outdoor mobile robots are included in the pedestrian scope for sidewalk definition. It does not directly prescribe `SlowDown`, `YieldWait`, or `safeDistanceCm`.
- Revision guidance: Rewrite as a reference/supporting definition candidate, or attach it as supplemental evidence to an active/candidate-active source chunk.

### CAND-RSR-001-PEDESTRIAN-001

- Source status at review: `supporting_candidate`
- Evidence check: The evidence exists in `data/sources/processed/research/RSR-001_METRANS_Sidewalk_ADR_Interactions.md` at processed lines 238-244 after preserving original line breaks.
- Reason: The research report supports pedestrian/bicyclist interaction, conflict, unsafe travel condition, and PET-based observation concepts. It does not directly prescribe robot actions such as `LocalAvoidance` or `Stop`.
- Revision guidance: Rewrite as scenario difficulty, near-miss analysis, or pedestrian interaction support rather than direct legal/policy action guidance.

## Runtime Promotion Notes

- `KOR-004` confirmed candidates were added to `data/rag/policy_knowledge_cards.jsonl` and `data/rag/policy_rag_chunks.jsonl`.
- `KOR-002` and `RSR-001` were not added to runtime RAG.
- `promotionDecision.can_promote_to_runtime=true` remains advisory for future candidates and does not perform automatic promotion.
- Runtime chunk sources must satisfy the current runtime source status guardrail before being added to the runtime RAG store.
