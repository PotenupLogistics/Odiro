# LLM Provider Configuration

## 1. 목적

이 문서는 OpenAI와 Ollama를 교체 가능한 LLM provider로 사용하기 위한 설정 구조를 정리한다. 현재 기본 방향은 OpenAI를 정확도 우선 provider로 사용하고, Ollama를 로컬 fallback provider로 유지하는 것이다.

## 2. Manual Ollama Smoke 설정

수동 Ollama smoke runner는 애플리케이션과 같은 설정 값을 읽는다.

* `OLLAMA_BASE_URL`
* `OLLAMA_MODEL`
* `OLLAMA_TIMEOUT_SEC`
* `OLLAMA_MAX_TOKENS`
* `OLLAMA_TEMPERATURE`

CLI flag로 단일 실행의 model과 base URL을 덮어쓸 수 있다.

```bash
uv run python scripts/run_ollama_world_config_smoke.py --prompt "장애물이 있는 보도" --model llama3.1:8b --base-url http://localhost:11434
```

`--report`를 지정하지 않으면 smoke runner는 파일을 생성하지 않는다.

## 3. OpenAI First Provider Chain

현재 provider 순서:

* `LLM_PROVIDER=openai`
* `LLM_PROVIDER_CHAIN=openai,ollama`

OpenAI는 WorldConfig generation의 1순위 provider다. Ollama는 API key 누락, timeout, rate limit, HTTP error, invalid response, repair 이후 validation failure 같은 제한된 상황에서 fallback으로 유지한다.

OpenAI-first EpisodeSpec handoff smoke는 `providerUsed=openai`, `fallbackUsed=false`, `episodeValidationPassed=true`, `episodeScenarioReflectionPassed=true`, `ueCompilerReadiness=true`로 통과했다. Legacy setup_pair smoke도 별도 문서에서 관리한다.

OpenAI 설정:

* `OPENAI_API_KEY`
* `OPENAI_MODEL`
* `OPENAI_TIMEOUT_SEC`
* `OPENAI_MAX_TOKENS`
* `OPENAI_TEMPERATURE`
* `OPENAI_MAX_REPAIR_ATTEMPTS`
* `OPENAI_DAILY_REQUEST_LIMIT`

실제 `.env` 파일은 project가 생성하지 않는다. Live smoke를 실행하는 사용자가 로컬에서 `OPENAI_API_KEY`를 제공한다.

## 4. Provider 구현 상태

Ollama provider client는 로컬 `/api/chat` endpoint를 대상으로 `httpx`로 구현되어 있다.

* Base URL은 `OLLAMA_BASE_URL`에서 읽는다.
* Model은 `OLLAMA_MODEL`에서 읽는다.
* 요청은 `stream=false`를 사용한다.
* JSON mode는 `format=json`을 사용한다.
* `options.temperature`와 `options.num_predict`는 generation request 설정에서 매핑한다.
* 자동 테스트는 mock HTTP transport를 사용하며 실제 Ollama server를 호출하지 않는다.

OpenAI provider client는 Responses API를 대상으로 `httpx`로 구현되어 있고, WorldConfig 응답에 JSON Schema structured output을 사용한다.

## 5. 비용 제어 전략

* 정확도가 중요한 WorldConfig generation에는 OpenAI first를 사용한다.
* 제한된 실패 상황에서는 Ollama fallback을 허용한다.
* max tokens를 제한한다.
* 낮은 temperature를 사용한다.
* repair attempt 횟수를 제한한다.
* prompt context topK를 작게 유지한다.
* daily request limit을 설정한다.

## 6. 자동 테스트 정책

자동 tests와 harness checks는 mock 또는 dry-run 경로만 사용한다. 실제 OpenAI 또는 Ollama service를 호출하지 않는다.
