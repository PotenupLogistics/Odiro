> Archived document.
> This document is kept for historical reference and is not the current handoff status.
> Current handoff status is summarized in `docs/handoff/HANDOFF_RELEASE_NOTES.md`.

# Setup Pair Handoff Result

## 1. Summary

* `responseFormat=setup_pair` live smoke 성공.
* 자연어 prompt와 environmentSampling 기반으로 EpisodeSetup + DeliveryBotSetup pair 생성 성공.
* EpisodeSetup / DeliveryBotSetup validation 통과.
* 기존 `episode_spec` 경로는 legacy handoff로 유지됨.

## 2. Verified API

* `POST /api/v1/ue5/world-config/handoff?provider=openai&responseFormat=setup_pair`
* HTTP status: `200`
* `success=true`
* `providerUsed=openai`
* `effectiveResponseFormat=setup_pair`
* `episodeSetupValidationPassed=true`
* `deliveryBotSetupValidationPassed=true`
* `episodeSpec=null`

## 3. Verified EpisodeSetup

* schema: `episode_actor_spawn_mvp`
* scenarioId: `scenario_001`
* runTimeLimitS: `60.0`
* sidewalkWidthM: `1.2`
* robotStartXY: `[0.0, 0.0]`
* robotGoalXY: `[8.0, 0.0]`
* staticObstacleCount: `1`
* obstacleXY: `[4.0, 0.0]`
* pedestriansEmpty: `true`
* pathsEmpty: `true`
* forbidden fields absent:
  * `units`
  * `transform`
  * `location_m`
  * `rotation_deg`
  * `scale`

## 4. Verified DeliveryBotSetup

* schema: `delivery_bot_setup`
* `drive` exists.
* `path_follow` exists.
* `lidar` exists.
* stopDistanceM: `1.2`
* slowDownDistanceM: `3.5`
* forbidden fields absent:
  * `run`
  * `actors`
  * `route`
  * `location`
  * `transform`
  * `xy_m`
  * `yaw_deg`

## 5. Trace / candidate archive

* `setupPairTraceExists=true`
* Fine-tuning candidate saved under local ignored path:
  * `data/fine_tuning_candidates/20260604T040540Z_UE-HANDOFF-SETUP-PAIR-001`
* Full JSON is not stored in the harness report.
* Full JSON is not committed.
* The candidate archive is local-only and covered by `.gitignore`.

## 6. Next UE action

UE 팀에서 EpisodeSetup + DeliveryBotSetup pair를 실제 Runner/Compiler에 넣어 단일 실행 테스트를 진행한다.

확인할 항목:

* EpisodeSetup compile
* DeliveryBotSetup compile
* robot spawn
* route injection
* static obstacle spawn
* lidar stop/slowDown distance 반영
* 에피소드 실행 시작 여부
