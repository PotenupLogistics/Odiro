# World Config Generation Orchestrator

The orchestrator now applies deterministic scenario post-processing before falling back to scenario repair prompts. The current order is:

Natural Language Request -> Prompt Package -> LLM Client -> Raw Content -> JSON Extraction -> Contract Validation -> Scenario Reflection -> Scenario Post-Processing -> Contract Validation -> Scenario Reflection -> Repair Loop -> Generation Result

If post-processing resolves the semantic gaps, `generatedPayload` is returned with `scenarioPostProcessing` diagnostics and patch records. If it does not, the existing scenario repair prompt flow remains the next step.

The UE5 handoff service consumes the final generation result. It passes `generatedPayload` through contract validation again before exposing it as `worldConfig`.

## Manual Ollama Smoke Flow

`scripts/run_ollama_world_config_smoke.py` exercises the orchestrator manually.

Dry-run path:

Natural-language prompt -> prompt package -> deterministic RAG context -> console output

Live path:

Natural-language prompt -> prompt package -> Ollama client -> JSON extraction -> `world_config` validation -> generation result

The live path is not called from pytest or harness checks. It is reserved for manual local verification.

## Attempt-Level Diagnostics

Each generation attempt records:

- raw output preview and length
- JSON extraction success
- extracted JSON keys and preview
- validation errors and summary
- repair prompt preview
- provider error code

`generatedPayload` is populated only after JSON extraction and `world_config` validation pass.

Provider timeout and connection errors are classified before validation failures. If all attempts fail before JSON extraction, the final error uses the provider error code such as `ollama_timeout` and validation status remains `skipped`.

## Ollama Provider Flow

When `provider=ollama`, the orchestrator can call `OllamaLlmClient` through the factory.

The raw Ollama `message.content` is passed through JSON extraction and `world_config` validation before `generatedPayload` is populated.

Repair attempts reuse the same provider.

## Provider Chain Policy

The provider chain is OpenAI first, Ollama fallback second.

When `provider=openai`, the orchestrator calls `OpenAILlmClient`. If OpenAI fails with an eligible provider error or cannot produce a valid payload after repair, the provider policy may route the same request to Ollama. Fallback is recorded in `fallbackTrace`.

Eligible fallback cases include missing OpenAI API key, timeout, rate limit, HTTP error, invalid response, missing content, and validation failure after repair. Ambiguous or unsupported user requests do not trigger fallback.

## API Endpoint Status

The orchestrator is now reachable through:

```text
POST /api/v1/generation/world-config
```

The endpoint defaults to `provider=disabled`, so it returns a clear failed result without external LLM calls.

OpenAI and Ollama provider clients are integrated. Automated tests and harness checks still avoid real provider calls.

## 1. Purpose

The orchestrator connects the internal flow from natural-language request to validated World Config generation result.

## 2. Current Scope

- Prompt package generation
- Deterministic RAG context inclusion
- LLM client abstraction call
- Disabled provider failure handling
- JSON extraction utility
- `world_config` contract validation
- Repair-loop structure based on validation errors

## 3. Not Implemented Yet

- External LLM API calls
- FastAPI generation endpoint
- Direct UE5 integration
- Sample JSON fixture generation
- Vector DB or embedding index

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

When the provider is `disabled`, the orchestrator returns a failed generation result with `error.code = provider_disabled`.

No World Config payload is generated in this mode.

## 6. Later Steps

- Add a FastAPI generation endpoint.
- Implement explicit OpenAI, Gemini, Ollama, or custom provider clients.
- Connect UE5 handoff only after validation succeeds.

## Scenario Repair Loop

When schema validation passes but scenario reflection fails, the orchestrator now creates a `scenario_repair` prompt. The repair prompt preserves the schema-valid JSON and asks the model to add only missing semantic requirements. Final success requires both schema validation and scenario reflection validation.
