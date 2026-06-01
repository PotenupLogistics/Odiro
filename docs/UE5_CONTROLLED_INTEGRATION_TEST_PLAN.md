# UE5 Controlled Integration Test Plan

## 1. 목적

AI Backend handoff response를 UE5가 읽어 최소 환경을 생성할 수 있는지 검증한다.

## 2. 테스트 범위

* API 호출
* handoff response 수신
* `worldConfig` 파싱
* map 생성
* robot spawn 생성
* goal marker 생성
* Kickboard obstacle 생성
* crossing pedestrian 생성
* runtime 설정 적용

## 3. 성공 기준

* handoff response `success=true`
* `worldConfig` 존재
* map plane 생성
* robot actor 생성
* goal marker 생성
* Kickboard actor 생성
* pedestrian actor 생성
* pedestrian `behavior=Crossing` 반영
* `runtime.maxDurationSec` 적용
* UE5 로그에 `worldId`와 `scenarioId` 기록

## 4. 실패 기준

* `success=false` handoff를 UE5가 실행함
* `worldConfig=null`인데 map 생성을 시도함
* 필수 필드 누락
* obstacle 또는 pedestrian 배열 무시
* 좌표 단위 불일치
* cm 단위를 m로 잘못 해석

## 5. 테스트 순서

1. AI Backend 서버 실행
2. UE5에서 handoff endpoint 호출
3. `response.success` 확인
4. `worldConfig` 파싱
5. map 생성
6. actor 생성
7. runtime 실행
8. event log 확인

## 6. 주의

본 테스트는 프로젝트 내부 시뮬레이션 검증이다. 실제 로봇 안전 보장이나 공식 인증 준수를 의미하지 않는다.
# EpisodeSpec adapter check

통합 테스트 전 `WorldConfig -> EpisodeSpec` 변환을 먼저 검증한다.

1. in-memory `WorldConfig`를 생성한다.
2. adapter로 `EpisodeSpec`을 만든다.
3. cm 값이 meter로 변환됐는지 확인한다.
4. pedestrian path와 actor 참조가 맞는지 확인한다.
5. Kickboard가 임시 prop으로 매핑되고 warning이 남는지 확인한다.
6. UE 쪽에서 `EpisodeSpec` parser가 동일 필드를 읽는지 확인한다.

## API smoke precheck

UE controlled integration 전에 `POST /api/v1/ue5/world-config/handoff?provider=ollama&responseFormat=episode_spec`를 수동 실행해 `EpisodeSpec` root structure와 `episodeValidation` 결과를 확인한다.

## Scenario reflection precheck

UE controlled integration 전 `scripts/run_ue5_episode_spec_controlled_smoke.py`로 controlled scenario smoke를 실행한다. `episodeScenarioReflectionPassed`, `hasKickboardSemantic`, `hasBlockingRatio`, `hasCrossingPedestrian`, `pedestrianPathLinked`가 모두 true인지 확인한다.

## UE checklist

* [ ] `responseFormat=episode_spec`로 호출
* [ ] `response.success=true` 확인
* [ ] `episodeSpec.schema == episode_actor_spawn_mvp`
* [ ] `units.distance == m`
* [ ] `ground_model.regions[0]`로 보도 생성
* [ ] `actors.robot` 생성
* [ ] `actors.static_obstacles[0].properties.semantic_type == Kickboard` 확인
* [ ] `actors.static_obstacles[0].properties.blocking_ratio` 확인
* [ ] `paths[0].role == pedestrian_crossing` 확인
* [ ] `actors.pedestrians[0].path_id`가 `paths[0].path_id`와 연결됨
* [ ] `actors.pedestrians[0].properties.semantic_behavior == Crossing` 확인
* [ ] `run.time_limit_s` 적용
* [ ] `success=false` 응답은 실행하지 않음
