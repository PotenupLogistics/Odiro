# World Config Prompt Hardening

## 1. 목적 (Purpose)

이 문서는 Ollama live smoke에서 JSON extraction은 가능했지만 `world_config` validation이 실패한 뒤 추가한 prompt hardening 내용을 기록한다.

## 2. 확인된 문제 (Problem Observed)

이전 live smoke report에서는 JSON extraction이 성공해도 generated payload가 required nested field를 누락하거나 schema-extra field를 추가해 validation이 실패하는 문제가 확인됐다.

자주 누락된 required path는 다음과 같았다.

- `map.lengthCm`
- `map.sidewalkWidthCm`
- `map.slopeDegree`
- `map.surfaceCondition`
- `robot.botId`
- `robot.spawn`
- `robot.goal`
- `robot.policyId`
- `runtime.maxDurationSec`
- `runtime.captureReplay`
- `runtime.emitEventLog`

## 3. 강화 전략 (Hardening Strategy)

- `schemas/world_config.schema.json`에서 required field checklist를 직접 만든다.
- prompt에 allowed top-level field guidance를 포함한다.
- top-level과 nested object 내부의 extra key를 명시적으로 금지한다.
- required field에는 `null`을 사용하지 말라고 지시한다.
- initial prompt와 repair prompt에 schema-derived checklist content를 모두 포함한다.
- validation failure를 missing required fields, extra fields, enum errors, type errors로 그룹화한다.

## 4. Scope Limits

- JSON Schema는 변경하지 않는다.
- policy card와 RAG chunk는 변경하지 않는다.
- sample JSON이나 fixture file은 생성하지 않는다.
- vector DB나 embedding index는 생성하지 않는다.
- OpenAI call을 추가하지 않는다.

## 5. 다음 단계 (Next Step)

이 섹션은 초기 검증 계획이다. 현재 코드 기준으로는 OpenAI/Ollama provider client가 구현되어 있고, automated tests/harness에서는 실제 provider call을 하지 않는다. 추가 live smoke는 사용자가 명시적으로 요청한 경우에만 수행한다.

## 6. V2 Output Contract Hardening

Prompt hardening v2는 system prompt와 repair prompt에 명시적인 World Config output contract를 추가한다. 이 contract는 required nested path, allowed top-level field, extra field prohibition, scenario requirement binding을 나열한다.

이 변경은 JSON Schema를 바꾸지 않는다. 기존 contract를 model에게 더 명확히 전달하는 역할만 한다.
