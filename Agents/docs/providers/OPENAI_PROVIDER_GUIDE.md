# OpenAI Provider Guide

## 1. 목적

OpenAI를 기본 WorldConfig generation provider로 사용하는 방법을 설명한다. Ollama는 로컬 개발/수동 smoke용 선택 provider이며, v2 endpoint의 안정성 fallback은 자체 rule-based/deterministic 경로다.

## 2. .env 설정

* `LLM_PROVIDER=openai`
* `LLM_PROVIDER_CHAIN=openai`
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
* OpenAI 실패 시 v2 API가 rule-based 또는 deterministic fallback으로 안전하게 degraded되는지 확인
* 하네스/pytest에서 실제 API 호출 금지

## 4. fallback 정책

* `/api/v2/analysis/run`은 OpenAI key 없음, timeout, rate limit, invalid response 같은 LLM 실패를 API 500으로 올리지 않고 rule-based fallback 응답으로 처리한다.
* `/api/v2/scenarios/generate`는 같은 LLM 실패를 API 500으로 올리지 않고 deterministic scenario generation으로 처리한다.
* Ollama는 로컬 개발에서 명시적으로 선택할 수 있지만, v2 endpoint의 기본 실패 처리 경로가 아니다.
* 사용자 입력이 모호한 경우에는 fallback하지 않고 추가 입력 요구

## 5. smoke 실행

Dry-run은 OpenAI를 호출하지 않고 prompt package만 확인하는 경로다.

```powershell
uv run python scripts/run_openai_world_config_smoke.py --prompt "좁은 보도에서 공유 킥보드가 로봇 경로를 막고, 오른쪽에서 보행자가 횡단하는 상황을 만들어줘." --dry-run
```

Live smoke는 실제 OpenAI 호출을 수행하므로 사용자가 명시적으로 요청한 경우에만 실행한다.

```powershell
uv run python scripts/run_openai_world_config_smoke.py --prompt "좁은 보도에서 공유 킥보드가 로봇 경로를 막고, 오른쪽에서 보행자가 횡단하는 상황을 만들어줘." --print-payload --report harness/reports/manual_openai_world_config_smoke.json
```

Live smoke는 사용자가 실제 `.env`에 `OPENAI_API_KEY`를 넣은 뒤 수동으로만 실행한다.

Smoke report는 공통 report serialization helper를 통해 저장한다. API key, full `generatedPayload`, full `episodeSpec`, rawContent 전문은 report에 저장하지 않는다.

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

Ollama는 로컬 개발과 수동 provider 비교용으로 유지한다.
