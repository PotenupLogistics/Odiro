# UE Setup Pair Handoff Package

## 전달 대상

* EpisodeSetup JSON
* DeliveryBotSetup JSON
* EpisodeRunQueue JSON

## local candidate path

* `data/fine_tuning_candidates/20260604T040540Z_UE-HANDOFF-SETUP-PAIR-001/episode_setup.final.json`
* `data/fine_tuning_candidates/20260604T040540Z_UE-HANDOFF-SETUP-PAIR-001/delivery_bot_setup.final.json`

주의:

* 이 파일들은 로컬 테스트 산출물이며 git commit 대상이 아님.
* UE 팀 전달 시 파일 내용만 복사해서 전달하거나 별도 공유한다.
* Fine-tuning candidate archive는 `.gitignore` 대상이며 repository에 포함하지 않는다.

## UE 확인 요청

* EpisodeSetup JSON이 UE compiler에서 통과하는지 확인.
* DeliveryBotSetup JSON이 FDeliveryBotSetupInfo로 컴파일되는지 확인.
* EpisodeSetup + DeliveryBotSetup pair 실행 시 robot이 `[0,0]`에서 `[8,0]`으로 이동하는지 확인.
* obstacle `[4,0]`이 생성되는지 확인.
* `stop_distance_m=1.2`, `slow_down_distance_m=3.5`가 반영되는지 확인.
* blocked/obstacle 상황에서 SlowDown/Stop/Repath 로직과 충돌하지 않는지 확인.

## 전달 메모

`responseFormat=setup_pair`는 최신 UE 계약 기준의 EpisodeSetup + DeliveryBotSetup pair만 반환한다. 기존 `responseFormat=episode_spec`은 legacy 경로로 유지되며, UE 단일 pair 실행 검증이 끝날 때까지 제거하지 않는다.

RunQueue package는 API 응답 규격을 바꾸지 않고 `scripts/export_ue5_run_queue_package.py`로 local export한다. 기본 episode count는 5이며, 산출물은 `data/run_queue_exports/<timestamp>_<requestId>/Json/Input/` 아래에 저장한다.

사용자용 `POST /api/v1/scenarios/generate`는 같은 RunQueue 계약을 API 응답으로 직접 반환한다. 입력은 자연어 `prompt`를 필수로 받고 선택적으로 `episode_count`를 허용한다. 사용자가 EpisodeSetup / DeliveryBotSetup / RunQueue JSON을 직접 작성하지 않는다. `episode_count`가 없으면 `SCENARIO_EPISODE_DEFAULT_COUNT`를 사용한다.

UE용 `EpisodeRunQueue_<scenario>.json`은 공식 RunQueue 계약 그대로 아래 필드만 포함한다.

* `schema`
* `version`
* `runs[].pair_id`
* `runs[].episode_setup`
* `runs[].delivery_bot_setup`

RunQueue JSON에는 `success`, `responseFormat`, `diagnostics`, `setupPairs`, `episodeSetup`, `deliveryBotSetup`, `validation`, `trace`를 넣지 않는다. 검증 결과와 variation trace는 같은 export root의 `validation_summary.json`, `trace_summary.json`, `export_summary.json`에 분리 저장한다.

EpisodeSetup / DeliveryBotSetup export는 null-free 정책을 따른다. optional field 값이 없으면 `null`을 출력하지 않고 필드 자체를 생략한다. DeliveryBotSetup에서 생략된 값은 UE C++ 구조체 기본값 fallback을 사용한다. episode별 tuning variation은 UE 문서 default catalog와 deterministic policy를 기준으로 하며 LLM 임의 수치를 사용하지 않는다.

같은 export target에 기존 `Json/Input/*.json`이 있으면 삭제하지 않고 `data/run_queue_exports/_backup/` 아래로 이동한 뒤 새 package를 생성한다.
