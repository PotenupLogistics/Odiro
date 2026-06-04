# RAG Retrieval Strategy

## 1. 목적

policy RAG chunk를 검색해 자연어 기반 World Config 생성 또는 정책 판단 근거로 사용할 수 있도록 retrieval 전략을 정의한다.

## 2. 현재 retrieval 범위

- 대상: `data/rag/policy_rag_chunks.jsonl`
- 방식: deterministic keyword/category/action/parameter filtering
- 제외: embedding, vector DB, source document chunks

## 3. 검색 시나리오

- EmergencyStop 관련 근거 검색
- speed_policy 관련 근거 검색
- obstacle/perception 관련 근거 검색
- RequestOperator 관련 근거 검색
- terrain safety 관련 근거 검색

## 4. 자연어 World Config 생성과의 연결

자연어 입력에서 추출된 키워드 또는 시나리오 유형에 따라 관련 policy chunk를 검색하고, 검색 결과를 LLM prompt context로 제공한다.

예:

- "좁은 보도", "보행자", "횡단보도" -> sidewalk_operation, speed_policy 검색
- "장애물", "킥보드", "막힘" -> perception_requirement 검색
- "정지", "위험", "충돌" -> emergency_stop 검색
- "관제", "수동 개입" -> operator_control 검색

## 5. 후속 확장

- embedding 기반 검색은 retrieval 동작 검증 이후 별도 단계에서 진행한다.
- source document RAG는 policy card RAG와 분리한다.
- vector DB는 chunk와 retrieval harness가 안정화된 뒤 추가한다.
