# Policy Card Coverage Report

- totalCards: 9
- cardsByCategory: emergency_stop 2, operator_control 1, perception_requirement 1, sidewalk_operation 1, speed_policy 2, terrain_or_dynamic_safety 2

## MVP Situation Coverage

- PedestrianAhead: CARD-KOR-003-perception_requirement-001, CARD-KOR-003-sidewalk_operation-001
- ObstacleAhead: CARD-KOR-003-perception_requirement-001, CARD-KOR-003-emergency_stop-001
- FallOrTilt: CARD-KOR-003-terrain_or_dynamic_safety-001, CARD-KOR-003-terrain_or_dynamic_safety-002
- TerrainRisk: CARD-KOR-003-terrain_or_dynamic_safety-001, CARD-KOR-003-terrain_or_dynamic_safety-002
- ApproachingObject: CARD-KOR-003-perception_requirement-001, needs_more_evidence
- CrosswalkApproach: CARD-KOR-003-sidewalk_operation-001
- CommunicationIssue: CARD-KOR-003-operator_control-001, CARD-KOR-003-emergency_stop-002
- CrowdedPath: CARD-KOR-003-operator_control-001, CARD-KOR-003-perception_requirement-001, needs_more_evidence

## Gaps

- ApproachingObject 세부 객체 유형은 RSR 또는 추가 KOR confirmed 근거가 필요하다.
- CrowdedPath 판단 기준은 추가 confirmed 근거가 필요하다.
- Evaluation metric은 RSR-001 검토 후 보강이 필요하다.

## Next Step Recommendation

1. Policy Config JSON 스키마 설계 전에 파라미터 후보를 검토한다.
2. Decision Request 필드 후보의 requiredForMvp를 확정한다.
3. 부족 영역은 추가 manual confirmation 또는 RSR 검토로 보강한다.
