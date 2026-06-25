# Scenario Generation v2 Agent Architecture

## 개요

Scenario generation v2는 사용자 자연어 prompt에서 Project Scenario v1 JSON을 생성하는 에이전트입니다. 실행 흐름은 항상 `ScenarioGenerationGraphRunnerV2`가 맡고, LangGraph `StateGraph`가 노드 실행과 검증/보정/fallback 분기를 제어합니다.

AI 서버 설정은 `.env`에서 읽습니다. UserProject의 `<UserProject>/setting.json`은 episode count, seed, FPS 같은 시뮬레이션 실행 설정이며, 이 에이전트의 LLM 사용 여부를 제어하지 않습니다.

## 런타임 설정

| 설정 | 기본값 | 의미 |
| --- | --- | --- |
| `V2_AGENT_LLM_ENABLED` | `false` | `true`이면 LLM JSON 후보를 먼저 생성하고, validator 통과 시 채택합니다. |
| `V2_AGENT_LLM_REPAIR_ENABLED` | `true` | LLM 후보가 invalid일 때 LLM 기반 repair를 시도할지 정합니다. |
| `V2_AGENT_LLM_MAX_REPAIR_ATTEMPTS` | `1` | LLM repair 허용 횟수입니다. 현재 runner는 0보다 크면 1회 repair를 시도합니다. |

## 실행 흐름

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

## LLM 우선 경로

`V2_AGENT_LLM_ENABLED=true`이면 `interpret_user_prompt_node`가 prompt 정규화 직후 LLM 후보 생성을 먼저 시도합니다. 후보가 `TemplateValidator`를 통과하면 deterministic `ScenarioTypeSelector`/`TemplatePlanner` 경로를 건너뛰고 `build_scenario_template_node`에서 해당 후보를 채택합니다.

LLM 후보가 없거나 invalid이면 deterministic 경로로 내려갑니다.

```text
prompt
-> LLM candidate
   -> valid: scenario 채택
   -> invalid: LLM repair 시도
   -> failed: deterministic planner/writer 사용
```

## 구성요소

| 구성요소 | 책임 |
| --- | --- |
| `RequestNormalizer` | prompt trim과 공백 정리 |
| `IntentParser` | deterministic fallback에 필요한 의도 신호 추출 |
| `ScenarioTypeSelector` | deterministic fallback에서 지원 가능한 생성 패턴 선택 |
| `TemplatePlanner` | deterministic fallback용 생성 계획 작성 |
| `TemplateJsonWriter` | deterministic Project Scenario JSON 작성 |
| `AgentLlmJsonClient` | 설정된 provider(OpenAI/Ollama)에 JSON 생성 요청 |
| `TemplateValidator` | Project Scenario v1 규칙 검증 |
| `RepairHandler` | 자동 보정 가능한 JSON 구조 수정 |
| `ResponseBuilder` | 내부 `ScenarioGenerateV2Response` 구성 |

## 출력 계약

외부 API 응답은 wrapper가 아니라 raw Project Scenario v1 JSON입니다.

필수 root key:

```text
schema
version
scenario_id
intent
corridor
obstacles
pedestrians
robot
```

`generation_mode`, `validation`, `diagnostics`, `assumptions`는 graph 내부 상태와 내부 response model에서만 사용하고 외부 response body에는 노출하지 않습니다.
