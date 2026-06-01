# Proto-AI

Proto-AI는 UE5 배달 로봇 시뮬레이션을 위한 AI 백엔드 프로젝트입니다. 자연어 시나리오 입력을 받아 정책 RAG context를 구성하고, `WorldConfig`를 생성/검증한 뒤 scenario reflection과 scenario post-processing을 거쳐 UE 실행 계약인 `EpisodeSpec`으로 변환합니다. 이후 `EpisodeSpec` validation과 `EpisodeSpec` scenario reflection을 통과한 결과를 UE5 handoff 대상으로 사용합니다.

이 프로젝트는 프로젝트 내부 시뮬레이션 검증용입니다. 실제 운영 안전성이나 외부 기준 충족을 주장하지 않습니다.

상세 문서 인덱스는 [docs/README.md](docs/README.md)를 참고합니다.

## 현재 파이프라인

```text
Natural Language Prompt
-> Scenario Intent Extraction
-> Policy RAG Retrieval
-> WorldConfig Generation
-> JSON Schema / Pydantic Validation
-> Scenario Reflection
-> Scenario Post-Processing
-> EpisodeSpec Adapter
-> EpisodeSpec Validation
-> EpisodeSpec Scenario Reflection
-> UE5 Handoff
```

## 완료된 주요 기능

* 한국 법/인증/운행 기준 및 연구 자료 기반 policy source registry
* KOR-003 수동 검토 기반 confirmed policy knowledge card 9개
* confirmed policy card 기반 policy RAG chunk 9개
* keyword/category/action/parameter/source 기반 deterministic policy RAG retrieval
* Policy Config, World Config, Decision Request, Decision Response, Evaluation Spec, Run Result용 JSON Schema 6종
* JSON 계약과 대응되는 Pydantic 모델
* JSON contract validation layer와 CLI
* 로컬 `WorldConfig` 생성을 위한 Ollama provider 경로
* prompt package, JSON extraction, validation, repair loop를 포함한 WorldConfig generation orchestrator
* scenario intent extraction, scenario reflection, deterministic scenario post-processing
* UE5 handoff endpoint
* `WorldConfig` -> `EpisodeSpec` adapter
* `EpisodeSpec` validator와 scenario reflection
* OpenAI-first / Ollama fallback provider chain
* environmentSampling 기반 numeric constraints의 EpisodeSpec handoff 반영
* UE team handoff package와 integration 문서

## 현재 검증 상태

현재 프로젝트 검증 상태:

* `uv run pytest` -> `362 passed, 1 warning`
* `uv run python -m harness.checks.check_all` -> `PASS_WITH_WARNING`

현재 harness warning은 일부 source document와 manual review workflow가 아직 완료되지 않았기 때문에 남아 있습니다. UE handoff 관련 check는 통과 상태입니다.

Controlled smoke 상태:

* `providerUsed=openai`
* `fallbackUsed=false`
* `effectiveResponseFormat=episode_spec`
* `handoffSuccess=true`
* `episodeValidationPassed=true`
* `episodeScenarioReflectionPassed=true`
* `ueCompilerReadiness=true`
* `environmentSampling.enabled=true`
* `sidewalkWidthCm=120`, `obstacleBlockingRatio=0.6`, `timeLimitSec=60`

## 주요 API

현재 FastAPI endpoint:

* `GET /health`
* `POST /api/v1/generation/world-config/prompt-package`
* `POST /api/v1/generation/world-config`
* `POST /api/v1/ue5/world-config/handoff`
* `POST /api/v1/contracts/validate/{contract_type}`

UE 연동 기본 권장 endpoint:

```text
POST /api/v1/ue5/world-config/handoff?provider=openai&responseFormat=episode_spec
```

OpenAI-first 권장 endpoint:

```text
POST /api/v1/ue5/world-config/handoff?provider=openai&responseFormat=episode_spec
```

디버그용 endpoint option:

```text
POST /api/v1/ue5/world-config/handoff?provider=openai&responseFormat=both
```

`/api/v1/ue5/world-config/handoff`의 기본 `responseFormat`은 `episode_spec`입니다.
`responseFormat=world_config`는 AI 내부 구조 확인용이며 이 경우 `episodeSpec`은 `null`일 수 있습니다.

## UE handoff 상태

AI 내부 계약은 계속 `WorldConfig`로 유지합니다. UE 실행 계약은 `EpisodeSpec`입니다.

`EpisodeSpec`은 UE MVP JSON guide에 맞춘 UE 연동 권장 payload 형식입니다. 현재 UE object catalog에 `obstacle.kickboard`가 없으므로 adapter는 임시로 `obstacle.road_barrier_01`을 사용합니다. 단, 원래 시나리오 의미는 `semantic_type="Kickboard"`로 보존합니다.

