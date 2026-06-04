# World Config Generation Orchestrator

이 문서는 자연어 요청에서 검증된 `WorldConfig` 생성 결과까지 이어지는 orchestration 흐름을 설명한다. 현재 orchestrator는 scenario repair prompt로 넘어가기 전에 deterministic scenario post-processing을 먼저 적용한다. 현재 순서는 다음과 같다.

Natural Language Request -> Prompt Package -> LLM Client -> Raw Content -> JSON Extraction -> Contract Validation -> Scenario Reflection -> Scenario Post-Processing -> Contract Validation -> Scenario Reflection -> Repair Loop -> Generation Result

post-processing이 semantic gap을 해결하면 `generatedPayload`와 함께 `scenarioPostProcessing` diagnostics 및 patch record를 반환한다. 해결하지 못하면 기존 scenario repair prompt flow가 다음 단계로 실행된다.

UE5 handoff service는 최종 generation result를 입력으로 사용한다. `generatedPayload`를 `worldConfig`로 노출하기 전에 contract validation을 한 번 더 수행한다.

## Manual Ollama Smoke Flow

`scripts/run_ollama_world_config_smoke.py`는 orchestrator를 수동으로 검증하기 위한 도구다.

Dry-run 경로:

Natural-language prompt -> prompt package -> deterministic RAG context -> console output

Live 경로:

Natural-language prompt -> prompt package -> Ollama client -> JSON extraction -> `world_config` validation -> generation result

live 경로는 pytest나 harness check에서 호출하지 않는다. 수동 local verification 용도로만 사용한다.

## Attempt-Level Diagnostics

각 generation attempt는 다음 정보를 기록한다.

- raw output preview와 length
- JSON extraction 성공 여부
- 추출된 JSON key와 preview
- validation error와 summary
- repair prompt preview
- provider error code

`generatedPayload`는 JSON extraction과 `world_config` validation을 모두 통과한 뒤에만 채워진다.

provider timeout과 connection error는 validation failure보다 먼저 분류한다. 모든 attempt가 JSON extraction 전에 실패하면 최종 error는 `ollama_timeout` 같은 provider error code를 사용하고 validation status는 `skipped`로 남긴다.

## Ollama Provider Flow

`provider=ollama`이면 orchestrator는 factory를 통해 `OllamaLlmClient`를 호출할 수 있다.

raw Ollama `message.content`는 JSON extraction과 `world_config` validation을 거친 뒤에만 `generatedPayload`로 사용된다.

repair attempt는 같은 provider를 재사용한다.

## Provider Chain Policy

provider chain은 OpenAI first, Ollama fallback second 구조다.

`provider=openai`이면 orchestrator는 `OpenAILlmClient`를 호출한다. OpenAI가 fallback 대상 provider error로 실패하거나 repair 후에도 valid payload를 만들지 못하면 provider policy가 같은 요청을 Ollama로 전달할 수 있다. fallback 여부는 `fallbackTrace`에 기록한다.

fallback 대상에는 OpenAI API key 없음, timeout, rate limit, HTTP error, invalid response, missing content, repair 이후 validation failure가 포함된다. 모호하거나 지원하지 않는 사용자 요청은 fallback을 유발하지 않는다.

## API Endpoint Status

orchestrator는 다음 endpoint를 통해 접근할 수 있다.

```text
POST /api/v1/generation/world-config
```

이 endpoint의 기본 provider는 `disabled`이므로, 외부 LLM 호출 없이 명확한 failed result를 반환한다.

OpenAI와 Ollama provider client는 통합되어 있다. automated tests와 harness checks는 여전히 실제 provider call을 수행하지 않는다.

## 1. Purpose

orchestrator의 목적은 자연어 요청에서 검증된 World Config generation result까지의 내부 흐름을 연결하는 것이다.

## 2. Current Scope

- prompt package 생성
- deterministic RAG context 포함
- LLM client abstraction 호출
- disabled provider failure handling
- JSON extraction utility
- `world_config` contract validation
- validation error 기반 repair-loop 구조

## 3. Not Implemented Yet

이 섹션은 초기 설계 시점의 기록이다. 현재 코드 기준으로 OpenAI/Ollama provider client와 FastAPI generation endpoint는 구현되어 있다. 아직 완료되지 않은 항목은 다음과 같다.

- direct UE5 integration
- sample JSON fixture generation
- vector DB 또는 embedding index

## 4. Generation Flow

Natural Language Request
-> Prompt Package
-> LLM Client
-> Raw Content
-> JSON Extraction
-> Contract Validation
-> Repair Loop
-> Generation Result

## 5. Disabled Provider Behavior

provider가 `disabled`이면 orchestrator는 `error.code = provider_disabled`인 failed generation result를 반환한다.

이 모드에서는 World Config payload를 생성하지 않는다.

## 6. Later Steps

- UE team 검증 결과를 받아 최신 EpisodeSetup / DeliveryBotSetup / RunQueue 중심으로 handoff 흐름을 정리한다.
- provider별 live smoke는 수동 절차로만 수행하고 automated checks에서는 실제 호출을 피한다.
- vector DB와 embedding index는 source review가 정리된 뒤 별도 단계에서 검토한다.

## Scenario Repair Loop

schema validation은 통과했지만 scenario reflection이 실패하면 orchestrator는 `scenario_repair` prompt를 생성한다. repair prompt는 schema-valid JSON을 유지하면서 누락된 semantic requirement만 추가하도록 요청한다. 최종 성공은 schema validation과 scenario reflection validation을 모두 통과해야 한다.
