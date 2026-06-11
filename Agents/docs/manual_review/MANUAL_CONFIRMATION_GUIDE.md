# Manual Confirmation Guide

## 1. 목적

High priority 후보 43개를 사람이 원본 PDF와 대조하여 confirmed 또는 rejected로 표시하는 절차를 설명한다.

## 2. 입력 파일

* data/sources/review/high_priority/high_priority_review_queue.json
* data/sources/review/high_priority/high_priority_review_queue.md
* data/sources/review/high_priority/KOR-001_high_priority_review.md
* data/sources/review/high_priority/KOR-002_high_priority_review.md
* data/sources/review/high_priority/KOR-003_high_priority_review.md
* data/sources/review/high_priority/KOR-004_high_priority_review.md
* data/sources/review/high_priority/KOR-005_high_priority_review.md

## 3. 사람이 직접 채워야 하는 필드

* manualReviewStatus: pending_manual_confirmation | confirmed | rejected
* rawPdfPage
* rawPdfSection
* confirmedText
* reviewer
* reviewedAt
* decisionReason
* nextAction

## 4. confirmed 조건

candidate가 confirmed가 되려면 아래 조건을 모두 만족해야 한다.

* 원본 PDF에서 candidate 내용이 실제로 확인됨
* rawPdfPage 또는 rawPdfSection 중 하나 이상 작성됨
* confirmedText가 원문 확인 기반으로 작성됨
* decisionReason이 작성됨
* relatedPolicyParams 또는 linkedMvpAction과 연결 가능함
* 공식 인증 준수 표현이 아니라 프로젝트 내부 정책 기준으로만 사용함

## 5. rejected 조건

아래 중 하나라도 해당하면 rejected 처리한다.

* 원본 PDF에서 해당 내용을 찾을 수 없음
* OCR/추출 오류로 보임
* 정책과 직접 관련이 약함
* 중복 후보임
* 문맥상 잘못 추출된 후보임

## 6. 다음 단계

confirmed 후보만 policy knowledge card 생성 대상으로 사용한다.
rejected 후보는 카드 생성 대상에서 제외한다.

CLI 사용법은 `docs/manual_review/MANUAL_CONFIRMATION_CLI.md`를 따른다. `--yes`가 없으면 dry-run으로만 동작한다.

## 7. Page Hint 참고

page hint 파일 위치:

* data/sources/review/high_priority/page_hints/high_priority_page_hints.json
* data/sources/review/high_priority/page_hints/high_priority_page_hints.md
* data/sources/review/high_priority/page_hints/KOR-001_page_hints.md
* data/sources/review/high_priority/page_hints/KOR-002_page_hints.md
* data/sources/review/high_priority/page_hints/KOR-003_page_hints.md
* data/sources/review/high_priority/page_hints/KOR-004_page_hints.md
* data/sources/review/high_priority/page_hints/KOR-005_page_hints.md

page hint는 확정 근거가 아니라 수동 검토 보조 정보다. confirmed 처리 시 반드시 원본 PDF를 직접 열어 확인해야 한다.

page hint가 `not_found`여도 후보가 틀렸다는 의미는 아니다. 표, 이미지, OCR, PDF 텍스트 추출 문제일 수 있으므로 사람이 원본을 직접 확인한다.
