# UE Team Message Draft

AI Backend에서 OpenAI-first EpisodeSpec handoff 준비가 완료됐습니다.

UE 쪽 기본 호출은 `provider=openai&responseFormat=episode_spec`를 권장합니다. 디버깅이 필요하면 `responseFormat=both`로 `WorldConfig`와 `EpisodeSpec`을 같이 볼 수 있습니다. Ollama는 fallback provider로 유지합니다.

최근 OpenAI-first EpisodeSpec handoff smoke에서 `providerUsed=openai`, `fallbackUsed=false`로 Kickboard obstacle, `blocking_ratio`, Crossing pedestrian, path linkage가 모두 반영되는 것을 확인했습니다.

현재 Kickboard는 `obstacle.kickboard`가 catalog에 없어 `obstacle.road_barrier_01`로 임시 매핑하고, `properties.semantic_type="Kickboard"`로 원래 의미를 보존합니다.

확인 부탁드립니다:

* `obstacle.kickboard` catalog 추가 가능 여부
* `obstacle.road_barrier_01` 임시 매핑 허용 여부
* `EpisodeSandbox`에서 UEpisodeCompiler로 EpisodeSpec compile 가능 여부
* actor spawn / path movement / route injection 확인
* `paths[].role=pedestrian_crossing` 처리 방식
* `actors.pedestrians[].properties.semantic_behavior="Crossing"` 처리 방식
* `blocking_ratio`를 debug/log/metric에 사용할지 여부

먼저 볼 문서:

* `UE_INTEGRATION_HANDOFF_INDEX.md`
* `UE_TEAM_HANDOFF_PACKAGE.md`
* `UE5_ENDPOINT_USAGE_FOR_UE_TEAM.md`
* `UE5_EPISODE_SPEC_CONTROLLED_SMOKE_RESULT.md`
* `OPENAI_FIRST_HANDOFF_RESULT.md`
