# Decision Action Mapping

## 1. 목적

AI가 반환할 selectedAction 후보를 policy card 근거와 연결한다.

## 2. Action 목록

| actionName | meaning | triggeringSituation | relatedPolicyParams | relatedCardIds | ue5ExecutionHint | status |
| --- | --- | --- | --- | --- | --- | --- |
| Continue | 기존 주행 유지 | CrosswalkApproach 조건 해소 | lowSpeedZoneSpeedKmh | CARD-KOR-003-sidewalk_operation-001 | 기존 경로와 속도 유지 | ready |
| SlowDown | 속도 제어 | PedestrianAhead; ObstacleAhead; TerrainRisk | maxSpeedKmh; lowSpeedZoneSpeedKmh; safeDistanceCm | CARD-KOR-003-speed_policy-001; CARD-KOR-003-speed_policy-002; CARD-KOR-003-perception_requirement-001 | targetSpeedKmh로 감속 | ready |
| Stop | 현재 위치 정지 | ObstacleAhead; CrosswalkApproach; TerrainRisk | emergencyStopDistanceCm; ttcThresholdSec; safeDistanceCm | CARD-KOR-003-emergency_stop-001; CARD-KOR-003-sidewalk_operation-001; CARD-KOR-003-terrain_or_dynamic_safety-002 | 현재 위치 정지 | ready |
| EmergencyStop | 최우선 제동 | ObstacleAhead; CommunicationIssue | emergencyStopDistanceCm; ttcThresholdSec | CARD-KOR-003-emergency_stop-001; CARD-KOR-003-emergency_stop-002 | 최우선 제동 | ready |
| LocalAvoidance | 근거리 회피 | PedestrianAhead; ObstacleAhead; ApproachingObject | safeDistanceCm; perceptionMinRangeM | CARD-KOR-003-perception_requirement-001 | DWA 또는 간소화 회피 | partial |
| ReplanPath | 경로 재탐색 | TerrainRisk; ObstacleAhead; CrowdedPath | traversabilityThreshold; rollPitchThresholdDeg; maxRerouteAttempts | CARD-KOR-003-terrain_or_dynamic_safety-001; CARD-KOR-003-terrain_or_dynamic_safety-002; CARD-KOR-003-perception_requirement-001 | A* 또는 NavMesh 기반 경로 재탐색 | ready |
| YieldWait | 통과 대기 | PedestrianAhead; CrosswalkApproach | waitTimeoutSec; lowSpeedZoneSpeedKmh | CARD-KOR-003-sidewalk_operation-001 | waitTimeoutSec 범위 내 대기 | partial |
| RequestOperator | 관제 요청 | CommunicationIssue; CrowdedPath; TerrainRisk | waitTimeoutSec; maxRerouteAttempts; operatorOverrideEnabled | CARD-KOR-003-operator_control-001; CARD-KOR-003-emergency_stop-002; CARD-KOR-003-terrain_or_dynamic_safety-001 | 관제 요청 및 수동 개입 대기 | ready |

## 3. UE5 실행 힌트

- SlowDown: targetSpeedKmh로 감속
- Stop: 현재 위치 정지
- EmergencyStop: 최우선 제동
- LocalAvoidance: DWA 또는 간소화 회피
- ReplanPath: A* 또는 NavMesh 기반 경로 재탐색
- YieldWait: waitTimeoutSec 범위 내 대기
- RequestOperator: 관제 요청 및 수동 개입 대기
