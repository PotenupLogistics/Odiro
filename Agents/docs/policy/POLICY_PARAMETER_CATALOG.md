# Policy Parameter Catalog

## 1. 목적

MVP Policy Config에 들어갈 후보 파라미터를 policy card 근거와 연결하여 정리한다. 구체 수치는 아직 확정하지 않으며, 실제 초기값은 JSON Schema 이후 Policy Config 작성 단계에서 정한다.

## 2. 파라미터 목록

| parameterName | unit | meaning | relatedCardIds | relatedMvpSituation | relatedActions | sourceCategory | status |
| --- | --- | --- | --- | --- | --- | --- | --- |
| maxSpeedKmh | km/h | 최대 주행 속도 후보 | CARD-KOR-003-speed_policy-001; CARD-KOR-003-speed_policy-002 | PedestrianAhead; CrosswalkApproach | SlowDown; Stop | speed_policy | ready |
| lowSpeedZoneSpeedKmh | km/h | 보호구역 또는 저속 구간 속도 후보 | CARD-KOR-003-speed_policy-001; CARD-KOR-003-sidewalk_operation-001 | PedestrianAhead; CrosswalkApproach | SlowDown; YieldWait; Continue | speed_policy; sidewalk_operation | ready |
| emergencyStopDistanceCm | cm | 긴급 정지 판단 거리 후보 | CARD-KOR-003-emergency_stop-001; CARD-KOR-003-emergency_stop-002 | ObstacleAhead; CommunicationIssue | EmergencyStop; Stop | emergency_stop | ready |
| ttcThresholdSec | sec | 충돌 임박 시간 판단 후보 | CARD-KOR-003-emergency_stop-001; CARD-KOR-003-perception_requirement-001 | ObstacleAhead; ApproachingObject | EmergencyStop; Stop; LocalAvoidance | emergency_stop; perception_requirement | ready |
| safeDistanceCm | cm | 사람/장애물과의 안전 거리 후보 | CARD-KOR-003-perception_requirement-001; CARD-KOR-003-sidewalk_operation-001 | PedestrianAhead; ObstacleAhead | SlowDown; Stop; LocalAvoidance | perception_requirement; sidewalk_operation | ready |
| perceptionMinRangeM | m | 인지 최소 범위 후보 | CARD-KOR-003-perception_requirement-001 | PedestrianAhead; ObstacleAhead; ApproachingObject | SlowDown; Stop; LocalAvoidance; ReplanPath | perception_requirement | ready |
| traversabilityThreshold | score | 지면 통과 가능성 판단 후보 | CARD-KOR-003-terrain_or_dynamic_safety-001; CARD-KOR-003-terrain_or_dynamic_safety-002 | TerrainRisk; FallOrTilt | SlowDown; Stop; ReplanPath | terrain_or_dynamic_safety | ready |
| rollPitchThresholdDeg | deg | 기울기/전복 위험 판단 후보 | CARD-KOR-003-terrain_or_dynamic_safety-002 | FallOrTilt; TerrainRisk | SlowDown; Stop; RequestOperator | terrain_or_dynamic_safety | ready |
| botMassKg | kg | 로봇 질량 제한 및 속도/안전 판단 후보 | CARD-KOR-003-terrain_or_dynamic_safety-001; CARD-KOR-003-terrain_or_dynamic_safety-002 | TerrainRisk | Stop; ReplanPath; RequestOperator | terrain_or_dynamic_safety | ready |
| robotWidthCm | cm | 로봇 폭 제한 및 보도 폭 판단 후보 | CARD-KOR-003-terrain_or_dynamic_safety-001 | TerrainRisk; CrowdedPath | Stop; ReplanPath; RequestOperator | terrain_or_dynamic_safety | ready |
| sidewalkWidthCm | cm | 보도 폭 여유 판단 후보 | CARD-KOR-003-terrain_or_dynamic_safety-001 | TerrainRisk; CrowdedPath | Stop; ReplanPath; RequestOperator | terrain_or_dynamic_safety | ready |
| waitTimeoutSec | sec | 대기 후 관제 요청 또는 계속 판단 후보 | CARD-KOR-003-sidewalk_operation-001; CARD-KOR-003-operator_control-001 | CrosswalkApproach; CrowdedPath | YieldWait; RequestOperator | sidewalk_operation; operator_control | needs_more_evidence |
| maxRerouteAttempts | count | 경로 재탐색 반복 제한 후보 | CARD-KOR-003-operator_control-001; CARD-KOR-003-terrain_or_dynamic_safety-002 | TerrainRisk; CrowdedPath | ReplanPath; RequestOperator | operator_control; terrain_or_dynamic_safety | needs_more_evidence |
| operatorOverrideEnabled | boolean | 관제/수동 개입 활성화 후보 | CARD-KOR-003-operator_control-001; CARD-KOR-003-emergency_stop-002 | CommunicationIssue; CrowdedPath | RequestOperator; Stop; EmergencyStop | operator_control; emergency_stop | ready |
