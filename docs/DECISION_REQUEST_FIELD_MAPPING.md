# Decision Request Field Mapping

## 1. 목적

UE5가 AI 서버로 보내야 하는 Decision Request 필드를 policy card와 연결한다.

## 2. 필수 request field 후보

| field | meaning | relatedCardIds | relatedPolicyParams | relatedActions | requiredForMvp | status |
| --- | --- | --- | --- | --- | --- | --- |
| event.type | 이벤트 유형 | CARD-KOR-003-operator_control-001; CARD-KOR-003-emergency_stop-002 | operatorOverrideEnabled | RequestOperator; EmergencyStop | true | ready |
| event.severity | 위험 심각도 | CARD-KOR-003-emergency_stop-001; CARD-KOR-003-emergency_stop-002 | emergencyStopDistanceCm; ttcThresholdSec | EmergencyStop; Stop | true | ready |
| event.confidence | 인지 신뢰도 | CARD-KOR-003-perception_requirement-001 | perceptionMinRangeM | SlowDown; Stop; LocalAvoidance | true | ready |
| event.source | 인지 이벤트 출처 | CARD-KOR-003-perception_requirement-001 | perceptionMinRangeM | SlowDown; Stop; LocalAvoidance | true | ready |
| botState.speedKmh | 현재 속도 | CARD-KOR-003-speed_policy-001; CARD-KOR-003-speed_policy-002; CARD-KOR-003-terrain_or_dynamic_safety-002 | maxSpeedKmh; lowSpeedZoneSpeedKmh | SlowDown; Stop | true | ready |
| botState.pitchDegree | 피치 기울기 | CARD-KOR-003-terrain_or_dynamic_safety-002 | rollPitchThresholdDeg | SlowDown; Stop; RequestOperator | true | ready |
| botState.rollDegree | 롤 기울기 | CARD-KOR-003-terrain_or_dynamic_safety-002 | rollPitchThresholdDeg | SlowDown; Stop; RequestOperator | true | ready |
| detectedObjects[].type | 감지 객체 유형 | CARD-KOR-003-perception_requirement-001 | perceptionMinRangeM; safeDistanceCm | SlowDown; Stop; LocalAvoidance | true | ready |
| detectedObjects[].distanceCm | 객체 거리 | CARD-KOR-003-emergency_stop-001; CARD-KOR-003-perception_requirement-001 | emergencyStopDistanceCm; safeDistanceCm | EmergencyStop; Stop; LocalAvoidance | true | ready |
| detectedObjects[].timeToCollisionSec | 충돌 임박 시간 | CARD-KOR-003-emergency_stop-001; CARD-KOR-003-emergency_stop-002 | ttcThresholdSec | EmergencyStop; Stop | true | ready |
| detectedObjects[].isOnPath | 경로 위 객체 여부 | CARD-KOR-003-sidewalk_operation-001; CARD-KOR-003-perception_requirement-001 | safeDistanceCm | SlowDown; Stop; YieldWait | true | ready |
| pathContext.pathBlocked | 경로 차단 여부 | CARD-KOR-003-operator_control-001; CARD-KOR-003-perception_requirement-001 | maxRerouteAttempts | ReplanPath; RequestOperator | true | needs_more_evidence |
| pathContext.leftTraversable | 좌측 통과 가능성 | CARD-KOR-003-terrain_or_dynamic_safety-002 | traversabilityThreshold | LocalAvoidance; ReplanPath | false | needs_more_evidence |
| pathContext.rightTraversable | 우측 통과 가능성 | CARD-KOR-003-terrain_or_dynamic_safety-002 | traversabilityThreshold | LocalAvoidance; ReplanPath | false | needs_more_evidence |
| pathContext.leftClearanceCm | 좌측 여유 폭 | CARD-KOR-003-terrain_or_dynamic_safety-001 | robotWidthCm; sidewalkWidthCm | LocalAvoidance; ReplanPath | false | needs_more_evidence |
| pathContext.rightClearanceCm | 우측 여유 폭 | CARD-KOR-003-terrain_or_dynamic_safety-001 | robotWidthCm; sidewalkWidthCm | LocalAvoidance; ReplanPath | false | needs_more_evidence |
| terrain.traversabilityScore | 지면 통과 가능 점수 | CARD-KOR-003-terrain_or_dynamic_safety-001; CARD-KOR-003-terrain_or_dynamic_safety-002 | traversabilityThreshold | SlowDown; Stop; ReplanPath | true | ready |
| terrain.slopeDegree | 경사도 | CARD-KOR-003-terrain_or_dynamic_safety-002 | rollPitchThresholdDeg; traversabilityThreshold | SlowDown; Stop; ReplanPath | true | ready |
| terrain.curbHeightCm | 턱 높이 | CARD-KOR-003-terrain_or_dynamic_safety-002 | traversabilityThreshold | Stop; ReplanPath | true | ready |
| environments[].type | 환경 유형 | CARD-KOR-003-speed_policy-001; CARD-KOR-003-sidewalk_operation-001 | lowSpeedZoneSpeedKmh | SlowDown; YieldWait; Continue | true | ready |
| environments[].state | 환경 상태 | CARD-KOR-003-sidewalk_operation-001 | lowSpeedZoneSpeedKmh; waitTimeoutSec | Stop; YieldWait; Continue | true | ready |
| communicationStatus | 통신 상태 | CARD-KOR-003-operator_control-001; CARD-KOR-003-emergency_stop-002 | operatorOverrideEnabled | RequestOperator; EmergencyStop; Stop | true | ready |
