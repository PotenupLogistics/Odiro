# KOR-001 수동 검토 체크리스트

## 1. Source Metadata

* sourceId: KOR-001
* title: 지능형 로봇 개발 및 보급 촉진법
* rawFilePath: data/sources/raw/korea/KOR-001_지능형로봇법.pdf
* processedFilePath: data/sources/processed/korea/KOR-001_지능형로봇법.md
* currentExtractionStatus: partial
* reviewStatus: not_started

## 2. 원본 PDF 대조 필요 항목

* [ ] 문서 제목이 정확한지 확인
* [ ] 발행기관 확인
* [ ] 발행일 또는 시행일 확인
* [ ] 목차/조항 구조가 processed Markdown에 누락되지 않았는지 확인
* [ ] 표가 누락되거나 깨지지 않았는지 확인
* [ ] 이미지/도표에 중요한 정책 기준이 있는지 확인
* [ ] 페이지 번호 또는 조항 번호가 필요한 항목 확인
* [ ] 정책으로 추출 가능한 문장 후보 표시
* [ ] 최종 reviewed 처리 가능 여부 판단

## 3. 정책 추출 후보 영역

| 카테고리 | 원본 위치/페이지 | 확인한 내용 | 연결 가능 정책 | 검토 상태 |
| --- | --- | --- | --- | --- |
| speed_policy |  |  |  | not_checked |
| emergency_stop |  |  |  | not_checked |
| perception_requirement |  |  |  | not_checked |
| operator_control |  |  |  | not_checked |
| sidewalk_operation |  |  |  | not_checked |
| crosswalk_operation |  |  |  | not_checked |
| terrain_or_dynamic_safety |  |  |  | not_checked |
| data_recording |  |  |  | not_checked |

## 4. 정책 카드 생성 전 확인사항

* [ ] sourceId가 registry와 일치한다
* [ ] 원본 PDF와 processed Markdown을 대조했다
* [ ] 추출한 내용이 원문에서 확인 가능하다
* [ ] 정책 카드로 만들 문장에는 sourceId와 페이지/조항 근거가 있다
* [ ] 공식 인증 준수 표현을 사용하지 않는다
* [ ] 프로젝트 내부 정책 기준으로만 사용한다

## 5. 수동 검토 메모
## 6. 자동 추출된 정책 후보 파일

* candidateFilePath: data/sources/review/candidates/korea/KOR-001_policy_candidates.md
* candidateStatus: needs_pdf_check
* 주의: 이 후보는 원본 PDF와 대조 전이며, policy knowledge card로 확정된 것이 아니다.
## 7. Triage Summary

* triageFilePath: data/sources/review/triage/policy_candidate_triage.json
* highPriorityCount: 1
* mediumPriorityCount: 48
* lowPriorityCount: 0
* nextReviewAction: 원본 PDF 대조 필요
