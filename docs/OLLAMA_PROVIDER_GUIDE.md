# Ollama Provider Guide

## Manual Live Smoke Runner

Use `scripts/run_ollama_world_config_smoke.py` for manual Ollama checks before wiring more automation.

Dry-run does not call Ollama:

```bash
uv run python scripts/run_ollama_world_config_smoke.py --prompt "좁은 보도에서 킥보드가 경로를 막고 보행자가 횡단" --dry-run
```

Manual live smoke requires a running local Ollama server:

```bash
uv run python scripts/run_ollama_world_config_smoke.py --prompt "좁은 보도에서 킥보드가 경로를 막고 보행자가 횡단"
```

Automated tests and harness checks do not call a real Ollama server. They only verify help and dry-run behavior.

## Failure Diagnostics

If live smoke returns `validation_failed`, rerun with detailed diagnostics:

```bash
uv run python scripts/run_ollama_world_config_smoke.py --prompt "..." --model llama3.1:8b --include-extracted-json --report harness/reports/manual_ollama_world_config_smoke_llama_detailed.json
```

Use `docs/OLLAMA_FAILURE_DIAGNOSTICS.md` to interpret `attemptsDetail`, `validationErrorSummary`, `extractionSummary`, and `recommendedNextAction`.

## 1. Purpose

This guide explains how to use local Ollama as the World Config generation provider.

## 2. Environment Settings

Use `.env.example` as the template:

- `OLLAMA_BASE_URL=http://localhost:11434`
- `OLLAMA_MODEL=llama3.1:8b`
- `LLM_PROVIDER=openai`
- `LLM_PROVIDER_CHAIN=openai,ollama`

Do not commit a real `.env` file.

## 3. Prerequisites

- Ollama must be running locally for real generation.
- The configured model must be pulled before use.
- OpenAI is now the first provider. Ollama remains the fallback provider.

## 4. API Usage

Endpoint:

```text
POST /api/v1/generation/world-config?provider=ollama
```

Request body uses `WorldConfigGenerationRequest`.

If generation succeeds and the extracted JSON passes validation, the response includes `generatedPayload`.

## 5. Failure Handling

- `ollama_connection_failed`: check whether the Ollama server is running.
- `ollama_timeout`: check model load time or timeout settings.
- `ollama_http_error`: inspect Ollama HTTP response status.
- `ollama_invalid_response`: response body was not valid JSON.
- `ollama_missing_content`: response did not include `message.content`.

Invalid JSON or validation failure can enter the repair loop. OpenAI fallback remains a later provider implementation step.

## 6. Testing Policy

Automated tests use `httpx.MockTransport` or fake clients. They do not call `localhost:11434`.
