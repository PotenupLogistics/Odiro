# Natural Language Generation Contract

## Generation Endpoint Contract

`POST /api/v1/generation/world-config` accepts `WorldConfigGenerationRequest` and returns `WorldConfigGenerationResult`.

Current provider behavior:

- `disabled`: returns a failed result with `provider_disabled`.
- `openai`, `gemini`, `ollama`, `custom`: return not implemented until provider clients are added.

`generatedPayload` is included only when extraction and validation succeed.

## LLM Provider Status

The generation contract will later use the LLM client abstraction after prompt package creation.

- Current provider: `disabled`
- Actual provider calls: not implemented
- Generated World Config payload: not returned in this stage
- Validation remains mandatory before UE5 handoff

## Current Service Layer

The current service layer supports internal prompt package construction for `world_config` generation.

- Input is represented by `WorldConfigGenerationRequest`.
- Constraints are represented by `WorldConfigGenerationConstraints`.
- Output is represented by `WorldConfigPromptPackage`.
- Repair prompts are represented by `WorldConfigRepairPromptPackage`.
- `targetContractType` remains limited to `world_config`.
- External LLM invocation and API transport are not implemented in this stage.

## 1. Generation Request 초안

파일을 생성하지 말고 문서 안에 구조만 설명한다.

필드:

- schemaVersion
- requestId
- generationType
- prompt
- targetContractType
- policyId
- constraints
- maxRepairAttempts

generationType:

- world_config

targetContractType:

- world_config

constraints:

- unitSystem
- allowedMapTypes
- allowedObjectTypes
- fixedPolicyId
- defaultSeed
- requireValidation

## 2. Generation Response 초안

필드:

- schemaVersion
- requestId
- generationId
- targetContractType
- valid
- generatedPayload
- validation
- assumptions
- warnings

## 3. MVP 제한

- targetContractType은 world_config만 허용
- generatedPayload는 validation 통과 후에만 반환
- API 구현은 다음 단계에서 진행

## Scenario Reflection Field

Generation responses may include `scenarioReflection`. This field reports whether the generated World Config reflects extracted scenario requirements such as narrow sidewalk, Kickboard obstacle, path blocking, and pedestrian crossing.
