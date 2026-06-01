# UE Team Handoff Package

## 1. 현재 AI Backend 상태

* 자연어 기반 `WorldConfig` 생성 가능
* `WorldConfig` schema validation 가능
* scenario reflection 가능
* scenario post-processing 가능
* `WorldConfig` -> `EpisodeSpec` 변환 가능
* `EpisodeSpec` validation 가능
* `EpisodeSpec` scenario reflection 가능
* OpenAI-first EpisodeSpec handoff smoke 통과
* Ollama fallback provider 유지

## 2. UE가 사용할 기본 endpoint

* `POST /api/v1/ue5/world-config/handoff?provider=openai&responseFormat=episode_spec`
* `POST /api/v1/ue5/world-config/handoff?provider=openai&responseFormat=both`

## 3. UE가 우선 사용할 format

* 기본 권장: `responseFormat=episode_spec`
* 디버깅용: `responseFormat=both`
* fallback: Ollama provider

## 4. EpisodeSpec root 구조

* `schema`
* `version`
* `scenario_id`
* `map_id`
* `units`
* `run`
* `ground_model`
* `paths`
* `actors`

## 5. UE 생성 대상

* `ground_model.regions`: 지면/보도 영역
* `actors.robot`: 배달 로봇
* `actors.static_obstacles`: 정적 장애물
* `paths`: 보행자 spline path
* `actors.pedestrians`: 보행자 actor
* `run.time_limit_s`: 실행 제한 시간

## 6. 현재 controlled scenario 반영 상태

* Kickboard semantic 반영됨: `semantic_type`
* path blocking 반영됨: `blocking_ratio`
* Crossing pedestrian 반영됨: `semantic_behavior`
* pedestrian path 연결됨: `pedestrian_crossing`
* `providerUsed=openai`, `fallbackUsed=false`

## 7. UE 확인 요청

* `obstacle.kickboard` prop_id 추가 가능 여부
* 현재 `obstacle.road_barrier_01` 임시 매핑 시각 허용 여부
* `EpisodeSandbox`에 `BP_DeliveryBot_GridBoundsActor`가 있는지
* `actors.pedestrians[].properties.semantic_behavior` 처리 방식
* `paths[].role=pedestrian_crossing` 처리 방식
* `blocking_ratio`를 UE debug/log/metric에 사용할지 여부
