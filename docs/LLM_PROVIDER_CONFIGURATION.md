# LLM Provider Configuration

## Manual Ollama Smoke Configuration

The manual smoke runner reads the same settings as the application:

- `OLLAMA_BASE_URL`
- `OLLAMA_MODEL`
- `OLLAMA_TIMEOUT_SEC`
- `OLLAMA_MAX_TOKENS`
- `OLLAMA_TEMPERATURE`

CLI flags can override the model and base URL for one run:

```bash
uv run python scripts/run_ollama_world_config_smoke.py --prompt "장애물이 있는 보도" --model llama3.1:8b --base-url http://localhost:11434
```

The smoke runner writes no file unless `--report` is provided.

## OpenAI First Provider Chain

Current provider order:

- `LLM_PROVIDER=openai`
- `LLM_PROVIDER_CHAIN=openai,ollama`

OpenAI is the first WorldConfig generation provider. Ollama remains the fallback provider for missing API key, timeout, rate limit, HTTP error, invalid response, or validation failure after repair.

The OpenAI-first EpisodeSpec handoff smoke passed with `providerUsed=openai`, `fallbackUsed=false`, `episodeValidationPassed=true`, `episodeScenarioReflectionPassed=true`, and `ueCompilerReadiness=true`.

OpenAI settings:

- `OPENAI_API_KEY`
- `OPENAI_MODEL`
- `OPENAI_TIMEOUT_SEC`
- `OPENAI_MAX_TOKENS`
- `OPENAI_TEMPERATURE`
- `OPENAI_MAX_REPAIR_ATTEMPTS`
- `OPENAI_DAILY_REQUEST_LIMIT`

The real `.env` file is not created by this project. Users provide `OPENAI_API_KEY` locally when running live smoke.

## Ollama Implementation Status

Ollama provider client is implemented with `httpx` against the local `/api/chat` endpoint.

- Base URL comes from `OLLAMA_BASE_URL`.
- Model comes from `OLLAMA_MODEL`.
- Request uses `stream=false`.
- JSON mode uses `format=json`.
- `options.temperature` and `options.num_predict` are mapped from generation request settings.
- Automated tests use mock HTTP transport and do not call a real Ollama server.

OpenAI provider is implemented with `httpx` against the Responses API and uses JSON Schema structured output for WorldConfig responses.

## 1. Purpose

This document describes the configuration structure for using OpenAI and Ollama as replaceable LLM providers.

## 2. Provider Policy

- Development default is OpenAI first, Ollama fallback.
- OpenAI is used as the accuracy-first provider.
- Ollama is retained for local fallback and cost control.
- API keys are read from `.env`.
- The real `.env` file is not committed.

## 3. Cost Control Strategy

- Use OpenAI first for accuracy-sensitive WorldConfig generation.
- Allow Ollama fallback for limited OpenAI failure cases.
- Limit max tokens.
- Use low temperature.
- Limit repair attempts.
- Keep prompt context topK small.
- Configure a daily request limit.

## 4. Fallback Policy

- OpenAI unavailable can fall back to Ollama.
- Invalid response or validation repair failure can fall back to Ollama.
- Ambiguous user input does not fall back and should request clearer input.
- Missing OpenAI API key can fall back to Ollama.
- Daily request limit exhaustion can fall back to Ollama.

## 5. Environment Setup

Use `.env.example` as the template.

Do not commit a real `.env` file.

Automated tests and harness checks use mocks or dry-run paths and do not call real OpenAI or Ollama services.
