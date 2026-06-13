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
- RunQueue model, service, export
- OpenAI-first / Ollama fallback provider chain
- local `WorldConfig` generation via Ollama provider

## Policy RAG

- 한국 법/인증/운행 기준 및 연구 자료 기반 policy source registry
- KOR-003 수동 검토 기반 confirmed policy knowledge card 9개
- confirmed policy card 기반 policy RAG chunk 9개
- keyword/category/action/parameter/source 기반 deterministic policy RAG retrieval

## Contracts And Models

- Policy Config, World Config, Decision Request, Decision Response, Evaluation Spec, Run Result용 JSON Schema 6종
- JSON contract와 대응되는 Pydantic model
- JSON contract validation layer와 CLI
- 공유 실행 계약 spec: `contracts/specs`

## UE Integration

- 사용자용 `/api/v1/scenarios/generate` endpoint
- EpisodeSetup + DeliveryBotSetup execution pair 생성
- RunQueue export/API path
- environmentSampling 기반 numeric constraints의 실행 계약 반영
- UE team RunQueue/setup pair 전달 문서
- legacy `WorldConfig` -> `EpisodeSpec` adapter와 validator/archive tooling

Legacy `EpisodeSpec` 자료는 `Agents/docs/archive/previous_episode_spec` 아래에서 과거 구현 추적용으로만 유지합니다.
