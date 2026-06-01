# World Config Prompt Spec

## Current Prompt Builder Implementation

The prompt builder now creates prompt packages only.

- System prompt: describes the World Config generator role, unit rules, JSON-only output, schema usage, and prohibited guarantee claims.
- User prompt: includes the original natural-language prompt, constraints, policyId, retrieved policy context, and required World Config fields.
- Repair prompt: includes validation errors and asks for a corrected JSON object only.
- RAG context comes from deterministic retrieval over policy RAG chunks.
- LLM API calls and FastAPI endpoints are deferred to later stages.

## 1. System Prompt 원칙

- 너는 UE5 배달 로봇 시뮬레이션용 World Config JSON 생성기다.
- 반드시 `world_config.schema.json` 구조를 따른다.
- 단위는 cm, kmh, sec, degree를 사용한다.
- schema에 없는 필드는 생성하지 않는다.
- 정책 준수/안전 보장 표현을 쓰지 않는다.
- 출력은 JSON object 하나만 한다.

## 2. User Prompt 구성요소

- scenario description
- fixed constraints
- optional constraints
- policyId
- seed
- map constraints
- object constraints

## 3. Repair Prompt 원칙

validation error를 LLM에 전달할 때:

- 어떤 필드가 누락되었는지
- 어떤 enum 값이 잘못되었는지
- 어떤 타입이 잘못되었는지
- 수정 후 JSON만 다시 출력하도록 요청한다.

## 4. 금지 표현

- 공식 인증 준수
- 법적 안전 보장
- 인증 수준
- 실제 배포 가능 보장

## 5. Prompt Hardening

The World Config prompt now includes a schema-derived required field checklist and allowed-field guidance. The model is instructed to include all required nested fields such as `map.lengthCm`, `robot.botId`, `robot.spawn.x`, `robot.goal.x`, and `runtime.maxDurationSec`.

Extra keys are not allowed at the top level or inside nested objects. Required fields must not be returned as `null`; the generator must choose a schema-valid value within the scenario context.

Repair prompts group validation feedback into missing required fields, schema-extra fields, enum errors, and type errors, then request a corrected JSON object only.

## 6. Scenario Requirements

The user prompt includes `Scenario Intent Summary` and `Scenario Requirements` sections. These sections tell the generator to represent scenario-specific details that are not guaranteed by schema validation alone.

Current examples:

- If the user mentions Kickboard, `obstacles` must include type `Kickboard`.
- If the user mentions pedestrian crossing, `pedestrians[].behavior` must represent crossing.
- If the user says the robot path is blocked, an obstacle must include a path-blocking cue such as `blockingRatio`.
- If the user mentions narrow sidewalk, `map.sidewalkWidthCm` must represent a narrow sidewalk in cm.

## 7. Output Contract

The system prompt includes an `Output Contract` section. It states that the model must return exactly one JSON object, include all required nested fields, avoid markdown or explanation text, and avoid keys outside the schema.
