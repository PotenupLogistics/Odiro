# OpenAI Provider Guide

## 1. 목적

OpenAI를 1순위 WorldConfig generation provider로 사용하고, Ollama를 fallback으로 사용하는 방법을 설명한다.

## 2. .env 설정

* `LLM_PROVIDER=openai`
* `LLM_PROVIDER_CHAIN=openai,ollama`
* `OPENAI_API_KEY=사용자가 직접 입력`
* `OPENAI_MODEL=gpt-4o-mini`
* `OPENAI_MAX_TOKENS=1200`
* `OPENAI_TEMPERATURE=0.1`

## 3. 비용 절감 전략

* maxTokens 제한
* temperature 낮게 유지
* maxRepairAttempts 1
* RAG context topK 제한
* daily request limit
* OpenAI 실패 시 Ollama fallback
* 하네스/pytest에서 실제 API 호출 금지

## 4. fallback 정책

* OpenAI key 없음, timeout, rate limit, invalid response, validation repair 실패 시 Ollama fallback 가능
* 사용자 입력이 모호한 경우에는 fallback하지 않고 추가 입력 요구

## 5. smoke 실행

Dry-run:

```powershell
uv run python scripts/run_openai_world_config_smoke.py --prompt "좁은 보도에서 공유 킥보드가 로봇 경로를 막고, 오른쪽에서 보행자가 횡단하는 상황을 만들어줘." --dry-run
```

Live smoke:

```powershell
uv run python scripts/run_openai_world_config_smoke.py --prompt "좁은 보도에서 공유 킥보드가 로봇 경로를 막고, 오른쪽에서 보행자가 횡단하는 상황을 만들어줘." --print-payload --report harness/reports/manual_openai_world_config_smoke.json
```

Live smoke는 사용자가 실제 `.env`에 `OPENAI_API_KEY`를 넣은 뒤 수동으로만 실행한다.

Smoke report는 공통 report serialization helper를 통해 저장한다. API key, full `generatedPayload`, full `episodeSpec`은 report에 저장하지 않는다.

## 6. Handoff 결과

최근 OpenAI-first EpisodeSpec handoff smoke 결과:

* `providerUsed=openai`
* `fallbackUsed=false`
* `schemaValidationPassed=true`
* `scenarioReflectionPassed=true`
* `episodeSpecConvertible=true`
* `episodeValidationPassed=true`
* `episodeScenarioReflectionPassed=true`
* `ueCompilerReadiness=true`

Ollama는 fallback provider로 유지한다.
