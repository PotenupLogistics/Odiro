# Handoff Release Notes

이 문서는 현재 AI backend handoff와 RunQueue generation 준비 상태를 정리한다. 최신 UE 실행 계약은 `EpisodeSetup`, `DeliveryBotSetup`, `RunQueue` 문서를 기준으로 한다. legacy EpisodeSpec 문서는 `docs/archive/previous_episode_spec/` 아래에 보관한다.

## 1. Release 범위

이번 release 기준으로 다음 흐름이 준비되어 있다.

* 자연어 기반 `WorldConfig` 생성 흐름
* Policy RAG retrieval
* JSON Schema와 Pydantic validation
* Scenario intent extraction
* Scenario reflection
* Scenario post-processing
* OpenAI-first provider 경로
* Ollama fallback provider 경로
* UE handoff endpoint
* legacy `WorldConfig` -> `EpisodeSpec` adapter
* 최신 `WorldConfig` -> `EpisodeSetup` + `DeliveryBotSetup` adapter
* `responseFormat=setup_pair` handoff 응답
* EpisodeSetup + DeliveryBotSetup pair validation
* 사용자용 `POST /api/v1/scenarios/generate`
* UE 계약 그대로의 null-free RunQueue export
* DeliveryBotSetup default catalog와 deterministic variation policy
* `narrow_sidewalk_obstacle_ahead_blocked_path` policy comparison queue

## 2. 현재 API 구분

`POST /api/v1/scenarios/generate`는 사용자용 entrypoint다. 사용자는 자연어 `prompt`만 입력하고, backend는 내부적으로 WorldConfig, EpisodeSetup, DeliveryBotSetup, RunQueue를 생성한다. 응답은 wrapper 없는 RunQueue JSON이다.

`POST /api/v1/ue5/world-config/handoff`는 UE handoff와 내부 검증용 endpoint다. legacy `episode_spec`, 최신 `setup_pair`, debugging용 `both`, AI 내부 inspection용 `world_config` 응답을 유지한다.

## 3. 검증 결과

* pytest: `500 passed, 1 warning`
* harness: `PASS_WITH_WARNING`
* warning 사유: manual review/source review 계열 확인 항목이 설계상 warning으로 남아 있다.
* OpenAI-first controlled smoke:
  * `providerUsed=openai`
  * `fallbackUsed=false`
  * handoff validation 통과
* setup pair live smoke:
  * `responseFormat=setup_pair`
  * `effectiveResponseFormat=setup_pair`
  * EpisodeSetup + DeliveryBotSetup pair 생성
  * `episodeSetupValidationPassed=true`
  * `deliveryBotSetupValidationPassed=true`
  * `setupPairTraceExists=true`
* environmentSampling smoke:
  * `sidewalkWidthCm=120`
  * `obstacleBlockingRatio=0.6`
  * `timeLimitSec=60`
  * EpisodeSpec `sidewalkWidthM=1.2`
  * EpisodeSpec `run.time_limit_s=60.0`
* policy comparison OpenAI live smoke:
  * OpenAI live 호출 1회
  * Ollama/fallback/fake/dry-run 미사용
  * 동일 EpisodeSetup 참조
  * DeliveryBotSetup만 5개 policy별 변경
  * null-free RunQueue 확인

## 4. RunQueue export 정책

RunQueue JSON에는 `success`, `responseFormat`, `diagnostics`, `setupPairs`, `validation`, `trace` 같은 wrapper/진단 필드를 넣지 않는다.

검증/trace/summary 정보는 local ignored report JSON에만 저장한다. `data/run_queue_exports/` 아래 runtime export는 git commit 대상이 아니다.

## 5. 보안과 저장 정책

다음 항목은 repository에 저장하지 않는다.

* API key 또는 인증값
* raw OpenAI/Ollama response 전문
* rawContent 저장물
* full WorldConfig
* full EpisodeSpec
* 실제 runtime export JSON
* sample JSON
* fixture JSON
* vector DB 또는 embedding index

## 6. 아직 포함하지 않은 항목

* UE C++ / Blueprint 구현
* 실제 UE actor spawn 확인
* Run Result receive API
* Evaluation scoring
* EvaluationReport 기반 result analysis agent
* DOE matrix generation
* batch scenario generation

## 7. 다음 확인 항목

* UE 팀에서 `EpisodeSetup` + `DeliveryBotSetup` pair compile 확인
* UE 팀에서 RunQueue JSON ingestion 확인
* policy comparison run 결과 수집 방식 합의
* EvaluationReport 계약 기반 분석 흐름 설계
