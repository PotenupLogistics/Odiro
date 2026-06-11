# KOR-003 Manual Review Pack

## 1. 검토 대상 문서

* sourceId: KOR-003
* sourceTitle: KIRIA 실외이동로봇 운행안전인증 가이드북
* rawPdfPath: data/sources/raw/korea/KOR-003_KIRIA_실외이동로봇_운행안전인증_가이드북.pdf
* highPriorityCount: 13
* pageHintFoundCount: 11
* pageHintPartialCount: 2

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
| 1 | CAND-KOR-003-026 | sidewalk_operation | partial | 9 | 그림 5 신호등이 없는 횡단보도 통행 시나리오 예시 · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · ·14 | PedestrianAhead | YieldWait, SlowDown, Continue | sidewalkAllowed, pedestrianPriorityRequired |  |
| 2 | CAND-KOR-003-027 | sidewalk_operation | partial | 9 | 그림 6 신호등이 있는 횡단보도 통행 시나리오 예시 · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · ·15 | PedestrianAhead | YieldWait, SlowDown, Continue | sidewalkAllowed, pedestrianPriorityRequired |  |
| 3 | CAND-KOR-003-050 | terrain_or_dynamic_safety | found | 13 | 1 질량 및 폭 제한 질량 폭 이하500 kg, 800 mm | FallOrTilt, TerrainRisk, ObstacleAhead | ReplanPath, LocalAvoidance, Stop | maxSlopeDeg, obstacleClearanceM |  |
| 4 | CAND-KOR-003-051 | sidewalk_operation | found | 13 | * 단 보도 폭이 최소 이상이면 까지 허용2500 mm 1200 mm | PedestrianAhead | YieldWait, SlowDown, Continue | sidewalkAllowed, pedestrianPriorityRequired |  |
| 5 | CAND-KOR-003-052 | speed_policy | found | 13 | 2 운행 속도 질량별 최대 운행 속도 보호구역 운행 속도 준수, | PedestrianAhead | SlowDown, Stop | maxSpeedKmh, lowSpeedZoneSpeedKmh |  |
| 6 | CAND-KOR-003-053 | terrain_or_dynamic_safety | found | 13 | 4 동적 안정성 경사로 주행 및 구조물 통과 시 안정성 확인 | FallOrTilt, TerrainRisk, ObstacleAhead | ReplanPath, LocalAvoidance, Stop | maxSlopeDeg, obstacleClearanceM |  |
| 7 | CAND-KOR-003-054 | emergency_stop | found | 13 | 5 비상정지 비상정지 스위치 장착 및 요구사항 준수 | ObstacleAhead | EmergencyStop, Stop | emergencyStopEnabled, minStopDistanceM |  |
| 8 | CAND-KOR-003-056 | speed_policy | found | 13 | 7 속도 제어 설정된 운행 속도 준수 | PedestrianAhead | SlowDown, Stop | maxSpeedKmh, lowSpeedZoneSpeedKmh |  |
| 9 | CAND-KOR-003-057 | perception_requirement | found | 13 | 8 장애물 감지 관련 규격에 따른 장애물 형상 감지 및 회피 | PedestrianAhead, ObstacleAhead, ApproachingObject | SlowDown, Stop, RequestOperator | perceptionMinRangeM, pedestrianDetectionRequired |  |
| 10 | CAND-KOR-003-058 | terrain_or_dynamic_safety | found | 13 | 11 방수 성능 로봇 외함에 대한 기본 방수 성능 만족 | FallOrTilt, TerrainRisk, ObstacleAhead | ReplanPath, LocalAvoidance, Stop | maxSlopeDeg, obstacleClearanceM |  |
| 11 | CAND-KOR-003-059 | sidewalk_operation | found | 13 | 13 횡단보도 통행 횡단보도 통행 시 필수 요건 충족 | PedestrianAhead | YieldWait, SlowDown, Continue | sidewalkAllowed, pedestrianPriorityRequired |  |
| 12 | CAND-KOR-003-060 | operator_control | found | 13 | 14 관제장치 모니터링 알림 등 관제장치 필수 요건 충족, | ApproachingObject | RequestOperator, Stop | operatorOverrideEnabled, maxRemoteResponseSec |  |
| 13 | CAND-KOR-003-062 | emergency_stop | found | 13 | 16 원격조작 원격에서 로봇을 정지시키는 수단 만족 | ObstacleAhead | EmergencyStop, Stop | emergencyStopEnabled, minStopDistanceM |  |

주의:

* pageHints는 확정 근거가 아니라 검토 힌트이다.
* confirmed/rejected 판단은 사람이 수행한다.
* extractedText가 긴 경우 300자 이내로만 표시한다.
* 원문을 길게 복사하지 않는다.
