# Capabilities

현재 Agents가 제공하는 기능과 주요 구현 범위.

## Generation Runtime

- Natural language prompt 기반 scenario generation API
- `WorldConfig` generation orchestrator
- prompt package, JSON extraction, validation, repair loop
- scenario intent extraction
- scenario reflection
- deterministic scenario post-processing
- `WorldConfig` -> EpisodeSetup adapter
- `WorldConfig` -> DeliveryBotSetup adapter
- Legacy RunQueue model, service, export tooling
- OpenAI-first provider configuration
- local development `WorldConfig` generation via Ollama provider
- rule-based result analysis fallback when optional LLM recommendation fails
- deterministic scenario generation fallback when optional LLM generation fails

## Policy RAG

- 한국 법/인증/운행 기준 및 연구 자료 기반 policy source registry
- KOR-003 수동 검토 기반 confirmed policy knowledge card 9개와 KOR-004 promoted candidate 기반 runtime 보강 card 2개
- confirmed policy card 기반 policy RAG chunk 17개
- keyword/category/action/parameter/source 기반 deterministic policy RAG retrieval

## Contracts And Models

- Policy Config, World Config, Decision Request, Decision Response, Evaluation Spec, Run Result용 JSON Schema 6종
- JSON contract와 대응되는 Pydantic model
- JSON contract validation layer와 CLI
- 공유 실행 계약 spec: `contracts/specs`

## UE Integration

- `/api/v1/scenarios/generate` removal notice
- `/api/v2/scenarios/generate` Project Scenario 생성
- `/api/v2/analysis/run` project/run 분석
- Legacy EpisodeSetup + DeliveryBotSetup + RunQueue export tooling
- environmentSampling 기반 numeric constraints의 실행 계약 반영
- UE team RunQueue/setup pair 전달 문서는 legacy reference
- legacy `WorldConfig` -> `EpisodeSpec` adapter와 validator/archive tooling

Legacy `EpisodeSpec` 자료는 `Agents/docs/archive/previous_episode_spec` 아래에서 과거 구현 추적용으로만 유지합니다.
