# KOR-002 High Priority Review

## 1. Source Metadata

* sourceId: KOR-002
* sourceTitle: 도로교통법 실외이동로봇 관련 법률
* rawFilePath: data/sources/raw/korea/KOR-002_도로교통법_실외이동로봇.pdf
* candidateCount: 19
* reviewStatus: pending_manual_confirmation

## 2. 검토 대상 후보

| candidateId | category | extractedText | linkedMvpSituation | linkedMvpAction | relatedPolicyParams | rawPdfPage | rawPdfSection | confirmedText | manualReviewStatus |
| ----------- | -------- | ------------- | ------------------ | --------------- | ------------------- | ---------- | ------------- | ------------- | ------------------ |
| CAND-KOR-002-004 | speed_policy | 3. “고속도로”란 자동차의 고속 운행에만 사용하기 위하여 지정된 도로를 말한다. | PedestrianAhead | SlowDown, Stop | maxSpeedKmh, lowSpeedZoneSpeedKmh |  |  |  | pending_manual_confirmation |
| CAND-KOR-002-005 | sidewalk_operation | 4. “차도”(車道)란 연석선(차도와 보도를 구분하는 돌 등으로 이어진 선을 말한다. 이하 같다), 안전표지 또는 그 | PedestrianAhead | YieldWait, SlowDown, Continue | sidewalkAllowed, pedestrianPriorityRequired |  |  |  | pending_manual_confirmation |
| CAND-KOR-002-010 | sidewalk_operation | 10. “보도”(步道)란 연석선, 안전표지나 그와 비슷한 인공구조물로 경계를 표시하여 보행자(유모차, 보행보조용 | PedestrianAhead | YieldWait, SlowDown, Continue | sidewalkAllowed, pedestrianPriorityRequired |  |  |  | pending_manual_confirmation |
| CAND-KOR-002-013 | sidewalk_operation | 11. “길가장자리구역”이란 보도와 차도가 구분되지 아니한 도로에서 보행자의 안전을 확보하기 위하여 안전표 | PedestrianAhead | YieldWait, SlowDown, Continue | sidewalkAllowed, pedestrianPriorityRequired |  |  |  | pending_manual_confirmation |
| CAND-KOR-002-014 | sidewalk_operation | 12. “횡단보도”란 보행자가 도로를 횡단할 수 있도록 안전표지로 표시한 도로의 부분을 말한다. | PedestrianAhead | YieldWait, SlowDown, Continue | sidewalkAllowed, pedestrianPriorityRequired |  |  |  | pending_manual_confirmation |
| CAND-KOR-002-015 | sidewalk_operation | 13. “교차로”란 ‘십’자로, ‘T’자로나 그 밖에 둘 이상의 도로(보도와 차도가 구분되어 있는 도로에서는 차도를 말 | PedestrianAhead | YieldWait, SlowDown, Continue | sidewalkAllowed, pedestrianPriorityRequired |  |  |  | pending_manual_confirmation |
| CAND-KOR-002-016 | sidewalk_operation | 13의2. “회전교차로”란 교차로 중 차마가 원형의 교통섬(차마의 안전하고 원활한 교통처리나 보행자 도로횡단 | PedestrianAhead | YieldWait, SlowDown, Continue | sidewalkAllowed, pedestrianPriorityRequired |  |  |  | pending_manual_confirmation |
| CAND-KOR-002-018 | sidewalk_operation | 14. “안전지대”란 도로를 횡단하는 보행자나 통행하는 차마의 안전을 위하여 안전표지나 이와 비슷한 인공구조 | PedestrianAhead | YieldWait, SlowDown, Continue | sidewalkAllowed, pedestrianPriorityRequired |  |  |  | pending_manual_confirmation |
| CAND-KOR-002-019 | emergency_stop | 15. “신호기”란 도로교통에서 문자ㆍ기호 또는 등화(燈火)를 사용하여 진행ㆍ정지ㆍ방향전환ㆍ주의 등의 신호 | ObstacleAhead | EmergencyStop, Stop | emergencyStopEnabled, minStopDistanceM |  |  |  | pending_manual_confirmation |
| CAND-KOR-002-023 | terrain_or_dynamic_safety | 18의2. “자율주행시스템”이란 「자율주행자동차 상용화 촉진 및 지원에 관한 법률」 제2조제1항제2호에 따른 자 | FallOrTilt, TerrainRisk, ObstacleAhead | ReplanPath, LocalAvoidance, Stop | maxSlopeDeg, obstacleClearanceM |  |  |  | pending_manual_confirmation |
| CAND-KOR-002-024 | terrain_or_dynamic_safety | 율주행시스템을 말한다. 이 경우 그 종류는 완전 자율주행시스템, 부분 자율주행시스템 등 행정안전부령으로 | FallOrTilt, TerrainRisk, ObstacleAhead | ReplanPath, LocalAvoidance, Stop | maxSlopeDeg, obstacleClearanceM |  |  |  | pending_manual_confirmation |
| CAND-KOR-002-025 | terrain_or_dynamic_safety | 18의3. “자율주행자동차”란 「자동차관리법」 제2조제1호의3에 따른 자율주행자동차로서 자율주행시스템을 갖 | FallOrTilt, TerrainRisk, ObstacleAhead | ReplanPath, LocalAvoidance, Stop | maxSlopeDeg, obstacleClearanceM |  |  |  | pending_manual_confirmation |
| CAND-KOR-002-030 | emergency_stop | 24. “주차”란 운전자가 승객을 기다리거나 화물을 싣거나 차가 고장 나거나 그 밖의 사유로 차를 계속 정지 상 | ObstacleAhead | EmergencyStop, Stop | emergencyStopEnabled, minStopDistanceM |  |  |  | pending_manual_confirmation |
| CAND-KOR-002-031 | emergency_stop | 25. “정차”란 운전자가 5분을 초과하지 아니하고 차를 정지시키는 것으로서 주차 외의 정지 상태를 말한다. | ObstacleAhead | EmergencyStop, Stop | emergencyStopEnabled, minStopDistanceM |  |  |  | pending_manual_confirmation |
| CAND-KOR-002-032 | terrain_or_dynamic_safety | 것(조종 또는 자율주행시스템을 사용하는 것을 포함한다)을 말한다. | FallOrTilt, TerrainRisk, ObstacleAhead | ReplanPath, LocalAvoidance, Stop | maxSlopeDeg, obstacleClearanceM |  |  |  | pending_manual_confirmation |
| CAND-KOR-002-033 | speed_policy | 28. “서행”(徐行)이란 운전자가 차 또는 노면전차를 즉시 정지시킬 수 있는 정도의 느린 속도로 진행하는 것을 | PedestrianAhead | SlowDown, Stop | maxSpeedKmh, lowSpeedZoneSpeedKmh |  |  |  | pending_manual_confirmation |
| CAND-KOR-002-034 | emergency_stop | 30. “일시정지”란 차 또는 노면전차의 운전자가 그 차 또는 노면전차의 바퀴를 일시적으로 완전히 정지시키는 | ObstacleAhead | EmergencyStop, Stop | emergencyStopEnabled, minStopDistanceM |  |  |  | pending_manual_confirmation |
| CAND-KOR-002-035 | sidewalk_operation | 31. “보행자전용도로”란 보행자만 다닐 수 있도록 안전표지나 그와 비슷한 인공구조물로 표시한 도로를 말한다 | PedestrianAhead | YieldWait, SlowDown, Continue | sidewalkAllowed, pedestrianPriorityRequired |  |  |  | pending_manual_confirmation |
| CAND-KOR-002-036 | sidewalk_operation | 31의2. “보행자우선도로”란 「보행안전 및 편의증진에 관한 법률」 제2조제3호에 따른 보행자우선도로를 말한다 | PedestrianAhead | YieldWait, SlowDown, Continue | sidewalkAllowed, pedestrianPriorityRequired |  |  |  | pending_manual_confirmation |

## 3. 수동 검토 메모
