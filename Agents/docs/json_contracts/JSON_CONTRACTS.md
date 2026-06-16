# JSON Contracts

## 1. 목적

이 문서는 UE5 배달 로봇 시뮬레이션과 AI 정책 판단 서버가 주고받을 JSON 계약의 위치와 단위 기준을 정리한다.

현재 프로젝트는 JSON Schema, Pydantic 모델, validation CLI/service, WorldConfig generation service, 사용자용 RunQueue generation API를 함께 제공한다. sample JSON과 fixture 파일은 repository에 추가하지 않는다.

## 2. UE5가 생성하는 JSON

- World Config: UE5 월드와 시나리오 구성을 위한 입력 계약
- Decision Request: UE5가 AI 서버에 전달하는 현재 상황 판단 요청
- Run Result: UE5 실행 후 결과와 metric을 전달하는 계약

## 3. AI가 생성하는 JSON

- Decision Response: AI 서버가 UE5에 반환하는 selectedAction과 command
- Policy Config: 정책 판단에 사용할 파라미터와 action 후보
- Evaluation Spec: Run Result를 점수화하기 위한 기준

## 4. schema 파일 위치

- `contracts/schemas/policy_config.schema.json`
- `contracts/schemas/world_config.schema.json`
- `contracts/schemas/decision_request.schema.json`
- `contracts/schemas/decision_response.schema.json`
- `contracts/schemas/evaluation_spec.schema.json`
- `contracts/schemas/run_result.schema.json`

## 5. Pydantic 모델 위치

- `app/models/policy.py`
- `app/models/world.py`
- `app/models/decision.py`
- `app/models/evaluation.py`
- `app/models/run_result.py`

## 6. 단위 기준

- 거리: cm
- 속도: kmh
- 시간: sec
- 각도: degree
- 좌표: UE5 world coordinate 기준

## 7. 현재 API / service 연결

* `POST /api/v1/scenarios/generate`: 사용자 자연어 `prompt`를 필수로 받고, 선택적 `episode_count`로 episode/run 개수를 지정할 수 있으며, wrapper 없는 RunQueue JSON을 반환한다.
* `POST /api/v1/analysis/run`: UE 실행 결과 파일과 setup 파일을 입력으로 받아 분석과 추천 결과를 반환한다.
* `app.services.json_contract_validator.validate_payload()`: 제출된 JSON payload를 schema와 Pydantic 모델로 검증한다.
* `app.services.world_config_generation_orchestrator.generate_world_config()`: 자연어 기반 WorldConfig generation 내부 흐름을 수행한다.

Sample JSON fixture 작성은 현재 repository 공유 범위가 아니다. runtime export는 local ignored path에만 저장한다.

## 8. Validation Layer

- service: `app/services/json_contract_validator.py`
- contract type mapping: `app/core/contract_types.py`
- CLI: `scripts/validate_contract.py`

CLI는 외부 JSON 파일을 읽어 JSON Schema와 Pydantic 모델을 모두 통과하는지 검증한다. 기본 실행은 report 파일을 만들지 않으며, `--report` 옵션이 있을 때만 `contract_validation_report.json`을 생성한다.
