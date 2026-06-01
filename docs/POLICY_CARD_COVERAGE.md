# Policy Card Coverage

## 1. 목적

현재 생성된 policy knowledge card 9개가 MVP 정책의 어떤 상황, 액션, 파라미터를 커버하는지 정리한다.

## 2. 현재 카드 요약

- 총 card 수: 9
- source별 card 수: KOR-003 9개
- category별 card 수: emergency_stop 2, speed_policy 2, terrain_or_dynamic_safety 2, perception_requirement 1, sidewalk_operation 1, operator_control 1

## 3. MVP 상황 커버리지

| MVP 상황 | 연결 cardId | category | Decision Request 필드 | selectedAction | 부족한 근거 여부 |
| --- | --- | --- | --- | --- | --- |
| PedestrianAhead | CARD-KOR-003-perception_requirement-001; CARD-KOR-003-sidewalk_operation-001 | perception_requirement; sidewalk_operation | detectedObjects[]; detectedObjects[].isOnPath; event.confidence | SlowDown; Stop; LocalAvoidance; YieldWait | ready |
| ObstacleAhead | CARD-KOR-003-perception_requirement-001; CARD-KOR-003-emergency_stop-001 | perception_requirement; emergency_stop | detectedObjects[]; detectedObjects[].distanceCm; detectedObjects[].timeToCollisionSec | Stop; EmergencyStop; LocalAvoidance; ReplanPath | ready |
| FallOrTilt | CARD-KOR-003-terrain_or_dynamic_safety-001; CARD-KOR-003-terrain_or_dynamic_safety-002 | terrain_or_dynamic_safety | terrain; botState.pitchDegree; botState.rollDegree | SlowDown; Stop; ReplanPath; RequestOperator | ready |
| TerrainRisk | CARD-KOR-003-terrain_or_dynamic_safety-001; CARD-KOR-003-terrain_or_dynamic_safety-002 | terrain_or_dynamic_safety | terrain.traversabilityScore; terrain.slopeDegree; terrain.curbHeightCm | SlowDown; Stop; ReplanPath; RequestOperator | ready |
| ApproachingObject | CARD-KOR-003-perception_requirement-001 | perception_requirement | detectedObjects[]; detectedObjects[].distanceCm; detectedObjects[].timeToCollisionSec | SlowDown; Stop; LocalAvoidance | needs_more_evidence: 동물, 자전거 등 세부 접근 객체 근거는 RSR 또는 추가 KOR 검토 필요 |
| CrosswalkApproach | CARD-KOR-003-sidewalk_operation-001 | sidewalk_operation | environments[].type; environments[].state; detectedObjects[].isOnPath | Stop; YieldWait; Continue | ready |
| CommunicationIssue | CARD-KOR-003-operator_control-001; CARD-KOR-003-emergency_stop-002 | operator_control; emergency_stop | communicationStatus; event.type; botState | RequestOperator; Stop; EmergencyStop | ready for operator request, needs_more_evidence for detailed communication failure handling |
| CrowdedPath | CARD-KOR-003-operator_control-001; CARD-KOR-003-perception_requirement-001 | operator_control; perception_requirement | pathContext.pathBlocked; detectedObjects[]; communicationStatus | RequestOperator; Stop; LocalAvoidance | needs_more_evidence: 보행자 밀집 판단 기준 추가 검토 필요 |

## 4. MVP 액션 커버리지

| MVP 액션 | 연결 cardId | 근거 category | 관련 policy parameter | 현재 충분한지 여부 |
| --- | --- | --- | --- | --- |
| Continue | CARD-KOR-003-sidewalk_operation-001 | sidewalk_operation | lowSpeedZoneSpeedKmh; waitTimeoutSec | partial: 횡단보도 상태 기반 계속 주행 판단은 후속 request field 설계 필요 |
| SlowDown | CARD-KOR-003-speed_policy-001; CARD-KOR-003-speed_policy-002; CARD-KOR-003-perception_requirement-001 | speed_policy; perception_requirement | maxSpeedKmh; lowSpeedZoneSpeedKmh; safeDistanceCm | ready |
| Stop | CARD-KOR-003-emergency_stop-001; CARD-KOR-003-perception_requirement-001; CARD-KOR-003-sidewalk_operation-001 | emergency_stop; perception_requirement; sidewalk_operation | emergencyStopDistanceCm; ttcThresholdSec; safeDistanceCm | ready |
| EmergencyStop | CARD-KOR-003-emergency_stop-001; CARD-KOR-003-emergency_stop-002 | emergency_stop | emergencyStopDistanceCm; ttcThresholdSec | ready |
| LocalAvoidance | CARD-KOR-003-perception_requirement-001 | perception_requirement | safeDistanceCm; perceptionMinRangeM | partial: 회피 알고리즘 기준은 별도 설계 필요 |
| ReplanPath | CARD-KOR-003-terrain_or_dynamic_safety-001; CARD-KOR-003-terrain_or_dynamic_safety-002; CARD-KOR-003-perception_requirement-001 | terrain_or_dynamic_safety; perception_requirement | traversabilityThreshold; rollPitchThresholdDeg; safeDistanceCm | ready for trigger, implementation detail deferred |
| YieldWait | CARD-KOR-003-sidewalk_operation-001 | sidewalk_operation | waitTimeoutSec; lowSpeedZoneSpeedKmh | partial: 대기 시간 초기값은 후속 Policy Config 단계에서 결정 |
| RequestOperator | CARD-KOR-003-operator_control-001; CARD-KOR-003-emergency_stop-002; CARD-KOR-003-terrain_or_dynamic_safety-001 | operator_control; emergency_stop; terrain_or_dynamic_safety | waitTimeoutSec; maxRerouteAttempts; operatorOverrideEnabled | ready |

## 5. 부족한 영역

- CommunicationIssue는 관제장치와 원격정지 card가 연결되지만, 통신 장애의 세부 상태값은 후속 검토가 필요하다.
- ApproachingObject는 장애물 감지 card와 일부 연결되지만, 동물/자전거 등 객체 유형별 근거는 RSR 또는 추가 KOR 검토가 필요하다.
- CrowdedPath는 operator_control과 perception_requirement에 일부 연결되지만, 보행자 밀집 기준은 추가 confirmed 후보가 필요하다.
- Evaluation metric은 RSR-001 검토 후 보강이 필요하다.