생성된 `EpisodeSpec`에는 다음 정보가 포함됩니다.

* 경로 차단 장애물의 `blocking_ratio`
* 보행자 횡단 행동의 `pedestrian_crossing`
* 횡단 의미를 나타내는 `semantic_behavior="Crossing"`

## 실행 방법

전체 하네스 실행:

```powershell
uv run python -m harness.checks.check_all
```

테스트 실행:

```powershell
uv run pytest
```

API 서버 실행:

```powershell
uv run uvicorn app.main:app --reload
```

Ollama 사용 전제:

```powershell
ollama serve
ollama pull llama3.1:8b
```

UE handoff export CLI 도움말:

```powershell
uv run python scripts/export_ue5_handoff_payload.py --help
```

EpisodeSpec export 예:

```powershell
uv run python scripts/export_ue5_handoff_payload.py --prompt "좁은 보도에서 공유 킥보드가 로봇 경로를 막고, 오른쪽에서 보행자가 횡단하는 상황을 만들어줘." --provider openai --format episode_spec
```

`--out`을 지정하지 않으면 export CLI는 파일을 생성하지 않습니다.

## 주요 문서

* [UE Integration Handoff Index](docs/UE_INTEGRATION_HANDOFF_INDEX.md)
* [UE Team Handoff Package](docs/UE_TEAM_HANDOFF_PACKAGE.md)
* [UE5 Endpoint Usage For UE Team](docs/UE5_ENDPOINT_USAGE_FOR_UE_TEAM.md)
* [UE EpisodeSpec JSON Guide](docs/UE_EPISODE_SPEC_JSON_GUIDE.md)
* [UE5 EpisodeSpec Handoff Summary](docs/UE5_EPISODE_SPEC_HANDOFF_SUMMARY.md)
* [UE5 EpisodeSpec Controlled Smoke Result](docs/UE5_EPISODE_SPEC_CONTROLLED_SMOKE_RESULT.md)
* [UE Handoff Delivery Manifest](docs/UE_HANDOFF_DELIVERY_MANIFEST.md)
* [Handoff Release Notes](docs/HANDOFF_RELEASE_NOTES.md)
* [Handoff Readiness Checklist](docs/HANDOFF_READINESS_CHECKLIST.md)
* [Harness Warning Explanation](docs/HARNESS_WARNING_EXPLANATION.md)
* [OpenAI Provider Guide](docs/OPENAI_PROVIDER_GUIDE.md)
* [OpenAI-first Handoff Result](docs/OPENAI_FIRST_HANDOFF_RESULT.md)
* [Environment Parameter Spec](docs/ENVIRONMENT_PARAMETER_SPEC.md)
* [Environment Sampler Design](docs/ENVIRONMENT_SAMPLER_DESIGN.md)
* [Environment Sampling Handoff Result](docs/ENVIRONMENT_SAMPLING_HANDOFF_RESULT.md)
* [Map Generation Data Sources](docs/MAP_GENERATION_DATA_SOURCES.md)
* [UE AI Integration Issues](docs/UE_AI_INTEGRATION_ISSUES.md)
* [Next Actions](docs/NEXT_ACTIONS.md)
* [Current Project Status](docs/CURRENT_PROJECT_STATUS.md)

## 현재 제한 사항

* OpenAI first / Ollama fallback provider chain은 구현되어 있으며, 최근 OpenAI EpisodeSpec handoff smoke가 통과했습니다.
* vector DB와 embedding index는 아직 구현하지 않았습니다.
* source document RAG는 아직 구현하지 않았습니다.
* 실제 UE actor spawn은 UE 팀 검증이 필요합니다.
* `obstacle.kickboard` prop ID는 UE 측 확인이 필요합니다.
* sample JSON과 fixture 파일은 의도적으로 자동 생성하지 않습니다.
* 환경 파라미터 sampler는 `WorldConfig`를 직접 생성하지 않고, 후속 generation constraints로 사용할 numeric parameter set만 생성합니다.
* `environmentSampling.enabled=true`이면 seed 기반 numeric parameter set을 `Numeric Environment Constraints`로 WorldConfig generation prompt와 deterministic post-processing에 연결합니다.
* environmentSampling 기반 단일 EpisodeSpec handoff smoke는 통과했으며, DOE matrix와 batch scenario generation은 아직 후속 단계입니다.

## 다음 액션

1. UE 팀에 handoff package를 전달합니다.
2. UE 팀에서 parser integration, spawn mapping, route injection을 확인합니다.
3. 최종 Kickboard prop ID를 확정합니다.
4. UE 피드백에 따라 adapter를 조정합니다.
5. 이후 Run Result API와 scoring을 별도 단계에서 진행합니다.
6. DOE / scenario matrix / batch generation은 UE 단일 케이스 검증 후 진행합니다.
