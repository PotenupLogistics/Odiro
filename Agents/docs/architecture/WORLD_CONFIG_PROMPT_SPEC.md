# World Config Prompt Spec

이 문서는 WorldConfig prompt builder가 LLM에 어떤 지시와 제약을 전달하는지 설명한다. prompt builder는 prompt package를 만들고, 실제 provider 호출 여부는 generation endpoint와 orchestrator 설정이 결정한다.

## 1. 현재 역할

WorldConfig prompt builder는 다음 정보를 조합한다.

* system prompt
* user prompt
* original natural-language prompt
* fixed/optional constraints
* policyId
* seed
* deterministic RAG context
* schema-derived required field checklist
* scenario intent summary
* scenario requirements
* repair prompt용 validation feedback

`build_world_config_prompt_package()` service 함수는 이 prompt package를 확인하는 경로다. 이 함수 자체는 LLM을 호출하지 않는다.

`generate_world_config()` service 함수는 설정된 provider와 orchestration 흐름에 따라 실제 WorldConfig generation을 수행할 수 있다. Public `/api/v1/scenarios/generate`는 RunQueue 제거 안내만 반환한다.

## 2. System Prompt 원칙

* UE5 배달 로봇 시뮬레이션용 `WorldConfig` JSON 생성기 역할을 명시한다.
* 반드시 `world_config.schema.json` 구조를 따르게 한다.
* 단위는 cm, kmh, sec, degree를 사용한다.
* schema에 없는 필드는 생성하지 않게 한다.
* 정책 준수, 법적 안전 보장, 인증 수준 같은 표현을 금지한다.
* 출력은 markdown이 아니라 JSON object 하나만 허용한다.

## 3. User Prompt 구성 요소

user prompt에는 다음 항목이 포함된다.

* scenario description
* fixed constraints
* optional constraints
* policyId
* seed
* map constraints
* object constraints
* retrieved policy context
* Scenario Intent Summary
* Scenario Requirements

## 4. Schema checklist와 allowed-field guidance

prompt에는 schema에서 파생한 required field checklist가 포함된다. 모델은 `map.lengthCm`, `robot.botId`, `robot.spawn.x`, `robot.goal.x`, `runtime.maxDurationSec` 같은 필수 nested field를 누락하지 않아야 한다.

top-level과 nested object 모두 schema 밖의 extra key를 만들 수 없다. required field는 `null`로 채우지 않고, scenario context 안에서 schema-valid 값을 선택해야 한다.

## 5. Scenario Requirements

schema validation만으로 보장되지 않는 의미 요구사항은 `Scenario Intent Summary`와 `Scenario Requirements`에 넣는다.

현재 대표 규칙:

* 사용자가 Kickboard를 언급하면 `obstacles`에 `Kickboard` 의도가 반영되어야 한다.
* 사용자가 pedestrian crossing을 언급하면 `pedestrians[].behavior`가 crossing 의미를 표현해야 한다.
* 사용자가 robot path blocked 상황을 요청하면 obstacle에는 `blockingRatio` 같은 path-blocking cue가 있어야 한다.
* 사용자가 narrow sidewalk를 요청하면 `map.sidewalkWidthCm`가 좁은 보도 폭을 cm 단위로 표현해야 한다.

## 6. Policy comparison prompt guidance

`narrow_sidewalk_obstacle_ahead_blocked_path` 흐름은 scene variation이 아니라 policy comparison 구조로 다룬다.

OpenAI가 만드는 base WorldConfig는 고정 장면의 의미를 제공하고, backend는 deterministic queue generator에서 동일 EpisodeSetup 하나와 policy별 DeliveryBotSetup 5개를 만든다. DeliveryBotSetup tuning 값은 LLM이 임의 생성하지 않고 default catalog와 variation policy를 기준으로 채운다.

## 7. Repair Prompt 원칙

validation error를 LLM에 전달할 때는 다음처럼 분류한다.

* missing required fields
* schema-extra fields
* enum errors
* type errors

repair prompt는 수정된 JSON object만 다시 출력하게 한다. 설명문, markdown fence, raw reasoning은 허용하지 않는다.

## 8. Output Contract

model output은 정확히 하나의 JSON object여야 한다.

* required nested fields를 모두 포함한다.
* schema 밖의 key를 만들지 않는다.
* 설명문과 markdown을 출력하지 않는다.
* required field를 `null`로 채우지 않는다.
* optional field는 값이 없으면 backend export 단계에서 생략될 수 있다.
