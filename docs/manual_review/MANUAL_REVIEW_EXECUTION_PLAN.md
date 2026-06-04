# Manual Review Execution Plan

## 1. 현재 상태

* high priority candidate: 43
* page hint found: 39
* page hint partial: 4
* confirmed: 0
* rejected: 0
* pending: 43

## 2. 검토 권장 순서

1. KOR-003 KIRIA 운행안전인증 가이드북
2. KOR-004 운행안전인증 절차 및 기준 고시
3. KOR-002 도로교통법
4. KOR-001 지능형로봇법
5. KOR-005 도로교통법 제2조 하위법령

## 3. 먼저 확인할 정책 카테고리

* emergency_stop
* speed_policy
* perception_requirement
* operator_control
* sidewalk_operation
* terrain_or_dynamic_safety

## 4. confirmed 처리 기준

confirmed는 아래 조건을 모두 만족할 때만 가능하다.

* 원본 PDF에서 후보 내용 확인
* page 또는 조항 위치 확인
* confirmedText 작성
* 프로젝트 정책 파라미터 또는 행동과 연결 가능
* decisionReason 작성

## 5. rejected 처리 기준

rejected는 아래 중 하나에 해당할 때 가능하다.

* 원본 PDF에서 찾을 수 없음
* OCR/추출 오류
* 정책과 직접 관련 없음
* 중복 후보
* 문맥상 잘못 추출됨

## 6. 수동 입력 위치

* data/sources/review/confirmed/manual_confirmation_results.json
  또는
* data/sources/review/confirmed/manual_confirmation_results.md

## 7. 수동 검토 후 해야 할 일

1. manual_confirmation_results.json 수정
2. uv run python -m harness.checks.check_all 실행
3. uv run pytest 실행
4. confirmed 후보가 생기면 다음 단계에서 policy knowledge card 생성
