# RAG Chunking Strategy

## 1. 목적

confirmed policy knowledge card를 RAG 검색 단위로 변환하는 전략을 정의한다.

## 2. 현재 RAG 대상

현재 RAG 대상은 원본 PDF가 아니라 `data/rag/policy_knowledge_cards.jsonl`의 confirmed policy card이다.

## 3. 기본 전략

- 1 policy card = 1 RAG chunk
- 원본 PDF와 processed Markdown은 현재 chunking 대상이 아니다.
- pending/rejected candidate는 RAG에 넣지 않는다.
- 각 chunk는 evidenceText, principle, projectRule, relatedActions, relatedPolicyParams, evidenceLocation을 포함한다.

## 4. chunk text 구성

chunkText는 아래 필드를 조합한다.

- category
- principle
- projectRule
- evidenceText
- evidenceLocation
- relatedPolicyParams
- relatedRequestFields
- relatedActions
- relatedMetrics
- caution

## 5. metadata 구성

각 chunk metadata에는 아래를 포함한다.

- chunkId
- cardId
- sourceIds
- category
- relatedPolicyParams
- relatedActions
- relatedRequestFields
- evidenceLocation
- createdFromCandidateId
- status: confirmed_policy_card

## 6. 검색 전략

- category 기반 필터링
- action 기반 필터링
- policy parameter 기반 필터링
- topK 기본값 3~5
- 검색 결과는 policy generation 또는 decision reasoning에 사용한다.

## 7. 후속 확장

- source document RAG는 별도 index로 분리한다.
- processed Markdown chunking은 reviewed 문서에 한해 section/page 단위로 수행한다.
- evaluation metric RAG는 RSR-001 검토 후 별도 card로 구성한다.
