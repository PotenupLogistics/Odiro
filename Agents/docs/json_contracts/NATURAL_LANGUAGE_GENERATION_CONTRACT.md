# Natural Language Generation Contract

이 문서는 자연어 입력을 기반으로 `WorldConfig` 또는 UE 실행용 RunQueue를 생성하는 API 계약을 설명한다. 파일명, API path, JSON field name은 영어 원문을 유지한다.

## Generation Endpoint Contract

WorldConfig generation은 `app.services.world_config_generation_orchestrator.generate_world_config()` service 함수가 `WorldConfigGenerationRequest`를 입력받고 `WorldConfigGenerationResult`를 반환한다. 이전 `/api/v1/generation/world-config` endpoint는 public API에서 제거되었다.

사용자용 scenario 생성 API는 다음 endpoint다.

```text
POST /api/v1/scenarios/generate
```

이 endpoint는 자연어 `prompt`를 필수로 받고, 선택적으로 `episode_count`를 허용한다. 성공 시 wrapper field 없는 RunQueue JSON을 반환한다. 사용자가 EpisodeSetup / DeliveryBotSetup / RunQueue JSON을 직접 작성하지 않는다. `episode_count`가 없으면 `SCENARIO_EPISODE_DEFAULT_COUNT`를 사용한다.

현재 provider 동작:

- `disabled`: `provider_disabled` 실패 결과를 반환한다.
- `openai`: live provider client가 구현되어 있으며 수동 smoke에서만 실제 호출한다.
- `ollama`: fallback provider로 유지한다.
- automated tests와 harness checks는 실제 OpenAI/Ollama 호출을 하지 않는다.

`generatedPayload`는 JSON extraction과 validation이 성공한 경우에만 포함한다.

## LLM Provider Status

generation contract는 prompt package 생성 후 LLM client abstraction을 사용한다.

- 기본 provider: `disabled`
- OpenAI/Ollama provider client: 구현됨
- 실제 provider call: 수동 smoke 또는 명시 요청에서만 수행
- UE5 handoff 전 validation은 항상 필수

## Current Service Layer

현재 service layer는 `world_config` 생성을 위한 내부 prompt package 구성을 지원한다.

- 입력은 `WorldConfigGenerationRequest`로 표현한다.
- 제약 조건은 `WorldConfigGenerationConstraints`로 표현한다.
- prompt package 출력은 `WorldConfigPromptPackage`로 표현한다.
- repair prompt는 `WorldConfigRepairPromptPackage`로 표현한다.
- `targetContractType`은 `world_config` 중심으로 유지한다.
- 최신 사용자용 RunQueue 생성은 `POST /api/v1/scenarios/generate`에서 backend 내부 변환/export 경로를 통해 제공한다.

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
- 사용자가 UE JSON을 직접 입력하는 구조는 제공하지 않음
- RunQueue JSON은 AI/backend 내부 생성 산출물로만 반환

## Scenario Reflection Field

generation response는 `scenarioReflection`을 포함할 수 있다. 이 field는 생성된 World Config가 좁은 보도, Kickboard obstacle, path blocking, pedestrian crossing 같은 추출된 scenario requirement를 반영했는지 보고한다.
