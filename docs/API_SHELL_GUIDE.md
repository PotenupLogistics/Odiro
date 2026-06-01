# API Shell Guide

## OpenAI First Provider Endpoint

Use:

```text
POST /api/v1/generation/world-config?provider=openai

POST /api/v1/generation/world-config?provider=ollama

POST /api/v1/ue5/world-config/handoff

The UE5 handoff endpoint wraps a validated World Config with metadata, validation summary, scenario reflection, post-processing diagnostics, and warnings. It does not create sample JSON files and does not send data directly to UE5.
```

UE handoff response format:

- default: `responseFormat=episode_spec`
- UE test recommendation: `responseFormat=episode_spec`
- debugging recommendation: `responseFormat=both`
- AI internal inspection only: `responseFormat=world_config`

The response diagnostics include `effectiveResponseFormat`. `episodeSpec` may be `null` only when `responseFormat=world_config` or when validation fails.

Environment sampling can be enabled through `generationRequest.constraints.environmentSampling`.
When enabled, the server samples numeric parameters from `seed` and `scenarioType`, adds a `Numeric Environment Constraints` prompt section, and records only a numeric summary in diagnostics.
Do not use low/middle/high as JSON values.

OpenAI is the first provider when configured with `OPENAI_API_KEY`. Ollama is the fallback provider. The local Ollama server must be running for real fallback generation. Tests mock or dry-run these paths and do not call OpenAI or `localhost:11434`.

OpenAI calls use JSON Schema structured output guidance for WorldConfig responses.

## Generation Endpoint

The API shell now exposes:

```text
POST /api/v1/generation/world-config
```

Query parameter:

- `provider`: `disabled`, `openai`, `gemini`, `ollama`, or `custom`
- default: `disabled`

Current behavior:

- `provider=disabled` calls the internal orchestrator and returns `success=false` with `error.code=provider_disabled`.
- `provider=openai` calls the OpenAI client when an API key is configured; otherwise it can fall back to Ollama according to provider policy.
- `provider=ollama` calls the Ollama client.
- `provider=gemini` or `custom` remains unimplemented unless injected in tests.
- Automated tests and harness checks do not call real external LLM APIs.
- `generatedPayload` is populated only after JSON extraction and `world_config` validation pass.
- The response must not be handed off to UE5 unless validation passes.

## Orchestrator Status

The World Config generation orchestrator exists internally, but the API shell does not expose a full generation endpoint yet.

Current API behavior remains:

- prompt-package endpoint returns prompt package only
- contract validation endpoint validates submitted payloads
- no endpoint returns a generated World Config payload

## LLM Client Abstraction Status

The API shell currently returns a prompt package only.

The returned prompt package is intended for a later LLM client abstraction step. The current provider is `disabled`, so no external LLM call is made and no World Config payload is generated.

## 1. Purpose

This FastAPI shell exposes natural-language prompt package generation and JSON contract validation.

## 2. Current Endpoints

- `GET /health`
- `POST /api/v1/generation/world-config/prompt-package`
- `POST /api/v1/contracts/validate/{contract_type}`

## 3. Not Implemented Yet

- External LLM API calls
- Actual World Config JSON generation
- Direct UE5 integration
- Sample JSON fixture generation
- Vector DB or embedding index

## 4. Natural Language Flow

Natural Language Prompt
-> prompt-package endpoint
-> prompt builder
-> deterministic RAG context
-> prompt package response
-> later LLM client stage

## 5. Run Command

```bash
uv run uvicorn app.main:app --reload
```

## 6. Swagger

Open:

```text
http://127.0.0.1:8000/docs
```

The prompt-package endpoint returns prompts and retrieved policy context only. It does not call an LLM and does not create a World Config payload.

## Scenario Reflection

The generation endpoint response includes `scenarioReflection` when generation reaches schema-valid World Config validation. This separates JSON contract validity from semantic reflection of the user's natural-language scenario.
