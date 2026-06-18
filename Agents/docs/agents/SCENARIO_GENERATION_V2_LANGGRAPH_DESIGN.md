# Scenario Generation v2 LangGraph Design

## 목적

Scenario generation v2 graph는 prompt-only 요청을 Project Scenario v1 JSON으로 변환하는 생성 workflow입니다. LangGraph는 node, edge, shared state, validation routing을 명시해 LLM 후보, deterministic fallback, repair loop를 같은 실행 모델 안에서 다룹니다.

## Graph

```text
START
-> validate_request_node
-> interpret_user_prompt_node
-> select_scenario_pattern_node
-> build_scenario_template_node
-> validate_scenario_template_node
   -> valid: build_response_node -> END
   -> repair: repair_scenario_template_node -> validate_scenario_template_node
   -> fallback: fallback_scenario_template_node -> validate_scenario_template_node
```

## State

`ScenarioGenerationGraphStateV2`는 graph node가 공유하는 typed state입니다.

```text
request
prompt
interpreted_intent
selected_pattern
llm_template_candidate
llm_validation
llm_warnings
scenario
validation
diagnostics
repair_count
status
summary
assumptions
response
output
```

## Node 책임

| Node | 책임 |
| --- | --- |
| `validate_request_node` | prompt 존재 여부 확인 |
| `interpret_user_prompt_node` | prompt 정규화, LLM 후보 우선 생성, 필요 시 deterministic intent 추출 |
| `select_scenario_pattern_node` | valid LLM 후보가 없을 때만 deterministic 생성 패턴 선택 |
| `build_scenario_template_node` | valid LLM 후보 채택 또는 deterministic JSON 생성 |
| `validate_scenario_template_node` | `TemplateValidator`로 Project Scenario v1 규칙 검증 |
| `repair_scenario_template_node` | deterministic local repair 후 재검증 |
| `fallback_scenario_template_node` | 안전한 deterministic fallback scenario 생성 |
| `build_response_node` | 내부 response model 구성 |

## LLM 후보 경로

`V2_AGENT_LLM_ENABLED=true`이면 `interpret_user_prompt_node`가 먼저 `AgentLlmJsonClient.generate_json()`을 호출합니다. 반환된 JSON은 `RepairHandler`와 `TemplateValidator`를 통과해야 합니다.

```text
prompt
-> AgentLlmJsonClient
-> JSON candidate
-> RepairHandler
-> TemplateValidator
   -> valid: deterministic selector/planner skip
   -> invalid: LLM repair or deterministic fallback
```

LLM 후보가 valid이면 `select_scenario_pattern_node`는 deterministic `ScenarioTypeSelector`를 호출하지 않습니다. 이 경로는 LLM을 최종 결정권자로 두는 것이 아니라, validator를 통과한 후보만 graph state에 채택하는 구조입니다.

## Routing

`validate_scenario_template_node` 이후 `route_validation_node`가 validation 결과와 `repair_count`로 분기합니다.

| Route | 조건 | 다음 node |
| --- | --- | --- |
| `valid` | validation 통과 | `build_response_node` |
| `repair` | validation 실패, repair_count < 2 | `repair_scenario_template_node` |
| `fallback` | repair 한도 초과 | `fallback_scenario_template_node` |

## Guardrails

* 외부 response는 raw Project Scenario v1 JSON입니다.
* graph 내부 diagnostics, validation, generation mode는 외부 response wrapper로 노출하지 않습니다.
* LLM 후보는 validator 통과 시에만 사용합니다.
* LLM 호출 실패, invalid output, repair 실패는 deterministic fallback으로 degraded 처리합니다.
* LangGraph import가 불가능한 환경에서만 `ScenarioGenerationV2Agent.generate()` fallback을 사용합니다.
