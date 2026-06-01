# Generic Obstacle Smoke Reporting

## 1. 목적

generic obstacle handoff smoke에서 API 응답을 요약하는 기준을 정의한다.

## 2. 확인 항목

* `worldConfig.obstacles`
* `obstacles[].blockingRatio`
* `map.sidewalkWidthCm`
* `episodeSpec.actors.static_obstacles`
* `episodeSpec` static obstacle `properties.blocking_ratio`
* no pedestrian condition
* `scenarioReflection.checkedRequirements`

## 3. 저장 원칙

* full payload 저장 금지
* summary만 저장
* API key 저장 금지

## 4. Summary helper

`app/utils/handoff_response_summary.py`의 `summarize_handoff_response()`를 사용한다.
이 helper는 `worldConfig`와 `episodeSpec` 전체를 report에 넣지 않고, obstacle, blocking ratio, sidewalk width, validation, scenario reflection, post-processing patch type만 요약한다.
