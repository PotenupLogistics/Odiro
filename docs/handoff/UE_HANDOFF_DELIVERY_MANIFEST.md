# UE Handoff Delivery Manifest

## 1. 목적

이 문서는 UE 팀에 전달해야 할 문서와 확인해야 할 파일 목록을 정리합니다.

## 2. 전달 대상 문서

* `README.md`
* `docs/handoff/UE_INTEGRATION_HANDOFF_INDEX.md`
* `docs/archive/previous_episode_spec/UE_TEAM_HANDOFF_PACKAGE.md`
* `docs/handoff/UE5_ENDPOINT_USAGE_FOR_UE_TEAM.md`
* `docs/archive/previous_episode_spec/UE5_EPISODE_SPEC_HANDOFF_SUMMARY.md`
* `docs/handoff/HANDOFF_RELEASE_NOTES.md`
* `docs/archive/previous_episode_spec/UE5_EPISODE_SPEC_ADAPTER.md`
* `docs/archive/previous_episode_spec/UE5_EPISODE_SPEC_SCENARIO_REFLECTION.md`
* `docs/archive/previous_episode_spec/UE5_EPISODE_SPEC_CONTROLLED_SMOKE_RESULT.md`
* `docs/archive/previous_episode_spec/UE5_CONTROLLED_INTEGRATION_TEST_PLAN.md`
* `docs/archive/previous_episode_spec/UE5_HANDOFF_ACCEPTANCE_CHECKLIST.md`
* `docs/handoff/UE_AI_INTEGRATION_ISSUES.md`

## 3. UE 기본 권장 endpoint

```text
POST /api/v1/ue5/world-config/handoff?provider=openai&responseFormat=episode_spec
```

## 4. 디버그 endpoint option

```text
responseFormat=both
```

## 5. Export CLI

```powershell
uv run python scripts/export_ue5_handoff_payload.py --prompt "..." --provider openai --format episode_spec
```

`--out`을 지정하지 않으면 export CLI는 파일을 쓰지 않습니다.

## 6. 현재 smoke 결과

상세 과거 smoke 결과 문서는 `docs/archive/deprecated/` 아래에 보관하고, 현재 공유 기준 요약은 `docs/handoff/HANDOFF_RELEASE_NOTES.md`를 사용합니다.

* `handoffSuccess=true`
* `providerUsed=openai`
* `fallbackUsed=false`
* `episodeValidationPassed=true`
* `episodeScenarioReflectionPassed=true`
* `ueCompilerReadiness=true`
* environmentSampling handoff:
  * `sidewalkWidthCm=120`
  * `obstacleBlockingRatio=0.6`
  * `timeLimitSec=60`
  * `sidewalkWidthM=1.2`
  * `run.time_limit_s=60.0`

## 7. UE 팀 확인 요청

* `obstacle.kickboard`를 UE prop catalog에 추가할 수 있는지 확인
* 임시 `obstacle.road_barrier_01` mapping을 허용할 수 있는지 확인
* `EpisodeSandbox`에서 `UEpisodeCompiler` compile 동작 확인
* `BP_DeliveryBot_GridBoundsActor` 존재 여부 확인
* static obstacle, pedestrian, route injection 동작 확인
* environmentSampling 기반 obstacle/width/time limit가 UE actor spawn과 route injection에 반영되는지 확인

이 handoff는 프로젝트 내부 시뮬레이션 검증용이며 실제 UE actor spawn은 UE 팀 확인이 필요합니다.
