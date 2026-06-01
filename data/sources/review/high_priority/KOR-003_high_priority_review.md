# KOR-003 High Priority Review

## 1. Source Metadata

* sourceId: KOR-003
* sourceTitle: KIRIA 실외이동로봇 운행안전인증 가이드북
* rawFilePath: data/sources/raw/korea/KOR-003_KIRIA_실외이동로봇_운행안전인증_가이드북.pdf
* candidateCount: 13
* reviewStatus: pending_manual_confirmation

## 2. 검토 대상 후보

| candidateId | category | extractedText | linkedMvpSituation | linkedMvpAction | relatedPolicyParams | rawPdfPage | rawPdfSection | confirmedText | manualReviewStatus |
| ----------- | -------- | ------------- | ------------------ | --------------- | ------------------- | ---------- | ------------- | ------------- | ------------------ |
| CAND-KOR-003-026 | sidewalk_operation | 그림 5 신호등이 없는 횡단보도 통행 시나리오 예시 · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · ·14 | PedestrianAhead | YieldWait, SlowDown, Continue | sidewalkAllowed, pedestrianPriorityRequired |  |  |  | pending_manual_confirmation |
| CAND-KOR-003-027 | sidewalk_operation | 그림 6 신호등이 있는 횡단보도 통행 시나리오 예시 · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · ·15 | PedestrianAhead | YieldWait, SlowDown, Continue | sidewalkAllowed, pedestrianPriorityRequired |  |  |  | pending_manual_confirmation |
| CAND-KOR-003-050 | terrain_or_dynamic_safety | 1 질량 및 폭 제한 질량 폭 이하500 kg, 800 mm | FallOrTilt, TerrainRisk, ObstacleAhead | ReplanPath, LocalAvoidance, Stop | maxSlopeDeg, obstacleClearanceM |  |  |  | pending_manual_confirmation |
| CAND-KOR-003-051 | sidewalk_operation | * 단 보도 폭이 최소 이상이면 까지 허용2500 mm 1200 mm | PedestrianAhead | YieldWait, SlowDown, Continue | sidewalkAllowed, pedestrianPriorityRequired |  |  |  | pending_manual_confirmation |
| CAND-KOR-003-052 | speed_policy | 2 운행 속도 질량별 최대 운행 속도 보호구역 운행 속도 준수, | PedestrianAhead | SlowDown, Stop | maxSpeedKmh, lowSpeedZoneSpeedKmh |  |  |  | pending_manual_confirmation |
| CAND-KOR-003-053 | terrain_or_dynamic_safety | 4 동적 안정성 경사로 주행 및 구조물 통과 시 안정성 확인 | FallOrTilt, TerrainRisk, ObstacleAhead | ReplanPath, LocalAvoidance, Stop | maxSlopeDeg, obstacleClearanceM |  |  |  | pending_manual_confirmation |
| CAND-KOR-003-054 | emergency_stop | 5 비상정지 비상정지 스위치 장착 및 요구사항 준수 | ObstacleAhead | EmergencyStop, Stop | emergencyStopEnabled, minStopDistanceM |  |  |  | pending_manual_confirmation |
| CAND-KOR-003-056 | speed_policy | 7 속도 제어 설정된 운행 속도 준수 | PedestrianAhead | SlowDown, Stop | maxSpeedKmh, lowSpeedZoneSpeedKmh |  |  |  | pending_manual_confirmation |
| CAND-KOR-003-057 | perception_requirement | 8 장애물 감지 관련 규격에 따른 장애물 형상 감지 및 회피 | PedestrianAhead, ObstacleAhead, ApproachingObject | SlowDown, Stop, RequestOperator | perceptionMinRangeM, pedestrianDetectionRequired |  |  |  | pending_manual_confirmation |
| CAND-KOR-003-058 | terrain_or_dynamic_safety | 11 방수 성능 로봇 외함에 대한 기본 방수 성능 만족 | FallOrTilt, TerrainRisk, ObstacleAhead | ReplanPath, LocalAvoidance, Stop | maxSlopeDeg, obstacleClearanceM |  |  |  | pending_manual_confirmation |
| CAND-KOR-003-059 | sidewalk_operation | 13 횡단보도 통행 횡단보도 통행 시 필수 요건 충족 | PedestrianAhead | YieldWait, SlowDown, Continue | sidewalkAllowed, pedestrianPriorityRequired |  |  |  | pending_manual_confirmation |
| CAND-KOR-003-060 | operator_control | 14 관제장치 모니터링 알림 등 관제장치 필수 요건 충족, | ApproachingObject | RequestOperator, Stop | operatorOverrideEnabled, maxRemoteResponseSec |  |  |  | pending_manual_confirmation |
| CAND-KOR-003-062 | emergency_stop | 16 원격조작 원격에서 로봇을 정지시키는 수단 만족 | ObstacleAhead | EmergencyStop, Stop | emergencyStopEnabled, minStopDistanceM |  |  |  | pending_manual_confirmation |

## 3. 수동 검토 메모
