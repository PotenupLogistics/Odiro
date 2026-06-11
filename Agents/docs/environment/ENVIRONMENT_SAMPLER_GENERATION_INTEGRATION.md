# Environment Sampler Generation Integration

## 1. 목적

seed 기반 numeric environment parameters를 WorldConfig generation constraints에 연결한다.

## 2. 흐름

seed + scenarioType
→ EnvironmentParameterSet
→ Numeric Environment Constraints
→ WorldConfig prompt
→ Scenario post-processing
→ EpisodeSpec handoff

## 3. 사용 예

요청의 `generationRequest.constraints.environmentSampling`에 다음 값을 넣는다.

* `enabled=true`
* `seed`
* `scenarioType`
* `fixedParameters`

`fixedParameters`는 `sidewalkWidthCm=120`, `obstacleBlockingRatio=0.6`, `timeLimitSec=60`처럼 numeric allowed value만 사용한다.
문서에는 요청 구조만 설명하며 sample JSON 파일을 생성하지 않는다.

## 4. 원칙

* low/middle/high는 JSON 값으로 사용하지 않음
* numeric constraints가 자연어보다 우선
* same seed + same scenarioType = same numeric parameters
* sampler는 DOE matrix가 아님
* DOE/batch는 후속 단계

## 5. Prompt/Post-processing 연결

Prompt builder는 `Numeric Environment Constraints` 섹션을 추가한다.
Scenario post-processing은 sampled `sidewalkWidthCm`, `obstacleBlockingRatio`, `timeLimitSec`, pedestrian speed/count를 schema-valid field에 반영한다.
Handoff diagnostics와 response summary는 sampled numeric parameter summary만 포함하며 full payload를 저장하지 않는다.

`environmentSampling`이 활성화된 요청에서는 LLM 출력이 schema-valid이고 자연어 reflection을 통과하더라도 deterministic post-processing을 실행해 sampled/fixed numeric 값을 우선 적용한다.
`sidewalkWidthCm`, `obstacleBlockingRatio`, `timeLimitSec`가 WorldConfig와 EpisodeSpec에 반영되지 않으면 warning이 아니라 reflection/validation 실패로 처리한다.

## 6. EpisodeSpec handoff 검증

장애물 또는 경로 차단 요구가 있으면 EpisodeSpec에는 `actors.static_obstacles`와 `properties.blocking_ratio`가 있어야 한다.
`environmentSampling.obstacleBlockingRatio`가 0보다 크면 static obstacle과 blocking ratio는 필수 요구사항으로 본다.
`environmentSampling.sidewalkWidthCm=120`이면 `ground_model.regions[].shape.size_m[1]`는 `1.2`로 변환되어야 한다.

## 7. Smoke 결과

최근 OpenAI-first handoff smoke에서 `responseFormat=episode_spec` 기준으로 다음 값이 확인됐다.

* `sidewalkWidthCm=120` -> EpisodeSpec `sidewalkWidthM=1.2`
* `obstacleBlockingRatio=0.6` -> EpisodeSpec `staticObstacleBlockingRatio=0.6`
* `timeLimitSec=60` -> EpisodeSpec `run.time_limit_s=60.0`
* `episodeValidationPassed=true`
* `episodeScenarioReflectionPassed=true`
* `ueCompilerReadiness=true`

이 연결은 단일 요청용 numeric constraints 연결이며, DOE matrix와 batch scenario generation은 UE 단일 케이스 검증 후 진행한다.
