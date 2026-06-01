# LLM Client Abstraction

## Ollama Client

`app/services/llm_ollama_client.py` implements the Ollama provider client.

It uses `httpx` and does not import the Ollama Python SDK.

## OpenAI Client

`app/services/llm_openai_client.py` implements the OpenAI provider client.

It uses `httpx` against the Responses API. The request includes the WorldConfig JSON Schema as structured output guidance. If `OPENAI_API_KEY` is empty, the client returns `openai_api_key_missing` without making a network call.

## Provider Configuration

Provider configuration is loaded through `app/core/settings.py`.

- Default provider chain: `openai, ollama`
- OpenAI is the accuracy-first provider.
- Ollama is the fallback provider.
- API keys are read from `.env`; no API key is hardcoded.
- Automated tests use mock transports or dry-run paths and do not call real OpenAI or Ollama services.

## Orchestrator Integration

The World Config generation orchestrator calls this abstraction.

Current behavior:

- `disabled` provider returns a failed generation response.
- Fake clients can be injected in tests to verify orchestration.
- Real provider clients remain unimplemented.
- Provider output must pass JSON extraction and contract validation before becoming `generatedPayload`.

## 1. Purpose

The LLM client abstraction defines a replaceable provider layer for future World Config generation.

## 2. Current Stage

The `disabled`, `openai`, and `ollama` providers are implemented. `gemini` and `custom` remain unimplemented factory targets unless a test injects a fake client.

## 3. Planned Providers

- OpenAI
- Gemini
- Ollama
- Custom provider

## 4. Safety Principles

- Do not hardcode API keys.
- External API calls must be added only after explicit provider configuration.
- LLM output must pass JSON contract validation before it can be used.
- Validation must pass before any UE5 handoff.

## 5. Later Steps

- Implement provider-specific clients.
- Add a World Config generation orchestrator.
- Connect validation and repair-loop behavior.
- Extend the API only after provider behavior is verified.
