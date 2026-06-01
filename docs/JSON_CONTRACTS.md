# JSON Contracts

## 1. 목적

이 문서는 UE5 배달 로봇 시뮬레이션과 AI 정책 판단 서버가 주고받을 JSON 계약의 위치와 단위 기준을 정리한다.

이번 단계에서는 JSON Schema와 Pydantic 모델만 정의한다. sample JSON, fixture, FastAPI API는 생성하지 않는다.

## 2. UE5가 생성하는 JSON

- World Config: UE5 월드와 시나리오 구성을 위한 입력 계약
- Decision Request: UE5가 AI 서버에 전달하는 현재 상황 판단 요청
- Run Result: UE5 실행 후 결과와 metric을 전달하는 계약

## 3. AI가 생성하는 JSON

- Decision Response: AI 서버가 UE5에 반환하는 selectedAction과 command
- Policy Config: 정책 판단에 사용할 파라미터와 action 후보
- Evaluation Spec: Run Result를 점수화하기 위한 기준

## 4. schema 파일 위치

- `schemas/policy_config.schema.json`
- `schemas/world_config.schema.json`
- `schemas/decision_request.schema.json`
- `schemas/decision_response.schema.json`
- `schemas/evaluation_spec.schema.json`
- `schemas/run_result.schema.json`

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

## 7. 다음 단계

1. schema와 Pydantic 모델 검증
2. sample JSON fixture 작성
3. validation API 또는 FastAPI endpoint 구현

## 8. Validation Layer

- service: `app/services/json_contract_validator.py`
- contract type mapping: `app/core/contract_types.py`
- CLI: `scripts/validate_contract.py`

CLI는 외부 JSON 파일을 읽어 JSON Schema와 Pydantic 모델을 모두 통과하는지 검증한다. 기본 실행은 report 파일을 만들지 않으며, `--report` 옵션이 있을 때만 `contract_validation_report.json`을 생성한다.
