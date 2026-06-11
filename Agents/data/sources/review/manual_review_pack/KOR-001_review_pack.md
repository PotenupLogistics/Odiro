# KOR-001 Manual Review Pack

## 1. 검토 대상 문서

* sourceId: KOR-001
* sourceTitle: 지능형 로봇 개발 및 보급 촉진법
* rawPdfPath: data/sources/raw/korea/KOR-001_지능형로봇법.pdf
* highPriorityCount: 1
* pageHintFoundCount: 1
* pageHintPartialCount: 0

## 2. 검토 방법

1. rawPdfPath의 원본 PDF를 연다.
2. 아래 후보의 page hint 페이지를 확인한다.
3. extractedText가 원본 PDF에 실제로 존재하는지 확인한다.
4. 정책으로 사용할 수 있는 문장인지 판단한다.
5. manual_confirmation_results.json에 사람이 직접 다음 필드를 입력한다.

   * manualReviewStatus: confirmed 또는 rejected
   * rawPdfPage
   * rawPdfSection
   * confirmedText
   * reviewer
   * reviewedAt
   * decisionReason
   * nextAction

## 3. 후보 목록

| No | candidateId | category | hintStatus | pageHints | extractedText | linkedMvpSituation | linkedMvpAction | relatedPolicyParams | 검토 메모 |
| -- | ----------- | -------- | ---------- | --------- | ------------- | ------------------ | --------------- | ------------------- | ----- |
| 1 | CAND-KOR-001-010 | emergency_stop | found | 8 | 면의 점용ㆍ사용 실시계획의 승인 등(매립면허를 받은 매립예정지는 제외한다), 같은 법 제28조에 따른 공유수면 | ObstacleAhead | EmergencyStop, Stop | emergencyStopEnabled, minStopDistanceM |  |

주의:

* pageHints는 확정 근거가 아니라 검토 힌트이다.
* confirmed/rejected 판단은 사람이 수행한다.
* extractedText가 긴 경우 300자 이내로만 표시한다.
* 원문을 길게 복사하지 않는다.
