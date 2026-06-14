# v2 Agent Testing And Operations Guide

## 1. v2 API 테스트 명령

```powershell
uv run pytest tests/test_v2_scenario_generation_api.py tests/test_v2_analysis_run_api.py -q
```

이 테스트는 v2 request/response 계약, deterministic/rule-based 기본 경로, fake/mock client 기반 LLM 경로, fallback warning을 검증합니다.

## 2. v1 회귀 테스트 명령

```powershell
uv run pytest tests/test_scenario_generation_api.py tests/test_analysis_run_endpoint.py -q
```

v2 작업 후에도 v1 scenario generation과 v1 analysis endpoint가 유지되는지 확인합니다.

## 3. 전체 테스트 명령

```powershell
uv run ruff check app tests
uv run pytest -q
```

문서 중심 변경이어도 전체 회귀를 실행합니다.

## 4. LLM disabled 기본 테스트 방법

기본 설정은 아래와 같습니다.

```text
V2_AGENT_LLM_ENABLED=false
```

이 상태에서는 외부 LLM provider를 호출하지 않습니다.

* scenario generation은 deterministic template writer를 사용합니다.
* analysis는 rule-based failure pattern detector와 recommendation generator를 사용합니다.
* 외부 API key 없이 테스트가 통과해야 합니다.

## 5. fake/mock client 기반 LLM 경로 테스트

LLM mode 테스트는 실제 provider를 호출하지 않고 fake JSON client를 agent에 주입합니다.

검증 대상:

* valid LLM JSON output
* Markdown JSON code block parsing
* invalid template 후 repair 성공
* LLM generation/repair 모두 실패 후 fallback
* LLM recommendation evidence validation
* invalid recommendation 후 rule-based fallback

## 6. 외부 API key 없이 테스트가 통과해야 하는 이유

v2 LLM mode는 optional 기능입니다. CI와 로컬 개발 기본 경로는 deterministic/rule-based여야 하며, 네트워크 상태나 provider key 존재 여부가 기본 테스트 성공 여부를 좌우하면 안 됩니다.

운영에서 LLM을 켤 때만 provider 설정과 API key를 준비합니다.

## 7. experiments root 설정

`/api/v2/analysis/run`은 experiments root 하위 파일을 분석합니다. 기본 fallback은 `%APPDATA%/OdiroSim/experiments` 계열이며, 명시적으로 지정하려면 `ODIROSIM_EXPERIMENTS_DIR`을 사용합니다.

Unix-like 예:

```text
ODIROSIM_EXPERIMENTS_DIR=/path/to/OdiroSim/experiments
```

Windows cmd 예:

```text
set ODIROSIM_EXPERIMENTS_DIR=C:\Users\<USER>\AppData\Roaming\OdiroSim\experiments
```

PowerShell 예:

```powershell
$env:ODIROSIM_EXPERIMENTS_DIR="C:\Users\<USER>\AppData\Roaming\OdiroSim\experiments"
```

## 8. LLM mode 운영 설정

```text
V2_AGENT_LLM_ENABLED=true
V2_AGENT_LLM_REPAIR_ENABLED=true
V2_AGENT_LLM_MAX_REPAIR_ATTEMPTS=1
```

LLM mode를 켜도 API는 fallback 정책을 유지합니다. LLM 호출 실패, JSON 파싱 실패, validator 실패는 API 500이 아니라 fallback response와 warning으로 처리합니다.

## 9. 운영 확인 체크리스트

1. `/health`가 200을 반환하는지 확인합니다.
2. `/api/v2/scenarios/generate`가 prompt-only request로 200을 반환하는지 확인합니다.
3. 실행 개수 필드가 422로 거부되는지 확인합니다.
4. `/api/v2/analysis/run`이 `{}` 또는 body 없음으로 200을 반환하는지 확인합니다.
5. 데이터가 없을 때 `overall_judgement="insufficient_data"`인지 확인합니다.
6. LLM mode를 켠 경우 `generation_mode` 또는 `analysis_mode`와 `warnings`를 함께 확인합니다.
