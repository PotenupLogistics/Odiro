# Manual Confirmation Input Guide

## 수정해야 하는 필드

* manualReviewStatus
* rawPdfPage
* rawPdfSection
* confirmedText
* reviewer
* reviewedAt
* decisionReason
* nextAction

## confirmed 예시

```json
{
  "manualReviewStatus": "confirmed",
  "rawPdfPage": "p.12",
  "rawPdfSection": "제X조 또는 표 제목",
  "confirmedText": "원본 PDF에서 확인한 짧은 문장",
  "reviewer": "검토자 이름",
  "reviewedAt": "YYYY-MM-DD",
  "decisionReason": "정책 파라미터 또는 액션과 연결 가능하기 때문",
  "nextAction": "create_policy_card"
}
```

## rejected 예시

```json
{
  "manualReviewStatus": "rejected",
  "decisionReason": "원본 PDF에서 확인되지 않거나 정책과 직접 관련이 약함",
  "nextAction": "exclude_from_policy_card"
}
```

## pending 상태 유지 예시

```json
{
  "manualReviewStatus": "pending_manual_confirmation",
  "rawPdfPage": "",
  "rawPdfSection": "",
  "confirmedText": "",
  "reviewer": "",
  "reviewedAt": "",
  "decisionReason": "",
  "nextAction": "needs_pdf_check"
}
```

예시는 가상의 형식 예시이며 실제 후보 상태를 변경하지 않는다.
