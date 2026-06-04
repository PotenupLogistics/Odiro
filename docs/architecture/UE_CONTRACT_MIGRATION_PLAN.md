# UE Contract Migration Plan

## 1. 배경

기존 Proto-AI는 WorldConfig -> EpisodeSpec -> UE handoff 구조였으나, 최신 UE 계약은 EpisodeSetup + DeliveryBotSetup pair 구조다.

## 2. 최신 UE 계약 문서

* `docs/ue_contracts/EPISODE_JSON_GUIDE.md`
* `docs/ue_contracts/EPISODE_SETUP_JSON.md`
* `docs/ue_contracts/DELIVERY_BOT_SETUP_JSON.md`
* `docs/ue_contracts/RUN_QUEUE_JSON.md`
* `docs/ue_contracts/EVALUATION_REPORT_JSON.md`

## 3. 용어 기준

* Scenario: 추상적인 상황 유형
* Episode: 실제 한 번 실행되는 구체 인스턴스
* EpisodeSetup: Episode의 맵/배치 JSON
* DeliveryBotSetup: 로봇 주행/센서/정책 튜닝 JSON
* Pair: EpisodeSetup + DeliveryBotSetup

## 4. 기존 구조

* WorldConfig
* EpisodeSpec
* `responseFormat=episode_spec`
* generationTrace

## 5. 신규 구조

* WorldConfig는 AI 내부 중간 표현으로 유지한다.
* WorldConfig -> EpisodeSetup 변환을 추가한다.
* WorldConfig / policy params -> DeliveryBotSetup 변환을 추가한다.
* UE에는 EpisodeSetup + DeliveryBotSetup pair를 전달한다.

## 6. 핵심 차이

| 기존 EpisodeSpec | 신규 EpisodeSetup |
| --- | --- |
| units 포함 | units 출력하지 않음 |
| location_m | xy_m |
| rotation_deg | yaw_deg |
| transform object | 사용하지 않음 |
| goal_m | route.goal_xy_m |
| scale | 사용하지 않음 |

| 구분 | EpisodeSetup | DeliveryBotSetup |
| --- | --- | --- |
| 로봇 배치 | 담당 | 담당하지 않음 |
| 로봇 목적지 | 담당 | 담당하지 않음 |
| 지면/장애물/보행자 배치 | 담당 | 담당하지 않음 |
| 주행 속도 튜닝 | 담당하지 않음 | 담당 |
| 경로 추종 튜닝 | 담당하지 않음 | 담당 |
| 라이다 반응 튜닝 | 담당하지 않음 | 담당 |

## 7. 코드 변경 계획

1차 내부 구현 파일:

* `app/models/episode_setup.py`
* `app/models/delivery_bot_setup.py`
* `app/models/run_queue.py`
* `app/services/world_config_to_episode_setup_adapter.py`
* `app/services/world_config_to_delivery_bot_setup_adapter.py`
* `app/services/episode_setup_validator.py`
* `app/services/delivery_bot_setup_validator.py`

현재 구현은 EpisodeSetup / DeliveryBotSetup 모델, validator, WorldConfig 변환 adapter, setup pair trace helper, handoff `responseFormat=setup_pair` 응답까지 포함한다. 기존 `responseFormat=episode_spec`은 legacy 경로로 유지한다.

`responseFormat=setup_pair` live smoke는 EpisodeSetup + DeliveryBotSetup pair 생성과 validation 통과를 확인했다. 검증된 candidate full JSON은 `data/fine_tuning_candidates/` 아래 로컬 ignored path에 보관하며 repository에 포함하지 않는다.

RunQueue 단계는 public handoff API 응답을 바꾸지 않고 script/export 중심으로 추가한다. 자연어 시나리오 1개에서 기본 5개 EpisodeSetup + DeliveryBotSetup pair를 deterministic variation으로 만들고, UE 계약 그대로의 `EpisodeRunQueue_<scenario>.json`을 생성한다. RunQueue JSON에는 `success`, `responseFormat`, `diagnostics`, `setupPairs`, `episodeSetup`, `deliveryBotSetup`, `validation`, `trace` 같은 wrapper/진단 필드를 넣지 않는다. 내부 validation/trace/summary는 `data/run_queue_exports/` 아래 별도 local report로 분리한다.

UE JSON 출력은 null-free 정책을 따른다. optional field는 값이 없으면 생략하며, DeliveryBotSetup에서 생략된 값은 UE C++ 구조체 기본값 fallback을 사용한다. DeliveryBotSetup tuning은 LLM이 직접 생성하지 않고 UE 계약 문서의 default catalog와 deterministic variation policy를 기준으로 adapter가 결정한다.

EvaluationReport 관련 파일은 다른 담당자 범위이므로 이번 migration plan에서는 향후 연동 지점으로만 표시한다.

## 8. 호환성 전략

* 기존 EpisodeSpec은 당장 제거하지 않고 legacy로 유지한다.
* 신규 `responseFormat`:
  * `setup_pair`
* `responseFormat=setup_pair`는 EpisodeSetup + DeliveryBotSetup pair를 반환한다.
* `responseFormat=both`는 기존 의미대로 worldConfig + EpisodeSpec 디버그 응답을 유지한다.
* 기존 `responseFormat=episode_spec`은 deprecated 예정으로 문서화한다.

## 9. 구현 순서

1. UE 계약 문서 반영
2. Scenario/Episode 용어 정리
3. 기존 EpisodeSpec 문서 legacy 표시
4. EpisodeSetup / DeliveryBotSetup 모델 추가
5. validator 추가
6. WorldConfig -> EpisodeSetup / DeliveryBotSetup adapter 추가
7. `responseFormat=setup_pair` 추가
8. generationTrace 확장
9. UE 단일 pair handoff smoke 확인
10. script/export 중심 RunQueue package 생성
11. 사용자용 `POST /api/v1/scenarios/generate` 추가
12. EvaluationReport 기반 결과 분석은 다른 담당자 범위로 분리

현재 다음 실제 액션은 UE 팀이 local candidate의 EpisodeSetup + DeliveryBotSetup pair와 RunQueue package를 Runner/Compiler에 넣어 계약 호환성을 확인하는 것이다. 사용자용 `POST /api/v1/scenarios/generate`는 이미 wrapper 없는 RunQueue JSON을 반환한다. 단, `/api/v1/ue5/world-config/handoff`의 `responseFormat=run_queue` 옵션은 아직 public handoff 옵션으로 구현하지 않았다.
