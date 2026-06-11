# JSON Contract Validation Guide

## 1. 목적

validation layer는 UE5 또는 외부 도구가 제공하는 JSON payload를 AI 서버 내부 모델로 사용하기 전에 검증하기 위한 공통 계층이다.

## 2. 지원 contract type

- `policy_config`
- `world_config`
- `decision_request`
- `decision_response`
- `evaluation_spec`
- `run_result`

## 3. CLI 사용법

```powershell
uv run python scripts/validate_contract.py --type decision_request --file path/to/request.json
uv run python scripts/validate_contract.py --type policy_config --file path/to/policy.json
uv run python scripts/validate_contract.py --type world_config --file path/to/world.json
```

report가 필요할 때만 `--report`를 지정한다.

```powershell
uv run python scripts/validate_contract.py --type policy_config --file path/to/policy.json --report harness/reports/contract_validation_report.json
```

## 4. 검증 방식

- JSON Schema 검증: 외부 JSON의 구조, required field, enum, 기본 타입을 확인한다.
- Pydantic 검증: Python 내부 모델로 변환 가능한지 확인하고 normalized payload를 만든다.
- 둘 중 하나라도 실패하면 validation result는 `valid=false`다.

## 5. sample JSON을 아직 만들지 않는 이유

이번 단계는 검증 계층을 정의하는 단계다. sample JSON과 fixture는 schema와 validation layer가 안정화된 이후 별도 단계에서 작성한다.

## 6. UE5 연동 전 검증 방법

UE5가 생성한 JSON 파일은 API로 보내기 전에 `scripts/validate_contract.py` 또는 `app.services.json_contract_validator.validate_payload`로 먼저 검증한다.
