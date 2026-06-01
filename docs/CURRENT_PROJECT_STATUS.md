# 현재 프로젝트 상태

## 1. 완료된 작업

* Policy document registry
* Confirmed policy card generation
* Policy RAG chunking
* Deterministic RAG retrieval
* JSON Schema / Pydantic models
* JSON contract validation
* Natural language input plan
* OpenAI provider
* Ollama fallback provider
* WorldConfig generation orchestrator
* Scenario intent extraction
* Scenario reflection
* Scenario post-processing
* UE5 handoff endpoint
* WorldConfig to EpisodeSpec adapter
* EpisodeSpec validator
* EpisodeSpec scenario reflection
* OpenAI-first EpisodeSpec handoff smoke
* environmentSampling 기반 EpisodeSpec handoff smoke
* guide-aligned route midpoint EpisodeSpec live smoke
* 맵 생성 데이터 근거 문서
* UE team handoff package
* 루트 README 프로젝트 entry point

## 2. 현재 end-to-end 흐름

Natural Language -> WorldConfig -> Validation -> Scenario Reflection -> Post-processing -> EpisodeSpec -> EpisodeSpec validation -> EpisodeSpec scenario reflection -> UE handoff

environmentSampling이 활성화된 요청은 seed/scenarioType/fixedParameters 기반 numeric constraints를 prompt와 deterministic post-processing에 연결한다. 최근 smoke에서는 `sidewalkWidthCm=120`, `obstacleBlockingRatio=0.6`, `timeLimitSec=60`이 EpisodeSpec까지 반영됐다.

맵 생성 데이터 근거는 사용자 자연어, scenario intent, environmentSampling, UE EpisodeSpec guide, deterministic placement rule을 기준으로 정리했다. 법령/인증 RAG는 좌표 생성 근거가 아니라 안전 정책 근거로 사용한다.

## 3. 현재 제한 사항

* vector DB와 embedding index는 아직 구현하지 않았습니다.
* source document RAG는 아직 구현하지 않았습니다.
* 실제 UE actor spawn은 UE 팀 확인이 필요합니다.
* Kickboard는 UE에서 `obstacle.kickboard`가 확정될 때까지 임시 prop mapping을 사용합니다.
* sample JSON과 fixture 파일은 의도적으로 자동 생성하지 않습니다.

## 4. 현재 검증 상태

* Harness: `PASS_WITH_WARNING`
* pytest: `381 passed, 1 warning`
* warning 사유: 기존 manual-review/partial-source 상태가 설계상 남아 있습니다.
* OpenAI-first UE handoff smoke는 `providerUsed=openai`, `fallbackUsed=false`, `episodeValidationPassed=true`, `episodeScenarioReflectionPassed=true`, `ueCompilerReadiness=true` 상태입니다.
* environmentSampling EpisodeSpec handoff smoke는 `responseFormat=episode_spec`, `sidewalkWidthM=1.2`, `staticObstacleBlockingRatio=0.6`, `run.time_limit_s=60.0` 상태입니다.
* guide midpoint smoke는 `penaltiesFieldAbsent=true`, `propertiesAreShallow=true`, `propIdInCatalog=true`, `obstacleNearRouteMidpoint=true` 상태입니다.
