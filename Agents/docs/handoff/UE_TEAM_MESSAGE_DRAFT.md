# UE Team Message Draft

AI Backend에서 legacy UE 계약 기반 EpisodeSetup + DeliveryBotSetup pair 생성 smoke가 성공했습니다.

이 문서는 과거 handoff 기록입니다. 현재 `/api/v1/scenarios/generate`는 `410 RUN_QUEUE_REMOVED` 안내만 반환합니다. 이전 wrapper 없는 RunQueue JSON 응답은 legacy tooling으로만 남습니다. 기존 `/api/v1/ue5/world-config/handoff?provider=openai&responseFormat=setup_pair` 및 `responseFormat=episode_spec` 경로는 FastAPI/OpenAPI에서 제거된 legacy handoff입니다.

로컬 candidate path:

* `data/fine_tuning_candidates/20260604T040540Z_UE-HANDOFF-SETUP-PAIR-001/episode_setup.final.json`
* `data/fine_tuning_candidates/20260604T040540Z_UE-HANDOFF-SETUP-PAIR-001/delivery_bot_setup.final.json`

위 파일들은 로컬 테스트 산출물이며 git commit 대상이 아닙니다. UE 팀 전달 시에는 파일 내용만 복사하거나 별도 공유하면 됩니다.

UE 쪽에서 단일 pair 실행 테스트를 요청드립니다.

확인 요청 항목:

* EpisodeSetup compile
* DeliveryBotSetup compile
* actor spawn
* route injection
* obstacle spawn
* `obstacle.kickboard` prop ID 존재 여부와 현재 임시 mapping인 `obstacle.road_barrier_01` 교체 가능 여부
* lidar/drive/path_follow 반영
* robot이 `[0,0]`에서 `[8,0]`으로 이동하는지 확인
* obstacle `[4,0]` 생성 확인
* `stop_distance_m=1.2`, `slow_down_distance_m=3.5` 반영 확인

EvaluationReport 분석 구현은 현재 AI Backend 전달 범위 밖이며, UE 단일 pair 실행 확인 이후 별도 단계로 진행합니다.
