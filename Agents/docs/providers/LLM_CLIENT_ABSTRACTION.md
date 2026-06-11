# LLM Client Abstraction

## 1. 목적

LLM client abstraction은 WorldConfig generation에서 provider를 교체할 수 있도록 만든 공통 계층이다. Orchestrator는 provider별 구현을 직접 알지 않고 이 abstraction을 통해 generation을 요청한다.

## 2. Ollama Client

`app/services/llm_ollama_client.py`는 Ollama provider client를 구현한다.

* `httpx`를 사용한다.
* Ollama Python SDK를 import하지 않는다.
* 로컬 Ollama server가 없으면 live call은 실패할 수 있지만, 자동 tests/harness는 실제 server를 호출하지 않는다.

## 3. OpenAI Client

`app/services/llm_openai_client.py`는 OpenAI provider client를 구현한다.

* Responses API를 `httpx`로 호출한다.
* 요청에는 WorldConfig JSON Schema 기반 structured output guidance가 포함된다.
* `OPENAI_API_KEY`가 비어 있으면 network call 없이 `openai_api_key_missing`을 반환한다.

## 4. Provider Configuration

Provider 설정은 `app/core/settings.py`에서 로드한다.

* 기본 provider chain: `openai, ollama`
* OpenAI는 accuracy-first provider다.
* Ollama는 fallback provider다.
* API key는 `.env`에서 읽으며 source code에 hardcode하지 않는다.
* 자동 tests는 mock transport 또는 dry-run 경로를 사용한다.

## 5. Orchestrator Integration

WorldConfig generation orchestrator는 이 abstraction을 호출한다.

현재 동작:

* `disabled` provider는 실패 generation response를 반환한다.
* 테스트에서는 fake client를 주입해 orchestration을 검증할 수 있다.
* `openai`와 `ollama` provider client는 구현되어 있다.
* Provider output은 JSON extraction과 contract validation을 통과해야 `generatedPayload`가 될 수 있다.

## 6. Safety Principles

* API key를 hardcode하지 않는다.
* 외부 API 호출은 명시적으로 설정된 provider 경로에서만 수행한다.
* LLM output은 JSON contract validation을 통과해야 한다.
* UE5 handoff 전에 validation이 통과해야 한다.
