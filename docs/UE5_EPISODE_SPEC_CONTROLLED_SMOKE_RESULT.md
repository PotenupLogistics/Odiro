# UE5 EpisodeSpec Controlled Smoke Result

## 1. Smoke 결과

* `handoffSuccess`: true
* `providerUsed`: openai
* `fallbackUsed`: false
* `episodeValidationPassed`: true
* `episodeScenarioReflectionPassed`: true
* `staticObstacleCount`: 1
* `hasKickboardSemantic`: true
* `hasBlockingRatio`: true
* `pedestrianCount`: 1
* `pathCount`: 1
* `pedestrianPathLinked`: true
* `hasCrossingPedestrian`: true
* `ueCompilerReadiness`: true

## 2. 의미

이 결과는 AI Backend가 OpenAI-first provider chain에서 UE가 읽을 `EpisodeSpec` 구조와 시나리오 의미 조건을 함께 만족하는 응답을 생성했음을 의미한다. Ollama는 fallback provider로 유지된다.

## 3. 아직 UE에서 확인해야 하는 것

* 실제 actor spawn
* mesh catalog
* path movement
* robot route injection
* grid bounds
* API key와 full WorldConfig/full EpisodeSpec은 smoke report에 저장하지 않는다.
