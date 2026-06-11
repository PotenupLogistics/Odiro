> Archived document.
> This document is kept for historical reference and is not the current UE contract.
> Current UE contracts live under `docs/ue_contracts/`.

# UE5 Handoff Acceptance Checklist

* [ ] AI Backend 서버 실행 가능
* [ ] `/health` 200 OK
* [ ] `/api/v1/ue5/world-config/handoff` 호출 가능
* [ ] `response.success=true`
* [ ] `worldConfig.schemaVersion` 존재
* [ ] `worldConfig.map.lengthCm` 존재
* [ ] `worldConfig.map.sidewalkWidthCm` 존재
* [ ] `worldConfig.robot.spawn` 존재
* [ ] `worldConfig.robot.goal` 존재
* [ ] `obstacles[]` 생성 가능
* [ ] `pedestrians[]` 생성 가능
* [ ] `behavior=Crossing` 처리 가능
* [ ] `runtime.maxDurationSec` 적용 가능
* [ ] `success=false` 응답은 UE5가 실행하지 않음
* [ ] `worldConfig=null` 응답은 UE5가 실행하지 않음
* [ ] UE5 로그에 `worldId`와 `scenarioId` 출력
# EpisodeSpec acceptance items

* [ ] `responseFormat=world_config` 기존 응답이 유지된다.
* [ ] `responseFormat=episode_spec`에서 `episodeSpec`이 반환된다.
* [ ] `responseFormat=both`에서 `worldConfig`와 `episodeSpec`이 함께 반환된다.
* [ ] `EpisodeSpec` 단위는 meter/degree이다.
* [ ] Kickboard는 임시 prop mapping warning을 포함한다.
* [ ] UE catalog에 `obstacle.kickboard` 추가 여부를 확인한다.
* [ ] `harness/reports/ue5_episode_spec_handoff_smoke.json`에서 `episodeSpecExists`, `episodeValidationPassed`, `ueCompilerReadiness`를 확인한다.
* [ ] `harness/reports/ue5_episode_spec_controlled_scenario_smoke.json`에서 scenario reflection 통과 여부를 확인한다.
* [ ] Kickboard semantic, blocking ratio, crossing pedestrian, path linkage가 모두 반영됐는지 확인한다.
* [ ] `responseFormat=episode_spec`를 기본 호출 형식으로 사용한다.
* [ ] `responseFormat=both`는 디버깅시에만 사용한다.
* [ ] `success=false` 응답의 `episodeSpec`은 UE 실행 대상으로 사용하지 않는다.
