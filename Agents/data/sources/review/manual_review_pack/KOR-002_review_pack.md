# KOR-002 Manual Review Pack

## 1. 검토 대상 문서

* sourceId: KOR-002
* sourceTitle: 도로교통법 실외이동로봇 관련 법률
* rawPdfPath: data/sources/raw/korea/KOR-002_도로교통법_실외이동로봇.pdf
* highPriorityCount: 19
* pageHintFoundCount: 19
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
| 1 | CAND-KOR-002-004 | speed_policy | found | 1 | 3. “고속도로”란 자동차의 고속 운행에만 사용하기 위하여 지정된 도로를 말한다. | PedestrianAhead | SlowDown, Stop | maxSpeedKmh, lowSpeedZoneSpeedKmh |  |
| 2 | CAND-KOR-002-005 | sidewalk_operation | found | 1 | 4. “차도”(車道)란 연석선(차도와 보도를 구분하는 돌 등으로 이어진 선을 말한다. 이하 같다), 안전표지 또는 그 | PedestrianAhead | YieldWait, SlowDown, Continue | sidewalkAllowed, pedestrianPriorityRequired |  |
| 3 | CAND-KOR-002-010 | sidewalk_operation | found | 1 | 10. “보도”(步道)란 연석선, 안전표지나 그와 비슷한 인공구조물로 경계를 표시하여 보행자(유모차, 보행보조용 | PedestrianAhead | YieldWait, SlowDown, Continue | sidewalkAllowed, pedestrianPriorityRequired |  |
| 4 | CAND-KOR-002-013 | sidewalk_operation | found | 1 | 11. “길가장자리구역”이란 보도와 차도가 구분되지 아니한 도로에서 보행자의 안전을 확보하기 위하여 안전표 | PedestrianAhead | YieldWait, SlowDown, Continue | sidewalkAllowed, pedestrianPriorityRequired |  |
| 5 | CAND-KOR-002-014 | sidewalk_operation | found | 2 | 12. “횡단보도”란 보행자가 도로를 횡단할 수 있도록 안전표지로 표시한 도로의 부분을 말한다. | PedestrianAhead | YieldWait, SlowDown, Continue | sidewalkAllowed, pedestrianPriorityRequired |  |
| 6 | CAND-KOR-002-015 | sidewalk_operation | found | 2 | 13. “교차로”란 ‘십’자로, ‘T’자로나 그 밖에 둘 이상의 도로(보도와 차도가 구분되어 있는 도로에서는 차도를 말 | PedestrianAhead | YieldWait, SlowDown, Continue | sidewalkAllowed, pedestrianPriorityRequired |  |
| 7 | CAND-KOR-002-016 | sidewalk_operation | found | 2 | 13의2. “회전교차로”란 교차로 중 차마가 원형의 교통섬(차마의 안전하고 원활한 교통처리나 보행자 도로횡단 | PedestrianAhead | YieldWait, SlowDown, Continue | sidewalkAllowed, pedestrianPriorityRequired |  |
| 8 | CAND-KOR-002-018 | sidewalk_operation | found | 2 | 14. “안전지대”란 도로를 횡단하는 보행자나 통행하는 차마의 안전을 위하여 안전표지나 이와 비슷한 인공구조 | PedestrianAhead | YieldWait, SlowDown, Continue | sidewalkAllowed, pedestrianPriorityRequired |  |
| 9 | CAND-KOR-002-019 | emergency_stop | found | 2 | 15. “신호기”란 도로교통에서 문자ㆍ기호 또는 등화(燈火)를 사용하여 진행ㆍ정지ㆍ방향전환ㆍ주의 등의 신호 | ObstacleAhead | EmergencyStop, Stop | emergencyStopEnabled, minStopDistanceM |  |
| 10 | CAND-KOR-002-023 | terrain_or_dynamic_safety | found | 2 | 18의2. “자율주행시스템”이란 「자율주행자동차 상용화 촉진 및 지원에 관한 법률」 제2조제1항제2호에 따른 자 | FallOrTilt, TerrainRisk, ObstacleAhead | ReplanPath, LocalAvoidance, Stop | maxSlopeDeg, obstacleClearanceM |  |
| 11 | CAND-KOR-002-024 | terrain_or_dynamic_safety | found | 2 | 율주행시스템을 말한다. 이 경우 그 종류는 완전 자율주행시스템, 부분 자율주행시스템 등 행정안전부령으로 | FallOrTilt, TerrainRisk, ObstacleAhead | ReplanPath, LocalAvoidance, Stop | maxSlopeDeg, obstacleClearanceM |  |
| 12 | CAND-KOR-002-025 | terrain_or_dynamic_safety | found | 2 | 18의3. “자율주행자동차”란 「자동차관리법」 제2조제1호의3에 따른 자율주행자동차로서 자율주행시스템을 갖 | FallOrTilt, TerrainRisk, ObstacleAhead | ReplanPath, LocalAvoidance, Stop | maxSlopeDeg, obstacleClearanceM |  |
| 13 | CAND-KOR-002-030 | emergency_stop | found | 3 | 24. “주차”란 운전자가 승객을 기다리거나 화물을 싣거나 차가 고장 나거나 그 밖의 사유로 차를 계속 정지 상 | ObstacleAhead | EmergencyStop, Stop | emergencyStopEnabled, minStopDistanceM |  |
| 14 | CAND-KOR-002-031 | emergency_stop | found | 3 | 25. “정차”란 운전자가 5분을 초과하지 아니하고 차를 정지시키는 것으로서 주차 외의 정지 상태를 말한다. | ObstacleAhead | EmergencyStop, Stop | emergencyStopEnabled, minStopDistanceM |  |
| 15 | CAND-KOR-002-032 | terrain_or_dynamic_safety | found | 3 | 것(조종 또는 자율주행시스템을 사용하는 것을 포함한다)을 말한다. | FallOrTilt, TerrainRisk, ObstacleAhead | ReplanPath, LocalAvoidance, Stop | maxSlopeDeg, obstacleClearanceM |  |
| 16 | CAND-KOR-002-033 | speed_policy | found | 4 | 28. “서행”(徐行)이란 운전자가 차 또는 노면전차를 즉시 정지시킬 수 있는 정도의 느린 속도로 진행하는 것을 | PedestrianAhead | SlowDown, Stop | maxSpeedKmh, lowSpeedZoneSpeedKmh |  |
| 17 | CAND-KOR-002-034 | emergency_stop | found | 4 | 30. “일시정지”란 차 또는 노면전차의 운전자가 그 차 또는 노면전차의 바퀴를 일시적으로 완전히 정지시키는 | ObstacleAhead | EmergencyStop, Stop | emergencyStopEnabled, minStopDistanceM |  |
| 18 | CAND-KOR-002-035 | sidewalk_operation | found | 4 | 31. “보행자전용도로”란 보행자만 다닐 수 있도록 안전표지나 그와 비슷한 인공구조물로 표시한 도로를 말한다 | PedestrianAhead | YieldWait, SlowDown, Continue | sidewalkAllowed, pedestrianPriorityRequired |  |
| 19 | CAND-KOR-002-036 | sidewalk_operation | found | 4 | 31의2. “보행자우선도로”란 「보행안전 및 편의증진에 관한 법률」 제2조제3호에 따른 보행자우선도로를 말한다 | PedestrianAhead | YieldWait, SlowDown, Continue | sidewalkAllowed, pedestrianPriorityRequired |  |

주의:

* pageHints는 확정 근거가 아니라 검토 힌트이다.
* confirmed/rejected 판단은 사람이 수행한다.
* extractedText가 긴 경우 300자 이내로만 표시한다.
* 원문을 길게 복사하지 않는다.
