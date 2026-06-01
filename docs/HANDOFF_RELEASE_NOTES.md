# Handoff Release Notes

## 1. Release 목적

이 문서는 AI 백엔드가 UE `EpisodeSpec` handoff를 위한 1차 준비 상태에 도달했음을 기록합니다.

## 2. 완료 범위

* 자연어 기반 `WorldConfig` 생성 흐름
* Policy RAG retrieval
* JSON Schema와 Pydantic validation
* Scenario reflection
* Scenario post-processing
* OpenAI-first provider 경로
* Ollama fallback provider 경로
* UE handoff endpoint
* `WorldConfig` to `EpisodeSpec` adapter
* `EpisodeSpec` validation
* `EpisodeSpec` scenario reflection
* environmentSampling numeric constraints handoff
* UE team handoff package
* Root README entry point

## 3. 검증 결과

* pytest: `362 passed, 1 warning`
* harness: `PASS_WITH_WARNING`
* OpenAI-first controlled smoke:
  * `providerUsed=openai`
  * `fallbackUsed=false`
  * `handoffSuccess=true`
  * `episodeValidationPassed=true`
  * `episodeScenarioReflectionPassed=true`
  * `ueCompilerReadiness=true`
* environmentSampling EpisodeSpec smoke:
  * `responseFormat=episode_spec`
  * `sidewalkWidthCm=120 -> sidewalkWidthM=1.2`
  * `obstacleBlockingRatio=0.6 -> staticObstacleBlockingRatio=0.6`
  * `timeLimitSec=60 -> run.time_limit_s=60.0`

## 4. 아직 포함하지 않은 항목

* Vector DB / embedding index
* Source document RAG
* UE C++ / Blueprint 구현
* Run Result receive API
* Evaluation scoring
* DOE matrix / batch scenario generation
* Sample JSON 또는 fixture 파일

## 5. 다음 단계

* UE controlled integration
* UE feedback 반영
* Run Result API 설계
* Evaluation scoring 설계
* DOE / batch generation은 UE 단일 케이스 검증 후 진행

## Report Serialization

* Smoke report는 공통 serialization helper를 사용해 Pydantic model, warning/error object, enum, datetime 값을 JSON-safe 값으로 변환한다.
* API key와 full generatedPayload/full episodeSpec은 report에 저장하지 않는다.
* Evaluation scoring 설계
* Ollama는 fallback provider로 유지한다.

이 release는 프로젝트 내부 시뮬레이션 handoff package이며, 실제 UE actor spawn은 UE 팀 확인이 필요합니다.
