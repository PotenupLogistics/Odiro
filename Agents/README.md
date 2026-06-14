# Proto-AI / Odiro AI Services

주행 로봇 시뮬레이션을 위한 AI 백엔드입니다. 프로젝트 내부 시뮬레이션 검증용이며 실제 운영 안전성이나 출시 기준을 충족하지 않습니다.

## 역할

- Natural language prompt를 받아 policy RAG context를 구성합니다.
- `WorldConfig`를 생성/검증한 뒤 EpisodeSetup + DeliveryBotSetup pair와 RunQueue로 변환합니다.
- 사용자용 scenario generation API와 result analysis API를 제공합니다.
- Legacy `EpisodeSpec` / UE5 Handoff Archive 문서는 과거 구현 추적용 archive로 유지합니다.

상세 기능과 상태:

- [Capabilities](docs/status/CAPABILITIES.md)
- [Verification Status](docs/status/VERIFICATION_STATUS.md)
- [Next Actions](docs/status/NEXT_ACTIONS.md)
- [Docs Index](docs/README.md)

## Pipeline

```text
Natural Language Prompt
-> Scenario Intent Extraction
-> Policy RAG Retrieval
-> WorldConfig Generation
-> Validation / Reflection / Post-Processing
-> EpisodeSetup + DeliveryBotSetup
-> RunQueue Generation / Export
```

Scenario/Episode 용어 기준은 [Scenario / Episode Terminology](docs/architecture/SCENARIO_EPISODE_TERMINOLOGY.md)를 따릅니다. 공유 실행 계약 spec은 [contracts/specs](../contracts/specs/) 아래에 있습니다.

## Public API

- `GET /health`
- `POST /api/v1/scenarios/generate`
- `POST /api/v1/analysis/run`
- `POST /api/v2/scenarios/generate`
- `POST /api/v2/analysis/run`

`POST /api/v1/scenarios/generate`는 자연어 `prompt`를 필수로 받고 선택적으로 `episode_count`를 허용합니다. 성공 응답은 wrapper field 없는 RunQueue JSON입니다.
`POST /api/v1/scenarios/generate`는 자연어 `prompt`를 필수로 받고 선택적으로 `episode_count`를 허용합니다. 성공 응답은 wrapper field 없는 RunQueue JSON입니다. UE 연동 기본 권장 endpoint입니다.

v2 Agent API는 v1 실행 계약을 변경하지 않는 신규 경로입니다. `/api/v2/scenarios/generate`는 prompt만 받아 `scenario.template.json` 형태의 템플릿을 반환하고, 실행 개수, seed, scenario sample, RunQueue 생성은 담당하지 않습니다. `/api/v2/analysis/run`은 body 없음 또는 `{}`로 experiments root 전체를 분석합니다. 기본값은 deterministic/rule-based이며, `V2_AGENT_LLM_ENABLED=true`일 때만 optional LLM JSON mode를 사용합니다. 자세한 내용은 [v2 Agent API 문서](docs/api/V2_AGENT_APIS.md), [v2 Agent Architecture](docs/agents/V2_AGENT_ARCHITECTURE.md)를 참고합니다.

Legacy `/api/v1/ue5/world-config/handoff` endpoint는 현재 FastAPI/OpenAPI에서 제거되었습니다. 이전 `responseFormat=episode_spec`, `responseFormat=setup_pair`, `responseFormat=both` 기반 handoff 설명은 archive 문서와 CLI tooling 참고용입니다.

## Quick Start

```powershell
uv run python -m harness.checks.check_all
uv run pytest
uv run uvicorn app.main:app --reload
```

RAG store 변경 후:

```powershell
uv run python scripts/check_file_based_rag_readiness.py
uv run python scripts/validate_file_based_rag_store.py
uv run python scripts/validate_policy_chunk_candidates.py
```

Ollama 사용 전제:

```powershell
ollama serve
ollama pull llama3.1:8b
```

## Key Docs

- [Natural Language Generation Contract](docs/json_contracts/NATURAL_LANGUAGE_GENERATION_CONTRACT.md)
- [JSON Contracts](docs/json_contracts/JSON_CONTRACTS.md)
- [Policy Decision Contract](docs/policy_server/POLICY_DECISION_JSON_GUIDE.md)
- [Environment Parameter Spec](docs/environment/ENVIRONMENT_PARAMETER_SPEC.md)
- [UE Contract Migration Plan](docs/architecture/UE_CONTRACT_MIGRATION_PLAN.md)
- [Handoff Release Notes](docs/handoff/HANDOFF_RELEASE_NOTES.md)
- [UE Handoff Delivery Manifest](docs/handoff/UE_HANDOFF_DELIVERY_MANIFEST.md)
- [UE5 Endpoint Usage For UE Team](docs/handoff/UE5_ENDPOINT_USAGE_FOR_UE_TEAM.md)
- [Legacy EpisodeSpec Archive](docs/archive/previous_episode_spec/)
- [OpenAI Provider Guide](docs/providers/OPENAI_PROVIDER_GUIDE.md)
- [v2 Agent API 문서](docs/api/V2_AGENT_APIS.md)
- [v2 Agent Architecture](docs/agents/V2_AGENT_ARCHITECTURE.md)
- [v2 Agent Testing And Operations Guide](docs/development/V2_AGENT_TESTING_GUIDE.md)

## Legacy Notes

`EpisodeSpec`은 이전 UE MVP JSON guide에 맞춘 legacy payload 형식입니다. 현재 UE object catalog에 `obstacle.kickboard`가 없으므로 legacy adapter는 임시로 `obstacle.road_barrier_01`을 사용합니다. 원래 시나리오 의미는 `semantic_type="Kickboard"`로 보존합니다.

UE setup pair smoke, environmentSampling smoke, policy comparison smoke 결과는 [Verification Status](docs/status/VERIFICATION_STATUS.md)와 [Handoff Release Notes](docs/handoff/HANDOFF_RELEASE_NOTES.md)를 참고합니다.
RunQueue export CLI는 `--out`을 지정하지 않으면 파일을 생성하지 않습니다.
