# POLICY_EXTRACTION_MATRIX

## 목적

이 문서는 KOR-001~KOR-005 원본 PDF와 processed Markdown을 수동 대조할 때 어떤 정책 후보를 확인해야 하는지 정리한 검토용 매트릭스다.

이 문서는 policy knowledge card가 아니다. 아래 항목은 사람이 원문에서 근거를 확인하기 전까지 정책 카드로 사용하지 않는다.

## 문서별 기대 추출 항목

| sourceId | 기대 추출 항목 | 현재 상태 |
| --- | --- | --- |
| KOR-001 | legal_background, certification_process, sidewalk_operation 관련 법적 근거 후보 | manual review required |
| KOR-002 | sidewalk_operation, crosswalk_operation, legal_background 관련 도로교통법 후보 | manual review required |
| KOR-003 | speed_policy, emergency_stop, perception_requirement, operator_control, data_recording 후보 | manual review required |
| KOR-004 | certification_process, speed_policy, emergency_stop, perception_requirement, operator_control 후보 | manual review required |
| KOR-005 | legal_background, sidewalk_operation, crosswalk_operation 관련 정의 및 운행 기준 후보 | manual review required |

## 정책 카테고리 설명

| 카테고리 | 의미 | 관련 Policy Config 파라미터 | 관련 Decision Request 필드 | 관련 Decision Response 액션 | 관련 Run Result metric |
| --- | --- | --- | --- | --- | --- |
| speed_policy | 로봇 주행 속도 제한 및 저속 주행 기준 | maxSpeedKmh, lowSpeedZoneSpeedKmh | botState.speedKmh, environments[].type | SlowDown, Stop | deliveryTimeSec, nearMissCount |
| emergency_stop | 비상정지 조건, 정지 거리, 안전 정지 기준 | emergencyStopEnabled, minStopDistanceM | botState.speedKmh, perception.hazards[] | Stop, EmergencyStop | emergencyStopCount, collisionCount |
| perception_requirement | 주변 인식, 장애물 감지, 보행자 인식 요구사항 | perceptionMinRangeM, pedestrianDetectionRequired | perception.objects[], perception.confidence | SlowDown, Stop, RequestOperatorReview | nearMissCount, perceptionFailureCount |
| operator_control | 관제장치, 원격 제어, 운영자 개입 기준 | operatorOverrideEnabled, maxRemoteResponseSec | operator.status, controlMode | RequestOperatorReview, Stop | operatorInterventionCount, remoteOverrideLatencyMs |
| sidewalk_operation | 보도 통행 가능 여부와 보행자 안전 기준 | sidewalkAllowed, pedestrianPriorityRequired | environments[].type, route.segmentType | Continue, SlowDown, Stop | sidewalkViolationCount, pedestrianYieldCount |
| crosswalk_operation | 횡단보도 접근, 진입, 통과 관련 기준 | crosswalkStopRequired, crosswalkMaxSpeedKmh | environments[].type, trafficSignal.state | Stop, SlowDown, CrossWithCaution | crosswalkStopCount, signalViolationCount |
| terrain_or_dynamic_safety | 경사, 노면, 장애물, 동적 위험 조건에서의 안전 기준 | maxSlopeDeg, obstacleClearanceM | terrain.slopeDeg, perception.hazards[] | Reroute, SlowDown, Stop | rerouteCount, hazardAvoidanceCount |
| data_recording | 운행 기록, 사고 기록, 로그 보존 기준 | recordTelemetry, incidentLogRetentionDays | requestId, botState, decisionContext | RecordEvent, RequestOperatorReview | eventLogCompleteness, incidentReportCount |
| legal_background | 실외이동로봇 정의, 법적 지위, 적용 법령 배경 | jurisdiction, legalBasisSourceIds | route.country, environments[].type | MarkLegalReviewRequired | legalReviewFlagCount |
| certification_process | 운행안전인증 절차, 기준, 심사 관련 요구사항 | certificationRequired, certificationSourceIds | botProfile.certificationStatus | RejectOperation, RequestCertificationReview | certificationBlockCount |

## 사용 원칙

- 각 카테고리는 검토자가 원본 PDF에서 근거 문장, 페이지, 조항을 확인한 뒤에만 정책 카드 후보로 승격한다.
- processed Markdown이 `partial`인 문서는 원본 PDF와 대조하기 전까지 `reviewed`로 표시하지 않는다.
- Policy Config, Decision Request, Decision Response, Run Result metric 연결은 설계 검토를 위한 후보이며 현재 단계에서 JSON schema나 fixture를 생성하지 않는다.
- processed Markdown에서 자동 추출된 후보는 모두 `needs_pdf_check` 상태로 시작한다.
- 자동 후보는 원본 PDF 수동 대조 후에만 `confirmed` 또는 `rejected`로 판단한다.
- `confirmed` 후보만 policy knowledge card 생성 대상으로 사용한다.
