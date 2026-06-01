# Policy Card Generation Guide

## 1. 목적

policy knowledge card는 사람이 원본 PDF와 대조해 `confirmed` 처리한 후보만 RAG 준비용 지식 단위로 변환하기 위한 산출물이다.

## 2. 생성 원칙

- `manualReviewStatus`가 `confirmed`인 후보만 card로 만든다.
- `pending_manual_confirmation` 후보는 제외한다.
- `rejected` 후보는 제외한다.
- `evidenceText`는 사람이 확인해 입력한 `confirmedText`를 사용한다.
- `evidenceLocation`은 `rawPdfPage`와 `rawPdfSection`을 조합한다.
- card는 프로젝트 내부 정책 기준이며 공식 인증 준수를 의미하지 않는다.
- sample JSON, fixture, JSON schema, API는 이 단계에서 만들지 않는다.

## 3. 입력과 출력

입력:

- `data/sources/review/confirmed/manual_confirmation_results.json`
- `data/sources/policy_source_registry.json`

출력:

- `data/rag/policy_knowledge_cards.jsonl`
- `data/rag/policy_knowledge_cards_report.json`
- `data/rag/policy_knowledge_cards_report.md`

## 4. caution 문구

모든 card는 다음 문구를 포함해야 한다.

`이 카드는 원본 문서의 수동 확인 내용을 바탕으로 만든 프로젝트 내부 정책 기준이며, 공식 인증 준수를 의미하지 않는다.`

## 5. 실행 명령

```powershell
uv run python scripts/generate_policy_cards.py
uv run python -m harness.checks.check_all
uv run pytest
```

## 6. 다음 단계

1. policy card 검증
2. RAG index 생성 준비
3. 이후 Policy Config JSON 스키마 설계
