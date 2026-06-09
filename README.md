# Proto-AI

Proto-AI는 UE5 배달 로봇 시뮬레이션을 위한 AI 백엔드 프로젝트입니다. 자연어 시나리오 입력을 받아 정책 RAG context를 구성하고, `WorldConfig`를 생성/검증한 뒤 scenario reflection과 scenario post-processing을 거쳐 최신 UE 실행 계약인 EpisodeSetup + DeliveryBotSetup pair와 RunQueue export/API 경로로 변환합니다. legacy UE 실행 계약인 `EpisodeSpec` 관련 자료는 archive와 export tooling 맥락으로만 유지합니다. 최신 계약 문서는 `docs/ue_contracts/` 아래에 있고 전환 기준은 [UE Contract Migration Plan](docs/architecture/UE_CONTRACT_MIGRATION_PLAN.md)에 정리했습니다.

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
-> EpisodeSetup Adapter / DeliveryBotSetup Adapter
-> RunQueue Generation / Export
-> UE5 Handoff Archive
```

최신 UE 계약 기준에서는 Scenario는 추상적인 상황 유형이고, Episode는 seed/파라미터/배치가 적용된 단일 실행 인스턴스입니다. UE Runner의 실제 입력 단위는 EpisodeSetup + DeliveryBotSetup execution pair이며, 용어 기준은 [Scenario / Episode Terminology](docs/architecture/SCENARIO_EPISODE_TERMINOLOGY.md)에 정리했습니다.

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
* 사용자용 scenario generation API
* `WorldConfig` -> EpisodeSetup + DeliveryBotSetup adapter
* RunQueue model/service/export
* legacy `WorldConfig` -> `EpisodeSpec` adapter와 validator/archive tooling
* OpenAI-first / Ollama fallback provider chain
* environmentSampling 기반 numeric constraints의 UE 계약 반영
* UE team RunQueue/setup pair 전달 문서
* 최신 UE 계약 기반 setup pair live smoke와 UE 전달 문서

## 현재 검증 상태

현재 프로젝트 검증 상태:

* `uv run pytest` -> `500 passed, 1 warning`
* `uv run python -m harness.checks.check_all` -> `PASS_WITH_WARNING`

현재 harness warning은 일부 source document와 manual review workflow가 아직 완료되지 않았기 때문에 남아 있습니다. Legacy UE handoff route는 제거되었고, 사용자용 scenario generation과 RunQueue export check를 기준으로 검증합니다.

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

Setup pair live smoke 상태:

* `providerUsed=openai`
* `effectiveResponseFormat=setup_pair`
* `handoffSuccess=true`
* `episodeSetupExists=true`
* `deliveryBotSetupExists=true`
* `episodeSetupValidationPassed=true`
* `deliveryBotSetupValidationPassed=true`
* `setupPairTraceExists=true`
* `EpisodeSetup`: robot `[0.0, 0.0]` -> `[8.0, 0.0]`, obstacle `[4.0, 0.0]`
* `DeliveryBotSetup`: `stop_distance_m=1.2`, `slow_down_distance_m=3.5`

사용자용 scenario generation smoke 상태:

* `POST /api/v1/scenarios/generate`
* 입력은 자연어 `prompt`를 필수로 받고, 선택적으로 `episode_count`를 허용
* 성공 응답은 wrapper field 없는 RunQueue JSON
* OpenAI live smoke 1회 검증 완료
* `run count=5`
* EpisodeSetup / DeliveryBotSetup / RunQueue 모두 null-free
* EpisodeSetup artifact에는 서버 기본 `RobotProfile`이 additive root field `robot_profile`로 포함됨
* narrow sidewalk policy comparison 구조에서는 모든 run이 동일 EpisodeSetup을 참조하고, DeliveryBotSetup만 5개 policy별로 달라짐

## 주요 API

현재 FastAPI endpoint:

* `GET /health`
* `POST /api/v1/scenarios/generate`
* `POST /api/v1/scenarios/generate-drive`
* `POST /api/v1/scenarios/generate-artifacts` (debug/test artifact zip download)

UE 연동 기본 권장 endpoint:

```text
POST /api/v1/scenarios/generate
```

Legacy `/api/v1/ue5/world-config/handoff` endpoint는 현재 FastAPI/OpenAPI에서 제거되었습니다. 이전 `responseFormat=episode_spec`, `responseFormat=setup_pair`, `responseFormat=both` 기반 handoff 설명은 archive 문서와 CLI tooling 참고용입니다.
`/api/v1/generation/world-config`, `/api/v1/generation/world-config/prompt-package`, `/api/v1/contracts/validate/{contract_type}`도 public API에서 제거되었습니다. 관련 기능은 service/helper 함수와 CLI 테스트 대상으로 유지합니다.

사용자용 scenario 생성 endpoint:

```text
POST /api/v1/scenarios/generate
```

이 endpoint는 사용자의 자연어 `prompt`만 입력받습니다. 사용자가 EpisodeSetup / DeliveryBotSetup / RunQueue JSON을 직접 작성하는 구조가 아니며, JSON은 AI와 backend가 내부적으로 생성한 UE 실행 산출물입니다. 정상 응답은 최상위 `schema`, `version`, `runs`만 포함하는 RunQueue JSON입니다.
`episode_count`를 함께 보내면 생성할 episode/run 개수를 지정할 수 있고, 생략하면 `SCENARIO_EPISODE_DEFAULT_COUNT`를 사용합니다. 요청값은 1 이상 `SCENARIO_EPISODE_MAX_COUNT` 이하의 strict integer여야 합니다.

로봇 실측 크기는 request body로 받지 않고 서버 기본 `RobotProfile`로 주입합니다. 기본 profile은 W/D/H `0.44m / 1.00m / 0.64m`, `footprint_shape=box`, `safety_margin_m=0.2`, `min_passable_width_m=0.84`이며, 생성된 `EpisodeSetup_*.json`의 root `robot_profile` field로 export됩니다. backend는 이 값을 보도 폭, 장애물 gap, robot spawn/goal 여유 검증에 사용합니다. UE는 우선 이 additive field를 무시해도 기존 `actors.robot`, `ground_model`, obstacle 구조가 바뀌지 않아야 하며, 실제 collision box와 크기 일치 여부는 UE 쪽 확인이 필요합니다.

Google Drive artifact 업로드 endpoint:

```text
POST /api/v1/scenarios/generate-drive
```

이 endpoint는 `/api/v1/scenarios/generate`와 같은 `ScenarioGenerateRequest` body를 받습니다. backend는 기존 scenario generation flow로 `EpisodeRunQueue`, `EpisodeSetup`, `DeliveryBotSetup` JSON 산출물을 만든 뒤 `.env`에 설정된 Google Drive 폴더로 업로드하고, 응답으로 파일 본문이나 zip 대신 Drive metadata JSON을 반환합니다. 응답의 `run_queue_file`은 UE가 Google Drive 동기화 폴더에서 먼저 읽어야 하는 RunQueue 파일명입니다.

Google Drive 방식에서는 AI 서버가 지정 폴더에 JSON artifact를 업로드하고, 언리얼은 로컬 Google Drive 동기화 폴더에서 `run_queue_file`이 실제로 생겼는지 확인한 뒤 실행하는 것이 안전합니다. 동기화 지연이 있을 수 있으므로 UE 쪽에서는 파일 존재 확인과 짧은 retry/polling을 두는 흐름을 권장합니다.

Drive 인증 방식은 `GOOGLE_DRIVE_AUTH_MODE`로 선택합니다. `service_account` mode는 Shared Drive 업로드에 권장하며 `GOOGLE_DRIVE_SERVICE_ACCOUNT_FILE`을 사용합니다. 사용자의 My Drive에 만든 공유 폴더는 service account에 저장소 quota가 없어 `Service Accounts do not have storage quota`로 실패할 수 있으므로 `oauth` mode를 사용합니다. `oauth` mode는 `GOOGLE_DRIVE_OAUTH_CLIENT_FILE`에서 OAuth client 설정을 읽고 `GOOGLE_DRIVE_OAUTH_TOKEN_FILE`에 사용자 token을 저장/재사용합니다.

`GOOGLE_DRIVE_BACKUP_BEFORE_UPLOAD=true`이면 새 artifact 업로드 전에 대상 Drive 폴더의 기존 직계 항목을 백업 폴더로 이동합니다. 백업 폴더는 `GOOGLE_DRIVE_BACKUP_FOLDER_ID`가 있으면 해당 ID를 우선 사용하고, 없으면 대상 폴더의 직계 child 중 `GOOGLE_DRIVE_BACKUP_FOLDER_NAME` 값(기본값 `백업`)과 일치하는 폴더를 찾습니다. 백업 폴더 자체와 백업 폴더 안의 기존 파일은 이동하지 않으며, 백업 폴더를 찾지 못하거나 이름이 중복되면 업로드를 중단합니다.

Drive folder id, auth mode, credentials/token 경로는 요청 body로 받지 않고 서버 설정값만 사용합니다. `secrets/oauth_client.json`, `secrets/google_drive_token.json`, `secrets/credentials.json`은 Git에 올리면 안 되며, OAuth client secret, OAuth token, service account private key, credentials 내용은 로그에 남기면 안 됩니다.

테스트용 artifact 다운로드 endpoint:

```text
POST /api/v1/scenarios/generate-artifacts
```

이 endpoint는 `/api/v1/scenarios/generate`와 같은 request body를 받지만 JSON body 대신 `scenario_artifacts.zip` 파일을 반환합니다. zip에는 `response.json`, `EpisodeRunQueue_*.json`, `EpisodeSetup_*.json`, `DeliveryBotSetup_*.json`이 포함됩니다. UE/통신 테스트에서 생성 산출물을 직접 확인하기 위한 debug/test endpoint입니다. Google Drive 기반 전달을 사용할 때는 `/api/v1/scenarios/generate-drive`가 권장 경로이며, zip endpoint는 legacy/debug 용도로만 유지합니다.

## UE 연동 상태

AI 내부 계약은 계속 `WorldConfig`로 유지합니다. 현재 사용자용 UE 연동 API는 `/api/v1/scenarios/generate`이며, backend가 내부적으로 EpisodeSetup + DeliveryBotSetup pair와 RunQueue를 생성/export합니다. 최신 UE 계약 문서는 [docs/ue_contracts](docs/ue_contracts/) 아래에 있습니다.

setup pair smoke, environmentSampling smoke, policy comparison smoke 결과는 release note에 요약했습니다. UE 전달 package는 별도 문서로 유지합니다.

* [Handoff Release Notes](docs/handoff/HANDOFF_RELEASE_NOTES.md)
* [UE Setup Pair Handoff Package](docs/handoff/UE_SETUP_PAIR_HANDOFF_PACKAGE.md)
* [UE Team Message Draft](docs/handoff/UE_TEAM_MESSAGE_DRAFT.md)

`EpisodeSpec`은 이전 UE MVP JSON guide에 맞춘 legacy payload 형식입니다. 현재 UE object catalog에 `obstacle.kickboard`가 없으므로 legacy adapter는 임시로 `obstacle.road_barrier_01`을 사용합니다. 단, 원래 시나리오 의미는 `semantic_type="Kickboard"`로 보존합니다.

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

* [UE Integration Handoff Index](docs/handoff/UE_INTEGRATION_HANDOFF_INDEX.md)
* [UE Team Handoff Package](docs/archive/previous_episode_spec/UE_TEAM_HANDOFF_PACKAGE.md)
* [UE5 Endpoint Usage For UE Team](docs/handoff/UE5_ENDPOINT_USAGE_FOR_UE_TEAM.md)
* [UE EpisodeSpec JSON Guide](docs/archive/previous_episode_spec/UE_EPISODE_SPEC_JSON_GUIDE.md)
* [UE5 EpisodeSpec Handoff Summary](docs/archive/previous_episode_spec/UE5_EPISODE_SPEC_HANDOFF_SUMMARY.md)
* [UE5 EpisodeSpec Controlled Smoke Result](docs/archive/previous_episode_spec/UE5_EPISODE_SPEC_CONTROLLED_SMOKE_RESULT.md)
* [UE Handoff Delivery Manifest](docs/handoff/UE_HANDOFF_DELIVERY_MANIFEST.md)
* [Handoff Release Notes](docs/handoff/HANDOFF_RELEASE_NOTES.md)
* [OpenAI Provider Guide](docs/providers/OPENAI_PROVIDER_GUIDE.md)
* [Environment Parameter Spec](docs/environment/ENVIRONMENT_PARAMETER_SPEC.md)
* [Environment Sampler Design](docs/environment/ENVIRONMENT_SAMPLER_DESIGN.md)
* [UE Setup Pair Handoff Package](docs/handoff/UE_SETUP_PAIR_HANDOFF_PACKAGE.md)
* [UE Team Message Draft](docs/handoff/UE_TEAM_MESSAGE_DRAFT.md)
* [Map Generation Data Sources](docs/architecture/MAP_GENERATION_DATA_SOURCES.md)
* [Map Generation Trace](docs/architecture/MAP_GENERATION_TRACE.md)
* [Research Alignment](docs/research/RESEARCH_ALIGNMENT.md)
* [Scenario / Episode Terminology](docs/architecture/SCENARIO_EPISODE_TERMINOLOGY.md)
* [UE Contract Migration Plan](docs/architecture/UE_CONTRACT_MIGRATION_PLAN.md)
* [Latest UE Contract Docs](docs/ue_contracts/)
* [EpisodeSetup Contract](docs/ue_contracts/EPISODE_SETUP_JSON.md)
* [DeliveryBotSetup Contract](docs/ue_contracts/DELIVERY_BOT_SETUP_JSON.md)
* [RunQueue Contract](docs/ue_contracts/RUN_QUEUE_JSON.md)
* [EvaluationReport Contract](docs/ue_contracts/EVALUATION_REPORT_JSON.md)
* [Policy Decision Contract](docs/policy_server/POLICY_DECISION_JSON_GUIDE.md)
* [Legacy EpisodeSpec Archive](docs/archive/previous_episode_spec/)
* [UE AI Integration Issues](docs/handoff/UE_AI_INTEGRATION_ISSUES.md)

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
* `responseFormat=setup_pair` live smoke는 통과했으며, fine-tuning candidate는 `data/fine_tuning_candidates/`에 로컬 저장하고 git commit 대상에서 제외합니다.
* EvaluationReport 기반 결과 분석과 Result Analysis Agent 구현은 현재 담당 범위가 아니며 별도 단계에서 진행합니다.

## 다음 액션

1. UE 팀에 setup pair handoff package를 전달합니다.
2. UE 팀에서 EpisodeSetup + DeliveryBotSetup 단일 pair compile과 실행을 확인합니다.
3. UE 팀에서 parser integration, spawn mapping, route injection을 확인합니다.
4. 최종 Kickboard prop ID를 확정합니다.
5. UE 피드백에 따라 adapter를 조정합니다.
6. 이후 Run Result API와 scoring을 별도 단계에서 진행합니다.
7. DOE / scenario matrix / batch generation은 UE 단일 케이스 검증 후 진행합니다.
