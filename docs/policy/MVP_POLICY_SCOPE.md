# MVP Policy Scope

## 1. MVP에서 다루는 상황

* PedestrianAhead: 전방 보행자 발견
* ObstacleAhead: 전방 장애물 발견
* FallOrTilt: 로봇 기울어짐/전복
* TerrainRisk: 턱/경사/불확실 지면
* ApproachingObject: 로봇에 접근하는 객체

## 2. MVP에서 다루는 액션

* SlowDown: 속도 제어
* Stop: 정지
* EmergencyStop: 긴급 정지
* ReplanPath: 경로 재탐색, A*
* LocalAvoidance: 회피, DWA 또는 간소화 회피
* RequestOperator: 관제 요청
* YieldWait: 보행자/장애물 통과 대기
* Continue: 기존 주행 유지

## 3. MVP 필수 정책 카테고리

* speed_policy
* emergency_stop
* perception_requirement
* operator_control
* sidewalk_operation
* crosswalk_operation
* terrain_or_dynamic_safety

## 4. MVP와 직접 연결되지 않는 카테고리

* legal_background
* certification_process
* data_recording

주의:
legal_background과 certification_process는 정책 근거 배경으로는 중요하지만, 당장 Policy Config 파라미터로 직접 변환할 후보는 후순위로 둔다.
